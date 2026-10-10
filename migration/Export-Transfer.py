"""Export native project code/handoff; no games, profiles or development SDKs."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import zipfile
from datetime import datetime

fork = Path(__file__).resolve().parents[1]
workspace = fork.parents[1]
destination = Path(r'F:\PS4-Desktop-Transfer-20261009')
destination.mkdir(parents=True, exist_ok=True)
lock = json.loads((fork / 'migration/dependency-lock.json').read_text(encoding='utf-8'))
third_party_paths = {entry['path'] for entry in lock['submodules']}
binary_extensions = {'.exe', '.dll', '.pdb', '.obj', '.lib', '.zip', '.7z', '.spv',
                     '.dxil', '.bin', '.pkg', '.self', '.elf', '.pyc'}

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4194304), b''):
            h.update(block)
    return h.hexdigest()

def exclude_directory(path):
    relative = path.relative_to(fork)
    name = path.name
    if name in {'.git', '__pycache__', 'node_modules'}:
        return True
    if relative.as_posix() in third_party_paths:
        return True
    if len(relative.parts) == 1 and name.startswith('build'):
        return True
    if relative.parts[0] == 'desktop' and (name in {'bin', 'obj', 'packages', 'payload'}
                                         or name.startswith(('publish', 'installer-build', 'installer-dist'))):
        return True
    return False

entries = []
for current, directories, files in os.walk(fork, followlinks=False):
    directories[:] = sorted(name for name in directories
                            if not exclude_directory(Path(current, name)))
    for name in sorted(files):
        path = Path(current, name)
        if name == '.git' or path.suffix.lower() in binary_extensions or path.is_symlink():
            continue
        entries.append((path, 'work/shadps4-dx12-src/' + path.relative_to(fork).as_posix()))

patch = destination / 'native-fork-tracked.patch'
with patch.open('wb') as stream:
    subprocess.run(['git', '-C', str(fork), 'diff', '--binary', 'HEAD'], stdout=stream, check=True)
entries.append((patch, 'handoff/native-fork-tracked.patch'))
evidence_root = workspace / 'outputs/ps4-desktop-deployment'
for name in ['native-load-core.map', 'verification-native.json', 'native-only-runtime.json',
             'load-owner-repeat-stacks.log', 'load-ranges-stacks.log']:
    entries.append((evidence_root / name, 'handoff/evidence/' + name))
for name, parent in [('native-load-check.log', 'build-native-dx12-clang'),
                     ('native-load-build.log', 'build-full-native-dx12')]:
    entries.append((fork / parent / name, 'handoff/evidence/' + name))
for path in sorted(Path(r'C:\Games\PS4-Desktop\native-profile').glob('load-*.log')):
    entries.append((path, 'handoff/loading-logs/' + path.name))

validation = workspace / 'work/migration-installer/validation'
for name in ['validation.json', 'setup.log', 'uninstall.log', 'core-help.txt', 'core-help-errors.txt']:
    entries.append((validation / name, 'handoff/migration-validation/' + name))
shutil.copy2(validation / 'validation.json', destination / 'validation.json')
for name in ['READ-ME-FIRST.txt', 'PROJECT-HANDOFF.txt', 'INSTALL-DEVELOPMENT.txt']:
    shutil.copy2(fork / 'migration' / name, destination / name)
shutil.copy2(fork / 'migration/dependency-lock.json', destination / 'dependency-lock.json')

archive_path = destination / 'PS4Desktop-Project-Backup-20261009.zip'
records = []
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=5, allowZip64=True) as archive:
    for path, name in entries:
        if not path.is_file():
            raise FileNotFoundError(path)
        archive.write(path, name)
        records.append({'path': name, 'bytes': path.stat().st_size, 'sha256': sha(path)})
    archive.writestr('source-files.json', json.dumps(records, ensure_ascii=False, indent=2))
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None, 'CRC failure'
    assert len(archive.namelist()) == len(set(archive.namelist())), 'Duplicate archive entries'
    assert not any(name.endswith(('.pkg', '.self', '.elf', '.spv')) for name in archive.namelist())

setup = destination / 'PS4Desktop-DX12-Setup-20261009.exe'
assert setup.is_file()
manifest = {'created': datetime.now().astimezone().isoformat(), 'project': 'PS4 Desktop native D3D12 fork',
            'core_sha256': sha(Path(r'C:\Games\PS4-Desktop\native\core\shadPS4-DX12-Core.exe')),
            'game_included': False, 'game_save_profile_included': False,
            'development_dependencies_included': False, 'dependency_restore_instructions_included': True,
            'runtime_dependencies_bundled_in_setup': True, 'source_archive_CRC_verified': True,
            'saved_world_verified': False,
            'files': [{'file': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path)}
                      for path in [setup, archive_path, destination / 'READ-ME-FIRST.txt',
                                   destination / 'PROJECT-HANDOFF.txt', destination / 'INSTALL-DEVELOPMENT.txt',
                                   destination / 'dependency-lock.json', destination / 'validation.json']]}
(destination / 'transfer-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
(destination / 'SHA256SUMS.txt').write_text('\n'.join(f"{row['sha256']}  {row['file']}" for row in manifest['files']) + '\n', encoding='utf-8')
print(json.dumps({'source_files': len(records), 'source_zip_bytes': archive_path.stat().st_size,
                  'setup_bytes': setup.stat().st_size, 'CRC_verified': True, 'destination': str(destination)}, ensure_ascii=False))
