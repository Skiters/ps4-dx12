#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import hashlib
import json
import pathlib
import re
import struct
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--executable',type=pathlib.Path,required=True)
parser.add_argument('--map',type=pathlib.Path,required=True)
parser.add_argument('--pdb',type=pathlib.Path,required=True)
parser.add_argument('--source',type=pathlib.Path,required=True)
parser.add_argument('--layer-directory',type=pathlib.Path,required=True)
parser.add_argument('--output',type=pathlib.Path,required=True)
args=parser.parse_args()
revision=subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip()
if revision!='e0a466753a1aacbc8db8253be685bf7ab4d9daae':
    raise RuntimeError('Reference source identity differs from the pinned original')
if subprocess.check_output(['git','-C',str(args.source),'status','--porcelain'],text=True).strip():
    raise RuntimeError('The immutable reference source has modifications')
raw=args.executable.read_bytes()
if raw[:2]!=b'MZ':raise RuntimeError('Reference executable is not PE')
pe=struct.unpack_from('<I',raw,0x3c)[0]
if raw[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',raw,pe+4)[0]!=0x8664:
    raise RuntimeError('Reference executable is not x64 PE')
optional=pe+24
if struct.unpack_from('<H',raw,optional)[0]!=0x20b:raise RuntimeError('Reference executable is not PE32+')
base=struct.unpack_from('<Q',raw,optional+24)[0]
image_size=struct.unpack_from('<I',raw,optional+56)[0]
sections=struct.unpack_from('<H',raw,pe+6)[0]
section_table=optional+struct.unpack_from('<H',raw,pe+20)[0]
def offset(rva,size):
    for index in range(sections):
        section=section_table+index*40
        virtual_size,virtual_address,raw_size,raw_offset=struct.unpack_from('<IIII',raw,section+8)
        if virtual_address<=rva and rva+size<=virtual_address+raw_size:
            at=raw_offset+rva-virtual_address
            if at+size>len(raw):raise RuntimeError('Truncated PE section')
            return at
    raise RuntimeError('PE RVA does not name complete file-backed bytes')
symbol=re.findall(r'^\s+\w{4}:[0-9a-fA-F]+\s+(\?Flip@VideoOutDriver@VideoOut@Libraries@@\S+)\s+([0-9a-fA-F]{16})\s+(?:f\s+)?\S+\.obj',args.map.read_text(),re.M)
if len(symbol)!=1:raise RuntimeError(f'Expected one original VideoOutDriver::Flip symbol, got {len(symbol)}')
name,address=symbol[0];rva=int(address,16)-base
if not 0<rva<image_size:raise RuntimeError('Reference Flip symbol is outside the executable image')
exception_rva,exception_size=struct.unpack_from('<II',raw,optional+112+3*8)
if not exception_rva or exception_size%12:raise RuntimeError('Reference has no complete x64 unwind directory')
exceptions=offset(exception_rva,exception_size)
ranges=[]
for entry in range(exceptions,exceptions+exception_size,12):
    begin,end,unwind=struct.unpack_from('<III',raw,entry)
    if begin==rva and begin<end<=image_size:
        offset(begin,end-begin);offset(unwind,4);ranges.append((begin,end))
if len(ranges)!=1:raise RuntimeError('Flip public symbol does not have one exact x64 runtime-function range')
begin,end=ranges[0]
flip=raw[offset(begin,end-begin):offset(begin,end-begin)+end-begin]
reviewed_flip_sha256='ba1004021a77999a9458cc9d282c350310e185cb476fb39a08c3a38bf2510667'
if hashlib.sha256(flip).hexdigest()!=reviewed_flip_sha256:
    raise RuntimeError('Flip machine code differs from the reviewed Request-register/callsite proof')
if flip[79:82]!=bytes.fromhex('4889d3') or flip[142:152]!=bytes.fromhex('488b134531c041b101e8') or flip[248:251]!=bytes.fromhex('8b4318'):
    raise RuntimeError('Reviewed Flip does not preserve Request in RBX through its Present call')
present_rva=begin+156+struct.unpack_from('<i',flip,152)[0]
present=re.findall(r'^\s+\w{4}:[0-9a-fA-F]+\s+(\?Present@Presenter@Vulkan@@QEAAXPEAUFrame@2@_N1@Z)\s+([0-9a-fA-F]{16})\s+(?:f\s+)?\S+\.obj',args.map.read_text(),re.M)
if len(present)!=1 or int(present[0][1],16)-base!=present_rva:
    raise RuntimeError('Reviewed Flip callsite does not call the exact original Presenter::Present')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
record={'referenceRevision':revision,'symbol':name,'flipRva':begin,'flipSize':end-begin,
        'flipPresentReturnRva':begin+156,'requestRegister':'rbx','requestIndexOffset':24,
        'flipMachineCodeSha256':reviewed_flip_sha256,'presentSymbol':present[0][0],'presentRva':present_rva,
        'executableSha256':digest(args.executable),'mapSha256':digest(args.map),'pdbSha256':digest(args.pdb),
        'mapFile':args.map.name,'pdbFile':args.pdb.name,
        'rangeEvidence':'Exact public Flip symbol and matching PE x64 RUNTIME_FUNCTION Begin/End; no neighboring-symbol size inference',
        'layerName':'VK_LAYER_CODEX_guest_completion',
        'layerManifestFile':'reference-completion/completion-layer.json',
        'layerManifestSha256':digest(args.layer_directory/'completion-layer.json'),
        'layerLibraryFile':'reference-completion/guest-completion-layer.dll',
        'layerLibrarySha256':digest(args.layer_directory/'guest-completion-layer.dll'),
        'layerSourceHashes':{name:digest(pathlib.Path(__file__).parent/name) for name in ('completion_layer.cpp','completion_layer.def','CMakeLists.txt','completion-layer.json')},
        'runtimeVerification':'not_run'}
record['layerSourceHashes']['src/common/gpu_identity.h']=digest(pathlib.Path(__file__).parents[2]/'src/common/gpu_identity.h')
args.output.write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({key:record[key] for key in ('referenceRevision','flipRva','flipSize','executableSha256','runtimeVerification')}))
