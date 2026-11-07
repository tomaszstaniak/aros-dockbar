#!/usr/bin/env python3
"""Reject local workspace files in a source revision or source archive."""
import argparse
from pathlib import PurePosixPath
import subprocess
import sys
import tarfile
import zipfile

LOCAL_NAMES = {'agents.md', 'claude.md', 'project.json', '.workspace.json',
               'local.env', 'local.json', 'local-weather-assets.json', '.ds_store'}
PRIVATE_ROOTS = {'work', 'internal', 'references', 'upstream', 'build', 'dist'}
PRIVATE_DOCS = {'evidence', 'current-state', 'backlog', 'research'}


def private_path(name):
    parts = PurePosixPath(name).parts
    if not parts:
        return False
    lower = tuple(p.lower() for p in parts)
    return (any(p in LOCAL_NAMES for p in lower)
            or lower[0] in PRIVATE_ROOTS
            or (len(lower) > 1 and lower[0] == 'docs' and lower[1] in PRIVATE_DOCS)
            or lower[:3] == ('applets', 'org.aros.weather', 'purchased'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', help='Git revision; default: tracked index')
    parser.add_argument('--archive', help='Source ZIP or TAR, including GitHub source downloads')
    args = parser.parse_args()
    if args.archive:
        if zipfile.is_zipfile(args.archive):
            with zipfile.ZipFile(args.archive) as archive:
                names = [i.filename for i in archive.infolist() if not i.is_dir()]
        else:
            with tarfile.open(args.archive) as archive:
                names = [i.name for i in archive.getmembers() if not i.isdir()]
        # GitHub sources have one enclosing directory. Keep unwrapped archives too.
        parts = [PurePosixPath(n).parts for n in names]
        wrapped = bool(parts) and all(len(p) > 1 for p in parts) and len({p[0] for p in parts}) == 1
        names = ['/'.join(p[1:] if wrapped else p) for p in parts]
    else:
        command = ['git', 'ls-tree', '-rz', '--name-only', args.ref] if args.ref else ['git', 'ls-files', '-z']
        names = subprocess.check_output(command).decode().rstrip('\0').split('\0')
    failures = [n for n in names if private_path(n)]
    for name in failures:
        print('Local/private publication entry: ' + name, file=sys.stderr)
    if failures:
        return 1
    print('Publication file boundaries OK (content and licence review are separate).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
