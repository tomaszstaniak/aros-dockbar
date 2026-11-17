#!/usr/bin/env python3
"""Stage the bundled Currency widget for one target. Its certificate bundle
and the licence texts of the provider's libraries are the Weather widget's:
both providers link the same libraries."""
import json
import pathlib
import shutil
import sys

repo = pathlib.Path(__file__).resolve().parents[1]
args = sys.argv[1:]
if len(args) != 4 or args[0] != '--target':
    sys.exit('usage: stage-currency.py --target TARGET PROVIDER DESTINATION')
target = args[1]
binary, destination = map(pathlib.Path, args[2:])
source = repo / 'applets/org.aros.currency'
shared = repo / 'applets/org.aros.weather'
manifest = json.loads((source/'applet.json').read_text())
if target not in manifest['entryPoints']:
    sys.exit(f'applet.json has no entry point for {target}')
package = destination/manifest['id']
if package.exists():
    shutil.rmtree(package)
package.mkdir(parents=True)
for name in ['settings.json', 'views.json', 'LICENSE']:
    shutil.copy2(source/name, package/name)
for folder in ['licenses', 'certs']:
    shutil.copytree(shared/folder, package/folder)
# one target per Dockbar package, as for Weather
manifest['entryPoints'] = {target: manifest['entryPoints'][target]}
(package/'applet.json').write_text(json.dumps(manifest, indent=2)+'\n')
entry = package/manifest['entryPoints'][target]
entry.parent.mkdir(parents=True)
shutil.copy2(binary, entry)
entry.chmod(0o755)
print(package)
