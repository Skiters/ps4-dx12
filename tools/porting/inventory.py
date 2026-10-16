# SPDX-License-Identifier: GPL-2.0-or-later
"""Reproducible source inventory; syntactic matches never certify runtime parity."""
import argparse
import collections
import hashlib
import json
import pathlib
import re
import subprocess

from tree_sitter import Language, Parser
import tree_sitter_cpp

REVISION = "e0a466753a1aacbc8db8253be685bf7ab4d9daae"
PARSER = Parser(Language(tree_sitter_cpp.language()))
CPP = {".cpp", ".h", ".cc", ".hpp", ".c", ".mm", ".vert", ".frag", ".comp",
       ".geom", ".tesc", ".tese", ".glsl", ".hlsl", ".hlsli"}


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def parser_source(data):
    # Scope-exit syntax is a captured lambda after macro expansion. Keep byte
    # offsets identical so signatures, bodies and hashes still use raw source.
    return re.sub(rb"\bSCOPE_EXIT(?=\s*\{)", b"[]()      ", data)


def walk(node):
    yield node
    for child in node.children:
        yield from walk(child)


def function_name(node, data):
    declarator = node.child_by_field_name("declarator")
    if declarator is None:
        return None
    function = next((n for n in walk(declarator) if n.type == "function_declarator"), None)
    name = function.child_by_field_name("declarator") if function else None
    if name is None:
        return None
    scopes, parent = [], node.parent
    while parent is not None:
        if parent.type in {"namespace_definition", "class_specifier", "struct_specifier"}:
            scope = parent.child_by_field_name("name")
            if scope is not None:
                scopes.append(data[scope.start_byte:scope.end_byte].decode("utf-8", "replace"))
        parent = parent.parent
    identifier = data[name.start_byte:name.end_byte].decode("utf-8", "replace")
    # C++ recovery can include the calling-convention macro in the name.
    identifier = re.sub(r"^(?:PS4_SYSV_ABI|__cdecl|__stdcall)\s+", "", identifier)
    return "::".join(reversed(scopes)) + "::" + identifier


def source_behavior(body, data):
    markers = []
    for node in walk(body):
        if node.type != "string_literal":
            continue
        value = data[node.start_byte:node.end_byte].decode("utf-8", "replace")
        if re.search(r"STUBBED|not implemented|unimplemented|unsupported", value, re.I):
            markers.append({"line": node.start_point.row + 1, "text": value})
    result = {"kind": "requires_semantic_review", "markers": markers}
    if not markers or body.has_error:
        return result
    result["kind"] = "marked_partial_or_requires_review"
    statements = [node for node in body.named_children if node.type != "comment"]
    operations, returns = [], []
    for statement in statements:
        if statement.type == "return_statement":
            returns.append(statement)
            continue
        if statement.type != "expression_statement" or len(statement.named_children) != 1:
            return result
        call = statement.named_children[0]
        if call.type != "call_expression":
            return result
        function = call.child_by_field_name("function")
        name = data[function.start_byte:function.end_byte].decode("utf-8", "replace") if function else ""
        nested = [node for node in walk(call) if node.type == "call_expression" and node != call]
        if nested:
            return result
        if name not in {"LOG_ERROR", "LOG_WARNING", "LOG_INFO", "LOG_DEBUG", "LOG_TRACE"}:
            operations.append((name, call))
    if not operations and len(returns) == 1 and any("STUBBED" in marker["text"] for marker in markers):
        expression = returns[0].named_children
        if len(expression) == 1:
            value = data[expression[0].start_byte:expression[0].end_byte].decode("utf-8", "replace").strip()
            if re.fullmatch(r"(?:[-+]?\d+(?:[uUlL]*)|0[xX][0-9a-fA-F]+[uUlL]*|true|false|nullptr|(?:ORBIS|SCE)_[A-Z0-9_]+)", value):
                result.update(kind="explicit_stub_constant_response", return_expression=value)
    elif not returns and len(operations) == 1 and operations[0][0] in {"UNREACHABLE", "UNREACHABLE_MSG"}:
        value = data[operations[0][1].start_byte:operations[0][1].end_byte].decode("utf-8", "replace")
        if re.search(r"not implemented|unimplemented|unsupported", value, re.I):
            result["kind"] = "explicit_unsupported_operation"
    return result


def dispatch_context(node, data):
    parent, function, selector = node.parent, None, None
    while parent is not None:
        if selector is None and parent.type == "switch_statement":
            condition = parent.child_by_field_name("condition")
            if condition is not None:
                selector = data[condition.start_byte:condition.end_byte].decode("utf-8", "replace")
        if parent.type == "function_definition":
            function = function_name(parent, data)
            break
        parent = parent.parent
    return {"function_name": function, "selector": selector}


def reconcile_dispatch(original, native):
    def key(entry):
        normalize = lambda text: re.sub(r"\s+", " ", text or "").strip()
        return (entry["path"], entry["kind"], entry.get("function_name"),
                normalize(entry.get("selector")), normalize(entry["text"]))
    available = collections.defaultdict(list)
    for entry in native:
        available[key(entry)].append(entry)
    consumed = collections.Counter()
    for entry in original:
        identity = key(entry)
        matches, occurrence = available[identity], consumed[identity]
        consumed[identity] += 1
        if occurrence < len(matches):
            match = matches[occurrence]
            entry["native_mapping"] = {k: match.get(k) for k in ("path", "line", "kind", "function_name", "selector", "text")}
            entry["status"] = "same_dispatch_unverified"
        else:
            entry["native_mapping"] = None
            entry["status"] = "requires_dispatch_mapping_or_review"


def link_callers(symbols):
    by_name = collections.defaultdict(list)
    for key, symbol in symbols.items():
        if symbol.get("name"):
            by_name[symbol["name"]].append(key)
        symbol["callers"] = []
        symbol["unresolved_calls"] = []
    for key, symbol in symbols.items():
        scope = symbol["name"].rsplit("::", 1)[0] if symbol.get("name") else ""
        for call in symbol["calls"]:
            matches = by_name.get(call, []) or by_name.get(scope + "::" + call, [])
            local = [candidate for candidate in matches if symbols[candidate]["path"] == symbol["path"]]
            if len(local) == 1:
                matches = local
            if len(matches) == 1:
                symbols[matches[0]]["callers"].append(key)
            else:
                symbol["unresolved_calls"].append(call)


def link_dispatch_targets(dispatch, symbols):
    by_name = collections.defaultdict(list)
    for key, symbol in symbols.items():
        if symbol.get("name"):
            by_name[symbol["name"]].append(key)
    for entry in dispatch:
        target = entry.get("target_name")
        if not target:
            continue
        scope = (entry.get("function_name") or "").rsplit("::", 1)[0]
        matches = by_name.get(target, []) or by_name.get(scope + "::" + target, [])
        local = [key for key in matches if symbols[key]["path"] == entry["path"]]
        if local:
            matches = local
        entry["target_candidates"] = matches
        kinds = {symbols[key]["source_behavior"]["kind"] for key in matches}
        entry["target_source_kind"] = next(iter(kinds)) if len(kinds) == 1 else "requires_semantic_review"


def inventory(root):
    files, symbols, dispatch = {}, {}, []
    tracked = git(root, "ls-files", "--cached", "--others", "--exclude-standard", "-z").split("\0")
    for name in tracked:
        if not name or not (name.startswith("src/") or name.endswith(("CMakeLists.txt", ".cmake"))):
            continue
        path = root / name
        if not path.is_file():
            continue
        data = path.read_bytes().replace(b"\r\n", b"\n")
        record = {"sha256_lf": digest(data), "bytes": len(data), "functions": []}
        record["local_includes"] = re.findall(r'^\s*#include\s+"([^"]+)"',data.decode("utf-8","replace"),re.M)
        files[name] = record
        if path.suffix in {".vert", ".frag", ".comp", ".geom", ".tesc", ".tese", ".glsl", ".inc", ".hlsl", ".hlsli"}:
            for line, text in enumerate(data.decode("utf-8", "replace").splitlines(), 1):
                if re.search(r"IR_OPCODE|^\s*(?:void\s+main|layout\s*\(|#define|\[numthreads)", text):
                    dispatch.append({"path": name, "line": line, "kind": "shader_or_opcode", "text": text.strip()})
        if path.suffix not in CPP:
            continue
        analysis_data=parser_source(data)
        tree = PARSER.parse(analysis_data)
        if analysis_data!=data:
            record["parser_normalization"]="scope_exit_macro_as_equal_width_lambda"
            raw_tree=PARSER.parse(data)
            record["raw_parse_errors"]=sum(n.type=="ERROR" or n.is_missing for n in walk(raw_tree.root_node))
        record["parse_errors"] = sum(n.type == "ERROR" or n.is_missing for n in walk(tree.root_node))
        for node in walk(tree.root_node):
            if node.type == "function_definition":
                declarator = node.child_by_field_name("declarator")
                body = node.child_by_field_name("body")
                if declarator is None or body is None:
                    continue
                # GLSL interface blocks can be recovered as C++ function nodes.
                # A block named input_buf is storage, not a function definition.
                if not any(n.type == "function_declarator" for n in walk(declarator)):
                    continue
                signature = data[declarator.start_byte:declarator.end_byte].decode("utf-8", "replace")
                scopes = []
                parent = node.parent
                while parent is not None:
                    if parent.type in {"namespace_definition", "class_specifier", "struct_specifier"}:
                        scope = parent.child_by_field_name("name")
                        if scope is not None:
                            scopes.append(data[scope.start_byte:scope.end_byte].decode("utf-8", "replace"))
                    parent = parent.parent
                symbol = "::".join(reversed(scopes)) + "::" + re.sub(r"\s+", " ", signature)
                key = name + "::" + symbol
                # Identical declarations in alternate preprocessor branches remain separate.
                while key in symbols:
                    key += "#"
                calls = sorted({data[n.start_byte:n.end_byte].decode("utf-8", "replace")
                    for n in walk(body) if n.type == "call_expression"
                    for n in [n.child_by_field_name("function")] if n is not None})
                source = data[node.start_byte:node.end_byte]
                symbols[key] = {"path": name, "symbol": symbol, "name": function_name(node, data), "line": node.start_point.row + 1,
                    "end_line": node.end_point.row + 1, "sha256_lf": digest(source),
                    "calls": calls, "contract": None, "mapping": None, "final_verification": None,
                    "source_behavior": source_behavior(body, data),
                    "status": "requires_semantic_review"}
                record["functions"].append(key)
            elif node.type == "case_statement":
                value = node.child_by_field_name("value")
                dispatch.append({"path": name, "line": node.start_point.row + 1, "kind": "switch_case",
                    "text": data[value.start_byte:value.end_byte].decode("utf-8", "replace") if value else "default",
                    **dispatch_context(node, data)})
            elif node.type == "call_expression":
                function = node.child_by_field_name("function")
                callee = data[function.start_byte:function.end_byte].decode("utf-8", "replace") if function else ""
                if callee.startswith(("LIB_", "REGISTER_")):
                    arguments = node.child_by_field_name("arguments")
                    last = arguments.named_children[-1] if arguments is not None and arguments.named_children else None
                    target = data[last.start_byte:last.end_byte].decode("utf-8", "replace") if last is not None else ""
                    dispatch.append({"path": name, "line": node.start_point.row + 1, "kind": "registration",
                        "text": data[node.start_byte:node.end_byte].decode("utf-8", "replace"),
                        "target_name": target if re.fullmatch(r"(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*", target) else None,
                        **dispatch_context(node, data)})
    return files, symbols, dispatch


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upstream", type=pathlib.Path, required=True)
    ap.add_argument("--native", type=pathlib.Path, required=True)
    ap.add_argument("--output", type=pathlib.Path, required=True)
    ap.add_argument("--contracts",type=pathlib.Path)
    args = ap.parse_args()
    if git(args.upstream, "rev-parse", "HEAD") != REVISION:
        raise SystemExit("Reference revision mismatch")
    upstream_files, symbols, dispatch = inventory(args.upstream)
    native_files, native_symbols, native_dispatch = inventory(args.native)
    relocated=collections.defaultdict(list)
    by_path_name=collections.defaultdict(list)
    for native in native_symbols.values():
        relocated[(native["symbol"],native["sha256_lf"])].append(native)
        by_path_name[(native["path"],native["name"])].append(native)
    for key, symbol in symbols.items():
        native = native_symbols.get(key)
        if native is None:
            matches=relocated[(symbol["symbol"],symbol["sha256_lf"])]
            if len(matches)==1:native=matches[0]
        if native is None and symbol["name"]:
            matches=by_path_name[(symbol["path"],symbol["name"])]
            if len(matches)==1:
                native=matches[0]
                symbol["mapping_basis"]="unique_same_path_name_requires_signature_and_semantic_review"
        if native:
            symbol["mapping"] = {k: native[k] for k in ("path", "symbol", "line", "sha256_lf")}
            symbol["status"] = "same_source_unverified" if symbol["sha256_lf"] == native["sha256_lf"] else "changed_requires_review"
        else:
            symbol["status"] = "requires_mapping_or_adaptation"
    for path, record in upstream_files.items():
        native = native_files.get(path)
        record["native_source_state"] = "absent" if not native else "same" if record["sha256_lf"] == native["sha256_lf"] else "changed"
    contracts=json.loads(args.contracts.read_text(encoding="utf-8")) if args.contracts else {}
    if contracts and contracts.get("upstream_revision")!=REVISION:raise SystemExit("Contract revision mismatch")
    group_ids=set()
    for group in contracts.get("groups",[]):
        if group["id"] in group_ids:raise SystemExit("Duplicate adaptation group: "+group["id"])
        group_ids.add(group["id"])
        for field,root in (("upstream_paths",args.upstream),("native_paths",args.native)):
            for path in group.get(field,[]):
                if not (root/path).is_file():raise SystemExit(f"Missing {field} in {group['id']}: {path}")
    upstream_by_path_name=collections.defaultdict(list)
    for symbol in symbols.values():
        upstream_by_path_name[(symbol["path"],symbol["name"])].append(symbol)
    for group in contracts.get("groups",[]):
        for mapping in group.get("function_mappings",[]):
            endpoints=[]
            for side,index,paths in (("upstream",upstream_by_path_name,"upstream_paths"),
                                     ("native",by_path_name,"native_paths")):
                path=mapping.get(side+"_path")
                name=mapping.get(side+"_name")
                if not path or not name or path not in group.get(paths,[]):
                    raise SystemExit(f"Invalid {side} mapping endpoint in {group['id']}: {path}::{name}")
                candidates=index[(path,name)]
                signature=mapping.get(side+"_symbol")
                if signature is not None:
                    candidates=[entry for entry in candidates if entry["symbol"]==signature]
                if len(candidates)!=1:
                    raise SystemExit(f"Nonunique {side} mapping endpoint in {group['id']}: {path}::{name} ({len(candidates)} definitions)")
                endpoints.append(candidates[0])
            symbol,native=endpoints
            selected=group.get("upstream_symbols",[])
            if selected and not any(re.search(pattern,symbol["symbol"]) for pattern in selected):
                raise SystemExit(f"Declared mapping outside symbol selection in {group['id']}: {symbol['symbol']}")
            target={k:native[k] for k in ("path","symbol","line","sha256_lf")}
            if symbol["mapping"] is not None and symbol["mapping"]!=target:
                raise SystemExit(f"Conflicting function mapping in {group['id']}: {symbol['symbol']}")
            symbol["mapping"]=target
            symbol["mapping_basis"]="declared_group_mapping_requires_signature_and_semantic_review"
            symbol.setdefault("declared_mapping_groups",[]).append(group["id"])
            symbol["status"]="same_source_unverified" if symbol["sha256_lf"]==native["sha256_lf"] else "changed_requires_review"
    for symbol in symbols.values():
        symbol["contract_associations"]=[]
    for group in contracts.get("groups",[]):
        for symbol in symbols.values():
            selected=group.get("upstream_symbols",[])
            if symbol["path"] in group.get("upstream_paths",[]) and (
                not selected or any(re.search(pattern,symbol["symbol"]) for pattern in selected)):
                symbol["contract"]=group["contract"]
                symbol["adaptation_group"]=group["id"]
                symbol["final_verification"]=group.get("final_verification")
                symbol["contract_associations"].append({"group":group["id"],"contract":group["contract"],
                    "native_paths":group.get("native_paths",[]),"final_verification":group.get("final_verification"),
                    "implementation":group.get("implementation"),
                    "behavior_verification":group.get("behavior_verification"),
                    "prepared_checks":group.get("prepared_checks",[])})
    link_callers(symbols)
    link_dispatch_targets(dispatch,symbols)
    reconcile_dispatch(dispatch,native_dispatch)
    result = {"upstream_revision": REVISION, "native_revision": git(args.native, "rev-parse", "HEAD"),
        "native_dirty":bool(git(args.native,"status","--porcelain")),
        "submodules": git(args.upstream, "submodule", "status", "--recursive").splitlines(),
        "scope": "All tracked src files and build definitions. Unique static caller links and function-scoped dispatch text are reconciled. Explicit upstream limitation strings and narrowly recognised log-only constant stubs/unreachable unsupported bodies are source evidence, not equivalence verification. Registration targets retain conservative source candidates. Unresolved C++ overload/member/dynamic calls, declarations/macros/generated dependencies and all semantic equivalence still require review.",
        "files": upstream_files, "functions": symbols, "dispatch": dispatch,
        "native_dispatch": native_dispatch,"adaptation_groups":contracts.get("groups",[]),
        "native_files":native_files,
        "summary": {"files": len(upstream_files), "functions": len(symbols), "dispatch_entries": len(dispatch),
            "function_states": dict(collections.Counter(s["status"] for s in symbols.values())),
            "dispatch_states":dict(collections.Counter(entry["status"] for entry in dispatch)),
            "upstream_source_behaviors":dict(collections.Counter(s["source_behavior"]["kind"] for s in symbols.values())),
            "registration_target_source_behaviors":dict(collections.Counter(entry["target_source_kind"] for entry in dispatch if "target_source_kind" in entry)),
            "static_caller_edges":sum(len(s["callers"]) for s in symbols.values()),
            "unresolved_call_names":sum(len(s["unresolved_calls"]) for s in symbols.values()),
            "contracted_functions":sum(s["contract"] is not None for s in symbols.values()),
            "declared_function_mappings":sum("declared_mapping_groups" in s for s in symbols.values()),
            "implementation_states":dict(collections.Counter(
                group.get("implementation",{}).get("state","not_recorded")
                for group in contracts.get("groups",[]))),
            "known_production_gaps":len(contracts.get("known_production_gaps",{})),
            "closed_semantic_contracts":sum(
                group.get("behavior_verification",{}).get("state")=="verified"
                for group in contracts.get("groups",[]))}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result["summary"]))


if __name__ == "__main__":
    main()
