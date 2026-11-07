# Build and packaging scripts

Select `DOCK_TARGET` and export matching compiler, SDK and SVG inputs before building. Scripts do not load `local.env` automatically. See [development](../docs/development.md) and [local.env.example](../local.env.example).

| Script | Purpose |
| --- | --- |
| `build.sh` | Build Dockbar, bundled Weather and developer probes for the selected target |
| `target.sh` | Shared target, ABI, output naming and link-library selection |
| `test-host.sh` | Run host tests in temporary storage |
| `package.sh` | Validate and package a target build with notices and checksums |
| `application-manifest.py` | Generate `application.json` from target TOML metadata |
| `scan-paths.sh` | Check staged files for private host paths |
| `build-bundled-applets.sh` | Build and stage Weather for the main build |
| `build-weather.sh` | Build WeatherProvider independently, using host or supplied target tools |
| `stage-weather.py` | Stage widget metadata, executable and artwork |
| `package-applet.py` | Package one independently built widget target |
| `test-applets.sh` | Test widget parsing, settings, cache and provider behaviour |
| `test-sprite.sh` | Test sprite decoding and timing |

The separate `build-macaros.sh` is not part of the three-target release workflow. Build and packaging scripts do not boot a VM or install system libraries. Native persistence probes require a disposable configuration path.

## Publication boundaries

Run `python3 scripts/check-publication.py --ref HEAD` before publishing source. The CI job also checks ZIP and TAR exports. For a downloaded GitHub source archive, use `--archive PATH`. This project excludes local agent instructions, workspace configuration, private evidence and purchased source artwork. Content, licence and actual public-download reviews are separate checks; a passing job does not certify them.
