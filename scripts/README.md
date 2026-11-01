# Workflow scripts

Both entry points use a POSIX shell and locate the repository relative to their
own location.

- `./scripts/build.sh` compiles the x86_64 ABIv11 application, native drop
  probe, icon comparison probe, and native resampler, composition and
  interaction tests. It stages SVG assets and the separate renderer library
  into a local test package. Required variables: `AROS_CC`, `AROS_SDK`,
  `SVGICON_INCLUDE` and `SVGICON_LIBRARY`. `DOCK_BUILD_DIR` optionally changes
  the output directory.
- `./scripts/test-host.sh` uses `${CC:-cc}` to build and run model,
  composition/configuration, pointer interaction and resampler tests. It uses
  a temporary directory and removes it on exit.

Export configuration explicitly before building. There is no automatic
`local.env` loader. You may edit a private copy of
[`local.env.example`](../local.env.example) and explicitly source it; its
assignments replace those variables in your shell.

See [development and usage](../docs/development.md) for commands, runtime
controls, versioned configuration, legacy-pin migration and verification
limits. Native composition tests need a disposable configuration path.
Neither script installs into an AROS system, changes a system library, boots
a VM, or rebuilds the SVG dependency.

- `test-applets.sh`: bounded package/JSON/snapshot/cache/settings and weather tests (also run by test-host.sh).
- `build-weather.sh`: independent Weather provider build, host or caller-selected target compiler.
- `package-applet.py`: reproducible applet archive and SHA-256 from one built target variant.

- `build-bundled-applets.sh`: invoked by the normal ABIv11 build; compiles and stages Weather with the target compiler/SDK.
- `stage-weather.py`: stages metadata/provider/assets and optional locally configured licensed sprites; no sources or user state.
- `test_weather_bundle.py` under tests/: regression coverage for standard applet bundling and incomplete-package rejection.
