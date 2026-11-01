#!/usr/bin/env python3
"""Build an independently versioned applet ZIP; never copy provider sources/state."""
import argparse
import hashlib
import json
import pathlib
import re
import zipfile


def load(path):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result
    data = path.read_bytes()
    if len(data) > 65536:
        raise ValueError(f"Metadata too large: {path.name}")
    return json.loads(data, object_pairs_hook=pairs)


def package(source, binary, target, output):
    source = source.resolve()
    manifest = load(source / 'applet.json')
    identity, version = manifest['id'], manifest['version']
    if not re.fullmatch(r'[A-Za-z0-9_.-]{1,79}', identity) or identity in ('.', '..'):
        raise ValueError('Invalid package ID')
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', version):
        raise ValueError('Expected a semantic package version')
    entry = pathlib.PurePosixPath(manifest['entryPoints'][target])
    if entry.is_absolute() or '..' in entry.parts or ':' in str(entry):
        raise ValueError('Unsafe entry point')
    # A release advertises exactly the variant present in this archive.
    manifest['entryPoints'] = {target: str(entry)}
    files = {'applet.json': (json.dumps(manifest, indent=2) + '\n').encode()}
    for name in (manifest['views'], manifest['settings'], 'LICENSE'):
        rel = pathlib.PurePosixPath(name)
        if rel.is_absolute() or '..' in rel.parts or ':' in name:
            raise ValueError('Unsafe package path')
        path = source / name
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(source):
            raise ValueError(f'Unsafe or missing input: {name}')
        if path.suffix == '.json':
            load(path)
        files[str(rel)] = path.read_bytes()
    for folder in ('assets', 'licenses'):
        root = source / folder
        if not root.exists():
            continue
        for path in root.rglob('*'):
            if path.is_symlink():
                raise ValueError(f'Symlinks cannot ship: {path}')
            if path.is_file():
                if not path.resolve().is_relative_to(source) or path.stat().st_size > 4 * 1024 * 1024:
                    raise ValueError(f'Unsafe asset: {path}')
                files[str(path.relative_to(source))] = path.read_bytes()
    if binary.stat().st_size > 32 * 1024 * 1024:
        raise ValueError('Provider exceeds 32 MiB')
    files[str(entry)] = binary.read_bytes()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f'{identity}-{version}-{target}.zip'
    staging = archive.with_suffix('.zip.tmp')
    with zipfile.ZipFile(staging, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, content in sorted(files.items()):
            info = zipfile.ZipInfo(f'{identity}/{name}', (2026, 10, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if name == str(entry) else 0o100644) << 16
            z.writestr(info, content)
    digest = hashlib.sha256(staging.read_bytes()).hexdigest()
    if archive.exists() and archive.read_bytes() != staging.read_bytes():
        staging.unlink()
        raise ValueError(f'Refusing to replace an existing release: {archive}')
    staging.replace(archive)
    archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n')
    print(archive)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    package(args.source, args.binary, args.target, args.output)
