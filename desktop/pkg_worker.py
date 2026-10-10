"""Transactional folder installer. External PKG reader remains in reader/ with provenance."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent / 'reader'))
from ps4_pkg import PkgReader, parse_sfo


def emit(**message):
    print(json.dumps(message, ensure_ascii=False), flush=True)


def safe_path(name):
    name = name.replace('\\', '/')
    path = PurePosixPath(name)
    if path.is_absolute() or not path.parts or any(p in ('..', '.') for p in path.parts):
        raise ValueError('Недопустимый путь в пакете: ' + name)
    for part in path.parts:
        if re.search(r'[<>:"|?*\x00-\x1f]', part) or part.endswith((' ', '.')):
            raise ValueError('Недопустимое имя файла: ' + name)
        if part.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}:
            raise ValueError('Зарезервированное имя файла: ' + name)
    return Path(*path.parts)


def describe(reader):
    info = reader.info()
    if not re.fullmatch(r'CUSA[0-9]{5}', info.title_id):
        raise ValueError('Пакет не содержит корректный TITLE_ID')
    if info.category not in ('gd', 'gp', 'ac'):
        raise ValueError('Не поддерживается категория PKG: ' + info.category)
    paths = [safe_path(item.path) for item in reader.files]
    folded = [str(path).casefold() for path in paths]
    if len(set(folded)) != len(folded):
        raise ValueError('В пакете есть повторяющиеся пути')
    if any(item.size < 0 for item in reader.files):
        raise ValueError('Отрицательный размер файла')
    if 'sce_sys\\param.sfo' not in folded and 'sce_sys/param.sfo' not in folded:
        raise ValueError('Отсутствует param.sfo')
    if info.category == 'gd' and 'eboot.bin' not in folded:
        raise ValueError('Отсутствует eboot.bin')
    return info, paths, sum(item.size for item in reader.files)


def find_base(roots, title_id):
    for root in roots:
        root = Path(root)
        if not root.exists():
            continue
        for folder in [root, *(p for p in root.iterdir() if p.is_dir() and not p.name.startswith('.'))]:
            sfo = folder / 'sce_sys' / 'param.sfo'
            if sfo.is_file() and (folder / 'eboot.bin').is_file():
                if parse_sfo(sfo.read_bytes()).get('TITLE_ID') == title_id:
                    return folder
    return None


def install(reader, root, roots, addon, cancel):
    info, paths, total = describe(reader)
    root = Path(root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    if info.category == 'gd':
        base = find_base([root, *roots], info.title_id)
        if base:
            raise ValueError('Эта игра уже установлена: ' + str(base))
        destination = root / info.title_id
    elif info.category == 'gp':
        base = find_base([root, *roots], info.title_id)
        if not base:
            raise ValueError('Сначала установите базовую игру ' + info.title_id)
        destination = base.with_name(base.name + '-UPDATE')
    else:
        label = info.content_id[20:]
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,40}', label):
            raise ValueError('Некорректное имя DLC')
        destination = Path(addon).resolve() / info.title_id / label
    if destination.exists():
        raise ValueError('Каталог уже существует; он не будет перезаписан: ' + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(destination.parent).free < total + 64 * 1024 * 1024:
        raise ValueError('Недостаточно места для распакованных файлов')
    stage = destination.parent / ('.ps4-install-' + uuid.uuid4().hex)
    stage.mkdir()
    complete = 0
    last = 0.0
    try:
        for item, relative in zip(reader.files, paths):
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            size = 0
            with target.open('xb') as out:
                for chunk in reader.read_file(item):
                    if Path(cancel).exists():
                        raise InterruptedError('Установка отменена')
                    size += len(chunk)
                    if size > item.size:
                        raise ValueError('Размер файла превышает заявленный: ' + item.path)
                    out.write(chunk)
                    complete += len(chunk)
                    now = time.monotonic()
                    if now - last >= .15:
                        emit(type='progress', done=complete, total=total, file=item.path)
                        last = now
            if size != item.size:
                raise ValueError('Неполный файл: ' + item.path)
        if Path(cancel).exists():
            raise InterruptedError('Установка отменена')
        # Commit only a complete tree, and never replace an existing installation.
        stage.rename(destination)
        emit(type='installed', path=str(destination), title=info.title, titleId=info.title_id)
        return destination
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('mode', choices=['probe', 'install'])
    p.add_argument('pkg')
    p.add_argument('--root', default='Games')
    p.add_argument('--roots', default='[]')
    p.add_argument('--addon', default='addcont')
    p.add_argument('--cancel', default='.cancel')
    a = p.parse_args()
    try:
        with PkgReader(a.pkg) as reader:
            if a.mode == 'probe':
                info, _, size = describe(reader)
                emit(type='info', title=info.title, titleId=info.title_id, version=info.app_version,
                     category=info.category, bytes=size, files=len(reader.files))
            else:
                install(reader, a.root, json.loads(a.roots), a.addon, a.cancel)
        return 0
    except Exception as exc:
        emit(type='error', message=str(exc))
        return 2


if __name__ == '__main__':
    sys.exit(main())
