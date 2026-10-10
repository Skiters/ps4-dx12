"""No game needed: encrypted PKG fixture, byte checks and transactional failures."""
import importlib.util
import io
import contextlib
from pathlib import Path
import sys
import tempfile
import subprocess
import json

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'publish' / 'installer'))
sys.path.insert(0, str(HERE.parents[1] / 'pkg-installer' / 'tests'))
import pkg_worker as worker
import make_test_pkg as fixtures


def expect_error(action, label):
    try:
        action()
    except (ValueError, InterruptedError, FileExistsError):
        print('PASS:', label)
        return
    raise AssertionError(label)


with tempfile.TemporaryDirectory(prefix='ps4-installer-test-') as temporary:
    root = Path(temporary)
    source = root / 'source'
    (source / 'sce_sys').mkdir(parents=True)
    content = {
        'eboot.bin': bytes(range(256)) * 1024,
        'data/effects.bin': b'effect-stream' * 20000,
        'data/empty.bin': b'',
        'sce_sys/param.sfo': fixtures._make_sfo({'TITLE_ID': 'CUSA00001', 'TITLE': 'Installer test', 'CATEGORY': 'gd', 'APP_VER': '01.00'}),
    }
    for name, data in content.items():
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    package = root / 'game.pkg'
    fixtures.build_pkg(str(source), str(package), title='Installer test')
    destination = root / 'Games'
    with worker.PkgReader(str(package)) as reader:
        with contextlib.redirect_stdout(io.StringIO()):
            installed = worker.install(reader, destination, [], root / 'addcont', root / 'cancel')
        for name, data in content.items():
            if name != 'sce_sys/param.sfo':
                assert (installed / name).read_bytes() == data, name
        assert worker.parse_sfo((installed / 'sce_sys/param.sfo').read_bytes())['TITLE_ID'] == 'CUSA00001'
        print('PASS: encrypted PKG -> complete folder; exact byte comparison')
        before = (installed / 'eboot.bin').read_bytes()
        expect_error(lambda: worker.install(reader, destination, [], root / 'addcont', root / 'cancel'), 'existing game cannot be overwritten')
        assert (installed / 'eboot.bin').read_bytes() == before
    executable = HERE / 'publish' / 'installer' / 'PS4PackageInstaller.exe'
    if executable.exists():
        probe = subprocess.run([str(executable), 'probe', str(package)], capture_output=True, text=True, encoding='utf-8')
        assert probe.returncode == 0, probe.stderr
        assert json.loads(probe.stdout)['titleId'] == 'CUSA00001'
        result = subprocess.run([str(executable), 'install', str(package), '--root', str(root / 'Frozen'), '--cancel', str(root / 'no-cancel')], capture_output=True, text=True, encoding='utf-8')
        assert result.returncode == 0, result.stdout + result.stderr
        assert (root / 'Frozen' / 'CUSA00001' / 'eboot.bin').read_bytes() == content['eboot.bin']
        print('PASS: standalone installer executable probe + install, no external Python')
    update = root / 'update.pkg'
    (source / 'sce_sys' / 'param.sfo').write_bytes(fixtures._make_sfo({'TITLE_ID': 'CUSA00001', 'TITLE': 'Installer test', 'CATEGORY': 'gp', 'APP_VER': '01.01'}))
    fixtures.build_pkg(str(source), str(update), category='gp', app_ver='01.01')
    with worker.PkgReader(str(update)) as reader, contextlib.redirect_stdout(io.StringIO()):
        updated = worker.install(reader, destination, [], root / 'addcont', root / 'cancel')
    assert updated.name == 'CUSA00001-UPDATE'
    assert worker.parse_sfo((updated / 'sce_sys' / 'param.sfo').read_bytes())['APP_VER'] == '01.01'
    assert (installed / 'eboot.bin').read_bytes() == before
    print('PASS: update uses sidecar folder; base unchanged')
    dlc = root / 'dlc.pkg'
    (source / 'sce_sys' / 'param.sfo').write_bytes(fixtures._make_sfo({'TITLE_ID': 'CUSA00001', 'TITLE': 'Installer test DLC', 'CATEGORY': 'ac', 'APP_VER': '01.00'}))
    fixtures.build_pkg(str(source), str(dlc), content_id='UP0001-CUSA00001_00-TESTDLC000000001', category='ac')
    with worker.PkgReader(str(dlc)) as reader, contextlib.redirect_stdout(io.StringIO()):
        extra = worker.install(reader, destination, [], root / 'addcont', root / 'cancel')
    assert extra.parent.name == 'CUSA00001'
    print('PASS: DLC installed under title and entitlement')
    (root / 'cancel').write_text('cancel')
    with worker.PkgReader(str(package)) as reader:
        expect_error(lambda: worker.install(reader, root / 'Cancelled', [], root / 'addcont', root / 'cancel'), 'cancel before commit')
    assert not list((root / 'Cancelled').glob('.ps4-install-*'))
    assert not (root / 'Cancelled' / 'CUSA00001').exists()
    for unsafe in ('../escape', '/absolute', 'C:/escape', 'foo:ads', 'NUL.txt', 'foo/../../bar'):
        expect_error(lambda name=unsafe: worker.safe_path(name), 'reject ' + unsafe)
    bad = root / 'bad.pkg'; bad.write_bytes(b'broken')
    try:
        worker.PkgReader(str(bad))
    except Exception:
        print('PASS: damaged PKG rejected')
    else:
        raise AssertionError('damaged PKG accepted')
print('All installer checks passed; no game or save modified.')
