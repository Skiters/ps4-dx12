"""Query the actual D3D12/XeSS runtime without changing the emulator or drivers.

This verifies context initialization, not XMX utilization or game integration.
ABI definitions follow Intel's MIT-licensed inc/xess/xess{,_d3d12}.h.
"""
import argparse
import ctypes as c
import json
from pathlib import Path
import uuid


class Version(c.Structure):
    _fields_ = [(name, c.c_uint16) for name in ("major", "minor", "patch", "reserved")]

    def text(self):
        return f"{self.major}.{self.minor}.{self.patch}"


class Resolution(c.Structure):
    _fields_ = [("x", c.c_uint32), ("y", c.c_uint32)]


class Init(c.Structure):
    _fields_ = [("output", Resolution), ("quality", c.c_int), ("flags", c.c_uint32),
                ("creation_node", c.c_uint32), ("visible_node", c.c_uint32),
                ("buffer_heap", c.c_void_p), ("buffer_offset", c.c_uint64),
                ("texture_heap", c.c_void_p), ("texture_offset", c.c_uint64),
                ("pipeline_library", c.c_void_p)]


class AdapterDesc(c.Structure):
    _fields_ = [("name", c.c_wchar * 128), ("vendor", c.c_uint32),
                ("device", c.c_uint32), ("subsystem", c.c_uint32), ("revision", c.c_uint32),
                ("video_memory", c.c_size_t), ("system_memory", c.c_size_t),
                ("shared_memory", c.c_size_t), ("luid_low", c.c_uint32),
                ("luid_high", c.c_int32), ("flags", c.c_uint32)]


def method(obj, slot, result, *args):
    table = c.cast(obj, c.POINTER(c.POINTER(c.c_void_p))).contents
    return c.WINFUNCTYPE(result, c.c_void_p, *args)(table[slot])


def release_com(obj):
    if obj:
        method(obj, 2, c.c_uint32)(obj)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--intel-extension", type=Path, help="Preload an existing signed Intel extension DLL for a separate diagnostic comparison")
    args = parser.parse_args()
    report = {"scope": "native D3D12 capability and XeSS context initialization only",
              "gameIntegration": False, "xmxExecutionVerified": False}
    messages = []
    log_type = c.CFUNCTYPE(None, c.c_char_p, c.c_int)
    @log_type
    def on_log(message, level):
        messages.append({"level": level, "message": message.decode("utf-8", errors="replace")})
    device, context = c.c_void_p(), c.c_void_p()
    factory, adapter = c.c_void_p(), c.c_void_p()
    release = destroy = None
    try:
        dxgi = c.WinDLL("dxgi.dll")
        factory_guid = (c.c_byte * 16).from_buffer_copy(uuid.UUID("770aae78-f26f-4dba-a829-253c83d1b387").bytes_le)
        make_factory = dxgi.CreateDXGIFactory1
        make_factory.argtypes = [c.c_void_p, c.POINTER(c.c_void_p)]
        make_factory.restype = c.c_long
        if make_factory(c.byref(factory_guid), c.byref(factory)) < 0:
            raise RuntimeError("Cannot enumerate hardware adapters")
        enum_adapter = method(factory, 12, c.c_long, c.c_uint32, c.POINTER(c.c_void_p))
        index = 0
        while True:
            hr = enum_adapter(factory, index, c.byref(adapter))
            if hr < 0:
                raise RuntimeError("No Intel hardware adapter found")
            desc = AdapterDesc()
            hr = method(adapter, 10, c.c_long, c.POINTER(AdapterDesc))(adapter, c.byref(desc))
            if hr >= 0 and desc.vendor == 0x8086 and not desc.flags & 2:
                report["adapter"] = desc.name
                report["pciId"] = f"{desc.vendor:04x}:{desc.device:04x}"
                break
            release_com(adapter)
            adapter = c.c_void_p()
            index += 1
        d3d12 = c.WinDLL("d3d12.dll")
        create = d3d12.D3D12CreateDevice
        create.argtypes = [c.c_void_p, c.c_uint32, c.c_void_p, c.POINTER(c.c_void_p)]
        create.restype = c.c_long
        guid = (c.c_byte * 16).from_buffer_copy(uuid.UUID("189819f1-1db6-4b57-be54-1821339b85f7").bytes_le)
        hr = create(adapter, 0xc000, c.byref(guid), c.byref(device))
        if hr < 0:
            raise RuntimeError(f"D3D12CreateDevice HRESULT={hr & 0xffffffff:08x}")
        vtable = c.cast(device, c.POINTER(c.POINTER(c.c_void_p))).contents
        release = c.WINFUNCTYPE(c.c_uint32, c.c_void_p)(vtable[2])
        check = c.WINFUNCTYPE(c.c_long, c.c_void_p, c.c_int, c.c_void_p, c.c_uint32)(vtable[13])
        options2 = (c.c_uint32 * 2)()
        hr = check(device, 18, options2, c.sizeof(options2))
        report["depthBoundsQueryHresult"] = hr
        report["depthBoundsSupported"] = bool(options2[0]) if hr >= 0 else None
        report["shaderModelQueries"] = []
        for requested in (0x6a, 0x69, 0x68, 0x67, 0x66):
            model = c.c_uint32(requested)
            model_hr = check(device, 7, c.byref(model), c.sizeof(model))
            report["shaderModelQueries"].append({"requested": f"6.{requested - 0x60}",
                "hresult": f"{model_hr & 0xffffffff:08x}",
                "reported": f"{model.value >> 4}.{model.value & 15}" if model_hr >= 0 else None})
        extension = None
        if args.intel_extension:
            extension = c.WinDLL(str(args.intel_extension.resolve()))
            report["preloadedIntelExtension"] = str(args.intel_extension.resolve())
        runtime = c.CDLL(str(args.runtime.resolve()))

        def bind(name, *types):
            fn = getattr(runtime, name)
            fn.argtypes = list(types)
            fn.restype = c.c_int
            return fn

        version = Version()
        report["sdkVersionStatus"] = bind("xessGetVersion", c.POINTER(Version))(c.byref(version))
        report["sdkVersion"] = version.text()
        status = bind("xessD3D12CreateContext", c.c_void_p, c.POINTER(c.c_void_p))(device, c.byref(context))
        report["contextStatus"] = status
        if status < 0:
            raise RuntimeError(f"xessD3D12CreateContext={status}")
        destroy = bind("xessDestroyContext", c.c_void_p)
        report["loggingCallbackStatus"] = bind("xessSetLoggingCallback", c.c_void_p,
            c.c_int, log_type)(context, 0, on_log)
        intel = Version()
        report["intelRuntimeVersionStatus"] = bind("xessGetIntelXeFXVersion", c.c_void_p, c.POINTER(Version))(context, c.byref(intel))
        report["intelRuntimeVersion"] = intel.text()
        report["driverRecommendationStatus"] = bind("xessIsOptimalDriver", c.c_void_p)(context)
        output, optimal, minimum, maximum = Resolution(1920, 1200), Resolution(), Resolution(), Resolution()
        report["resolutionStatus"] = bind("xessGetOptimalInputResolution", c.c_void_p,
            c.POINTER(Resolution), c.c_int, c.POINTER(Resolution), c.POINTER(Resolution),
            c.POINTER(Resolution))(context, c.byref(output), 104, c.byref(optimal), c.byref(minimum), c.byref(maximum))
        report["optimalInput"] = [optimal.x, optimal.y]
        report["inputRange"] = [[minimum.x, minimum.y], [maximum.x, maximum.y]]
        params = Init(output=output, quality=104)
        report["initializationStatus"] = bind("xessD3D12Init", c.c_void_p, c.POINTER(Init))(context, c.byref(params))
        report["initializedIntelRuntimeVersionStatus"] = bind("xessGetIntelXeFXVersion", c.c_void_p, c.POINTER(Version))(context, c.byref(intel))
        report["initializedIntelRuntimeVersion"] = intel.text()
        # Record the actual loaded runtimes; an adapter name alone does not
        # establish that the Intel-specific/XMX implementation was selected.
        kernel = c.WinDLL("kernel32.dll")
        kernel.GetCurrentProcess.restype = c.c_void_p
        psapi = c.WinDLL("psapi.dll")
        enum = psapi.EnumProcessModules
        enum.argtypes = [c.c_void_p, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint32)]
        modules = (c.c_void_p * 2048)()
        needed = c.c_uint32()
        paths = []
        filename = psapi.GetModuleFileNameExW
        filename.argtypes = [c.c_void_p, c.c_void_p, c.c_wchar_p, c.c_uint32]
        process = kernel.GetCurrentProcess()
        if enum(process, modules, c.sizeof(modules), c.byref(needed)):
            for module in modules[:min(needed.value // c.sizeof(c.c_void_p), len(modules))]:
                path = c.create_unicode_buffer(32768)
                if filename(process, module, path, len(path)) and any(
                    part in path.value.lower() for part in ("xess", "xefx", "xell", "igd12", "igdext")):
                    paths.append(path.value)
        report["loadedRuntimeModules"] = paths
    except Exception as error:
        report["error"] = str(error)
    finally:
        if context and destroy:
            report["destroyStatus"] = destroy(context)
        if device and release:
            release(device)
        release_com(adapter)
        release_com(factory)
        report["runtimeMessages"] = messages
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return int("error" in report or report.get("initializationStatus", -1) < 0)


if __name__ == "__main__":
    raise SystemExit(main())
