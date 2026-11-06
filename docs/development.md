# Development and usage

## Build requirements

Dockbar uses native Intuition, Workbench, icon, CyberGraphX and Zune/MUI APIs. Select a target explicitly and use its matching compiler, SDK and SVG library:

| `DOCK_TARGET` | Platform |
| --- | --- |
| `x86_64-v1` | Mainline AROS, PC |
| `x86_64-abiv11` | ABIv11, PC |
| `aarch64-raspi-v1` | Mainline AROS, Raspberry Pi |

The PC builds use GCC 10.5; the Pi build uses the AROS aarch64 LLVM toolchain. The build checks target and ABI compatibility. The mainline SDK must include the window-composition interfaces used by Dockbar. Each SDK also needs the curl, OpenSSL and supporting static libraries listed in [target.sh](../scripts/target.sh).

Example for ABIv11:

```sh
export DOCK_TARGET=x86_64-abiv11
export AROS_CC="/path/to/toolchain/x86_64-aros-gcc"
export AROS_SDK="/path/to/abiv11-sdk/Development"
export SVGICON_INCLUDE="/path/to/svgicon-dev/include"
export SVGICON_LIBRARY="/path/to/svgicon-dev/libs/svgicon.library"
./scripts/test-host.sh
./scripts/build.sh
```

`AROS_SDK` is the compiler sysroot, not an archive. SVG headers must retain their generated `proto`, `inline`, `defines` and `clib` directories. See [SVG dependency](svg-dependency.md).

Copy [local.env.example](../local.env.example) to ignored `local.env`, fill in local paths and source it explicitly with `. ./local.env`. Scripts do not load it automatically. `DOCK_BUILD_DIR` overrides `build/<target>`; `CC` selects the host test compiler.

The build stages the application in `build/<target>/package/`, including assets, `libs/svgicon.library` and `Widgets/org.aros.weather/`. Developer probes are placed separately in `build/<target>/dev/`. Copy the whole package drawer to AROS. The build does not replace system libraries.

## Packaging

Host packaging tools include Python 3.11 or later, ZIP/UnZip and SHA-256 utilities. The SVG package also requires:

```sh
export SVGICON_REVISION="<renderer source commit>"
export SVGICON_LICENSE_FILE="/path/to/LunaSVG/LICENSE"
export SVGICON_PROJECT="/path/to/aros-svg-icons"
./scripts/package.sh
```

Packaging checks the renderer's target, revision, checksum, BUILDINFO and licence inputs against [svg-provenance.txt](../packaging/svg-provenance.txt). Explicitly pinned legacy libraries are handled separately. It also checks bundled Weather, scans for private build paths and generates `application.json` from target metadata. See [application folder metadata](application-folder.md).

The default output is one full archive and an updated `SHA256SUMS` in `dist/`. `DOCK_DIST_DIR` selects another output directory. Since 0.5.2, releases publish the full SVG variant. `DOCK_SVG=0` still permits a renderer-free build for development. Packaging refuses to overwrite an existing archive with different bytes. Fixed timestamps make packaging reproducible from identical inputs.

## Bundled Weather build

All three targets build Weather with the selected compiler and SDK. A provider build failure fails the build; packaging rejects an incomplete widget. Weather is added through **Add widget...**, without changing an existing dock layout. Preserve `docks.config` and `WidgetState` during updates.

Source builds use original SVG weather artwork unless a licensed sprite pack is configured. The bundler checks `applets/org.aros.weather/purchased/`, `WEATHER_ASSETS_DIR` or the ignored `local-weather-assets.json` (see [example](../local-weather-assets.example.json)). `WEATHER_ASSETS_CONFIG` selects another configuration file. A configured missing or incomplete pack fails the build.

Purchased sheets are bundled in `assets/sprites.pak`, not as loose images. This is a resource format, not encryption or a substitute for permission to distribute the artwork. Keep purchased sources, generated packs and local asset paths out of Git. Package notices describe their separate terms.

## Running and controls

```text
Dockbar --log RAM:dockbar.log --config RAM:docks.config
```

Use a persistent, writable configuration path to keep settings across system restarts. The download's ReadMe covers autostart, including `Run <NIL: >NIL: QUIET` and an assign for installation paths containing spaces.

| Action | Control |
| --- | --- |
| Pin an application | Drop it from Wanderer onto a dock |
| Launch an item or open its panel | Click its icon |
| Reorder or move an item | Drag it to an insertion edge |
| Create a group | Hold an application over another application, then release |
| Move an item into a group | Drop into its panel, or hold over the group to open it |
| Edit an item | Shift-click, or use **Edit items...** in the dock menu |
| Move a dock | Drag its grip |
| Open the dock menu | Click its grip |
| Add Weather | **Add widget...** in the dock menu |
| Change layout and appearance | **Preferences...** |
| Remove a separator or spacer | **Edit items...**, select it, then Remove |
| Quit | **Quit** in the dock menu |

Removing a shortcut does not delete its target. Removing a widget also removes its saved settings and cached data once the provider has stopped; see [widget storage](applets.md#directories-and-compatibility). Locked docks reject editing and item movement. Preferences **Apply** previews changes, **Save** persists them, and **Cancel** restores the saved layout.

## Options and saved configuration

| Option | Purpose |
| --- | --- |
| `--config PATH` | Configuration; default `PROGDIR:docks.config` |
| `--log PATH` | Diagnostic log |
| `--log-entries` | Include drawer entry lists in the log |
| `--seconds N` | Exit after N seconds |
| `--icon-size N` | Icon size override, 32–72 px |
| `--corner N` | Corner radius override, 0–8 px |
| `--no-compose` | Use desktop capture instead of system window composition |
| `--no-transitions` | Disable widget data crossfades |
| `--widgets PATH` | Widget package root; default `PROGDIR:Widgets` |
| `--widget-state PATH` | Writable state root; default `PROGDIR:WidgetState` |
| `--svg-library PATH` | Renderer path; default `PROGDIR:libs/svgicon.library` |
| `--full-redraw` | Diagnostic comparison without partial redraws |
| `--reopen-on-resize` | Diagnostic comparison using window reopening |

Legacy import options are `--legacy-config`, `--legacy-prefs` and `--pins`. The old `--applets` and `--applet-state` names remain aliases. `--svg` is accepted for old scripts but no longer selects icons; choose an icon in the item editor.

The versioned configuration stores dock layouts, appearance, items, groups and widget identities. Malformed or newer configuration files are preserved rather than overwritten. Legacy files are imported when the current configuration is absent. Use the UI to edit layouts; keep a backup before changing configuration files manually.

## Rendering

SVG icons are rendered at the requested size and cached. Missing libraries or invalid images fall back to native icons or placeholders. Native ARGB and indexed icons use application-local filtering; planar icons use the native drawing path.

On systems supporting window composition, Dockbar supplies an alpha surface and the system updates the background behind it. Other systems use a captured desktop background. `--no-compose` selects this fallback explicitly. Live compositor backdrop blur and running-application indicators are separate development work, not features of the 0.6.0 release.

## Checks

`./scripts/test-host.sh` runs the host suites in temporary storage. Native probes are built separately; use a disposable configuration path for persistence tests because they replace it and its backup. Host checks do not exercise Intuition or real device drivers.

For a release, verify the target build, package provenance and installed application together. Runtime coverage and material system requirements belong in the release notes; raw test logs and temporary screenshots are not build inputs.
