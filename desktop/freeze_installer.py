from pathlib import Path
import sys

here = Path(__file__).resolve().parent
import PyInstaller.__main__

PyInstaller.__main__.run([
    '--noconfirm', '--clean', '--onedir', '--name=PS4PackageInstaller',
    '--paths=' + str(here / 'reader'),
    '--distpath=' + str(here / 'installer-dist'),
    '--workpath=' + str(here / 'installer-build'),
    '--specpath=' + str(here / 'installer-build'),
    str(here / 'pkg_worker.py'),
])
