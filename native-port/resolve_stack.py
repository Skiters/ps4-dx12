import bisect
import pathlib
import re
import sys
import argparse

parser = argparse.ArgumentParser(description='Resolve a one-shot stack snapshot using its matching linker map')
parser.add_argument('snapshot', type=pathlib.Path)
parser.add_argument('map', nargs='?', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parent.parent / 'build-full-native-dx12/native-core.map')
parser.add_argument('--thread', type=int)
args = parser.parse_args()

map_path = args.map
mapping = map_path.read_text(errors='replace')
base = int(re.search(r'Preferred load address is ([0-9a-f]+)', mapping)[1], 16)
symbols = []
for line in mapping.splitlines():
    match = re.match(r'\s+[0-9a-f]+:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{16})\s', line, re.I)
    if match:
        symbols.append((int(match[2], 16), match[1]))
symbols.sort()
addresses = [entry[0] for entry in symbols]
parts = re.split(r'(?=DEBUG THREAD)', args.snapshot.read_text(errors='replace'))
parts.sort(key=lambda s: int(re.search(r'CPU=(\d+)', s)[1]) if 'CPU=' in s else -1, reverse=True)
if args.thread is not None:
    parts = [part for part in parts if part.startswith(f'DEBUG THREAD {args.thread} ')]
for part in parts[:6]:
    print(part.splitlines()[0])
    for match in re.finditer(r'shadPS4-DX12-Core.exe \+ 0x([0-9a-f]+)', part):
        address = base + int(match[1], 16)
        index = bisect.bisect_right(addresses, address) - 1
        if index >= 0:
            print(' ', match[1], symbols[index][1], '+', hex(address-symbols[index][0]))
