"""Exercise the default bundled applet pipeline without a target toolchain."""
import json, os, pathlib, subprocess, tempfile
repo = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='dock-weather-build-') as tmp:
    root = pathlib.Path(tmp)
    compiler = root / 'cc'
    compiler.write_text('''#!/usr/bin/env python3
import sys,pathlib,json
args=sys.argv[1:]
pathlib.Path(__file__).with_name('args.json').write_text(json.dumps(args))
p=pathlib.Path(args[args.index('-o')+1]);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'provider');p.chmod(0o755)
''')
    compiler.chmod(0o755)
    sdk = root/'test-sdk'
    for name in ['lib/libcurl.a', 'lib/libssl.a', 'lib/libcrypto.a', 'include/curl/curl.h']:
        (sdk/name).parent.mkdir(parents=True, exist_ok=True); (sdk/name).write_bytes(b'')
    env = dict(os.environ, AROS_CC=str(compiler), AROS_SDK=str(sdk), DOCK_TARGET='x86_64-abiv11',
               DOCK_BUILD_DIR=str(root/'build'), WEATHER_ASSETS_CONFIG=str(root/'absent.json'))
    env['WEATHER_ASSETS_DIR'] = ''  # placeholders, whatever purchased/ holds
    command = ['sh', str(repo/'scripts/build-bundled-applets.sh')]
    subprocess.run(command, env=env, check=True)
    app = root/'build/package/Widgets/org.aros.weather'
    manifest = json.loads((app/'applet.json').read_text())
    assert manifest['entryPoints'] == {'x86_64-abiv11':'bin/x86_64-abiv11/WeatherProvider'}
    assert (app/'bin/x86_64-abiv11/WeatherProvider').read_bytes() == b'provider'
    assert not (app/'provider').exists() and not (app/'AppletState').exists()
    assert (app/'assets/conditions.json').is_file() and (app/'LICENSE').is_file()
    args=json.loads((root/'args.json').read_text())
    assert f'--sysroot={sdk}' in args and '-lcurl' in args and '-lssl' in args
    # The mainline target stages its own provider and names only that one.
    envv1 = dict(env, DOCK_TARGET='x86_64-v1', DOCK_BUILD_DIR=str(root/'build-v1'))
    subprocess.run(command, env=envv1, check=True, stdout=subprocess.DEVNULL)
    v1 = root/'build-v1/package/Widgets/org.aros.weather'
    assert json.loads((v1/'applet.json').read_text())['entryPoints'] == {'x86_64-v1':'bin/x86_64-v1/WeatherProvider'}
    assert (v1/'bin/x86_64-v1/WeatherProvider').is_file() and not (v1/'bin/x86_64-abiv11').exists()
    assert '-lmiami' in json.loads((root/'args.json').read_text())
    # A missing curl is a build error, not a Dockbar without Weather.
    (sdk/'lib/libcurl.a').unlink()
    r = subprocess.run(command, env=envv1, capture_output=True, text=True)
    assert r.returncode != 0 and 'libcurl.a' in r.stderr
    (sdk/'lib/libcurl.a').write_bytes(b'')
    # Licensed artwork ships packed: no loose PNG sheets, no local path.
    import struct, zlib
    art = root/'my-purchased-art'
    def png(path):
        def chunk(t, d): return struct.pack('>I', len(d))+t+d+struct.pack('>I', zlib.crc32(t+d))
        rows = b''.join(b'\0'+b'\x80'*512 for _ in range(128))
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 128, 128, 8, 6, 0, 0, 0))
                         + chunk(b'IDAT', zlib.compress(rows))+chunk(b'IEND', b''))
    stems = ['clear-sky', 'few-clouds', 'scattered-clouds', 'broken-clouds', 'mist', 'rain',
             'shower-rain', 'snow', 'thunderstorm']
    for phase in ('Day', 'Night'):
        d = art/phase/'128'; d.mkdir(parents=True)
        for i, stem in enumerate(stems):
            base = d/f'{stem}-{phase.lower()}-{i:02d}-sprite'
            png(base.with_suffix('.png'))
            base.with_suffix('.json').write_text(json.dumps(dict(frameWidth=128, frameHeight=128, columns=1, frames=1, fps=1)))
    (art/'LICENSE.txt').write_text('EULA text')
    env2 = dict(env, WEATHER_ASSETS_DIR=str(art))
    subprocess.run(command, env=env2, check=True, stdout=subprocess.DEVNULL)
    assets = app/'assets'
    assert (assets/'sprites.pak').is_file() and not list(assets.glob('*.png'))
    assert not (assets/'purchased-weather-LICENSE.txt').exists()
    assert b'\x89PNG' not in (assets/'sprites.pak').read_bytes()
    cond = json.loads((assets/'conditions.json').read_text())
    sprites = [c['sprite']['resource'] for c in cond['conditions'].values() if 'sprite' in c]
    assert sprites and all(r.startswith('sprites.pak#') for r in sprites)
    for f in app.rglob('*'):
        if f.is_file(): assert str(art).encode() not in f.read_bytes(), f
    # Explicitly configured missing artwork must fail, not silently omit it.
    env['WEATHER_ASSETS_DIR']=str(root/'missing-art')
    assert subprocess.run(command, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0
    # Packaging must reject a partial ordinary build instead of omitting Weather.
    (root/'build/package/Dockbar').write_bytes(b'dockbar')
    (root/'build/package/Dockbar').chmod(0o755)
    (app/'bin/x86_64-abiv11/WeatherProvider').unlink()
    env.update(DOCK_SVG='0', DOCK_DIST_DIR=str(root/'dist'))
    result=subprocess.run(['sh',str(repo/'scripts/package.sh')],env=env,capture_output=True,text=True)
    assert result.returncode != 0 and 'Missing bundled Weather' in result.stderr
print('PASS bundled weather: target compiler, complete package, no sources/state, licensed art packed without loose PNGs or local paths, missing artwork rejected')
