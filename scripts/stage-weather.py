#!/usr/bin/env python3
"""Stage a complete bundled applet; purchased artwork stays outside source control."""
import json
import os
import pathlib
import shutil
import struct
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dockpak  # noqa: E402

repo = pathlib.Path(__file__).resolve().parents[1]
args = sys.argv[1:]
if len(args) != 4 or args[0] != '--target':
    sys.exit('usage: stage-weather.py --target TARGET PROVIDER DESTINATION')
target = args[1]
binary, destination = map(pathlib.Path, args[2:])
source = repo / 'applets/org.aros.weather'
manifest = json.loads((source/'applet.json').read_text())
config = pathlib.Path(os.environ.get('WEATHER_ASSETS_CONFIG', repo/'local-weather-assets.json'))
art = os.environ.get('WEATHER_ASSETS_DIR')
# Set but empty: build with the placeholder pictures only (tests).
placeholders = art == ''
if placeholders:
    art = None
if art is None and not placeholders and config.exists():
    art = json.loads(config.read_text())['source']
    if not pathlib.Path(art).is_absolute():
        art = str(config.parent / art)
# The usual place: the untracked purchased/ drawer of the widget itself.
if art is None and not placeholders and (source/'purchased').is_dir():
    art = str(source/'purchased')
destination.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='.weather-', dir=destination) as tmp:
    package = pathlib.Path(tmp)/manifest['id']
    package.mkdir()
    for name in ['applet.json', 'settings.json', 'views.json', 'LICENSE']:
        shutil.copy2(source/name, package/name)
    for folder in ['assets', 'licenses', 'certs']:
        if (source/folder).exists():
            shutil.copytree(source/folder, package/folder)
    # A Dockbar package is for one target: its Weather names only that
    # target's provider, so another ABI's Dockbar finds none and says so.
    if target not in manifest['entryPoints']:
        sys.exit(f'applet.json has no entry point for {target}')
    manifest['entryPoints'] = {target: manifest['entryPoints'][target]}
    (package/'applet.json').write_text(json.dumps(manifest, indent=2)+'\n')
    entry = package/manifest['entryPoints'][target]
    entry.parent.mkdir(parents=True)
    shutil.copy2(binary, entry)
    entry.chmod(0o755)
    if art is not None:
        art = pathlib.Path(art).expanduser().resolve(strict=True)
        assets = package/'assets'
        conditions = json.loads((assets/'conditions.json').read_text())
        mapping = {'clear':'clear-sky', 'partly-cloudy':'few-clouds',
                   'scattered-clouds':'scattered-clouds', 'cloudy':'broken-clouds',
                   'fog':'mist', 'drizzle':'rain', 'rain':'rain', 'freezing-rain':'rain',
                   'snow':'snow', 'showers':'shower-rain', 'snow-showers':'snow',
                   'thunderstorm':'thunderstorm', 'thunderstorm-hail':'thunderstorm'}
        imported = set()
        packed = []
        for key, condition in conditions['conditions'].items():
            phase = 'night' if key.endswith('-night') else 'day'
            stem = key.removesuffix('-'+phase)
            if stem not in mapping:
                continue
            matches = list((art/phase.title()/'128').glob(mapping[stem]+'-'+phase+'-*-sprite.json'))
            if len(matches) != 1:
                raise ValueError(f'Missing or ambiguous artwork: {key}')
            metadata = json.loads(matches[0].read_text())
            png = matches[0].with_suffix('.png')
            raw = png.read_bytes()
            if raw[:8] != b'\x89PNG\r\n\x1a\n' or len(raw) > 4*1024*1024:
                raise ValueError(f'Invalid sprite PNG: {png.name}')
            width, height = struct.unpack('>II', raw[16:24])
            fw, fh, cols, count, fps = [metadata[k] for k in ['frameWidth','frameHeight','columns','frames','fps']]
            if (fw != 128 or fh != 128 or not 1 <= cols <= 32 or not 1 <= count <= 256
                    or not 1 <= fps <= 60 or width != fw*cols or height % fh
                    or count > cols*(height//fh) or width > 4096 or height > 4096):
                raise ValueError(f'Unsupported sprite metadata: {png.name}')
            if png.name not in imported:
                packed.append((png.name, raw))
            imported.add(png.name)
            # The licence allows the icons inside this program but not as
            # loose files that others could extract and reuse: they ship in
            # one private pack that only Dockbar reads (scripts/dockpak.py).
            condition['sprite'] = dict(resource='sprites.pak#'+png.name, frameWidth=fw, frameHeight=fh,
                                       columns=cols, frames=count, fps=fps)
        assert len(imported) == 18, 'Expected all nine day/night pairs'
        dockpak.write(assets/'sprites.pak', packed)
        (assets/'README-icons.txt').write_text(
            'The animated weather icons in sprites.pak are Animated Weather Icons,\n'
            '(c) 2026 Ubaidullah Effendi-Emjedi, used in Dockbar under licence.\n'
            'They are not covered by the MIT licence of this widget and may not be\n'
            'extracted, redistributed or reused outside Dockbar.\n')
        (assets/'conditions.json').write_text(json.dumps(conditions, indent=2)+'\n')
        print(f'Weather artwork: purchased sprites packed (18 sheets in assets/sprites.pak)')
    else:
        print('Weather artwork: bundled SVG placeholders; configure local-weather-assets.json for purchased sprites')
    target = destination/manifest['id']
    if target.exists():
        shutil.rmtree(target)
    shutil.move(str(package), target)
    print(f'Bundled Weather: {target}')
