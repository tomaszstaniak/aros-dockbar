---
type: guide
updated: 2026-10-13
---

# Development and usage

## Build requirements

Use a matching x86_64 AROS compiler and ABIv11 2026.09 SDK. The application is
GNU C99 and uses stock Intuition, Workbench, icon, CyberGraphX, Zune/MUI and
`input.device` facilities. No system libraries are replaced. The build requires
externally supplied generated SVG headers and a matching `svgicon.library`;
it does not build that dependency. See [SVG dependency and provenance](svg-dependency.md).

Export paths in your shell before building:

```sh
export AROS_CC="/path/to/toolchain/x86_64-aros-gcc"
export AROS_SDK="/path/to/abiv11-sdk/Development"
export SVGICON_INCLUDE="/path/to/svgicon-dev/include"
export SVGICON_LIBRARY="/path/to/svgicon-dev/libs/svgicon.library"
./scripts/test-host.sh
./scripts/build.sh
```

These are placeholders, not repository-supplied paths. `SVGICON_INCLUDE` must
contain generated `proto`, `inline`, `defines` and `clib` subdirectories.
`AROS_SDK` is the compiler sysroot, not the SDK archive. An absolute
`DOCK_BUILD_DIR` overrides the default output directory. `CC` selects the host
C compiler and defaults to `cc`.

[local.env.example](../local.env.example) contains matching exports. You may
copy it to ignored `local.env`, edit it, and explicitly source it with
`. ./local.env`. The scripts do not load it automatically; sourcing its
assignments replaces those variables in your shell.

The build emits the application and, separately, developer probes:

```text
build/x86_64-abiv11/dev/
  native_drop_probe  icon_scaling_probe  test_resample
  test_composition   test_interaction    backdrop_probe
build/x86_64-abiv11/package/
  Dockbar
  assets/folder.svg
  assets/terminal.svg
  assets/compass.svg
  libs/svgicon.library
  Applets/org.aros.weather/  # metadata, assets and target WeatherProvider
```

Keep the directory layout together on the target. This is not a public release
archive; stock `icon.library` remains installed unchanged.

## Tester archives

`scripts/package.sh` turns an existing build into two archives in `dist/`:
one with the SVG renderer and one `.info`-only, plus `SHA256SUMS`. Each holds a
`Dockbar` drawer with the program, `.info` icons drawn by
`scripts/make-icons.py`, `ReadMe` and `NOTICES` filled from `packaging/`. The
SVG variant needs `SVGICON_REVISION` (the renderer's source commit) and
`SVGICON_LICENSE_FILE` (its LunaSVG licence); `DOCK_SVG=0` builds only the
`.info`-only archive. Timestamps are fixed, so equal inputs give equal archives.

## Running and controls

From an AROS Shell in the package drawer:

```text
Dockbar --log RAM:dockbar.log --config RAM:dockbar.config
```

Choose a writable configuration path. `RAM:` is useful for testing application
restarts; use persistent storage to retain settings across system restarts.
The default composition contains Shell, MultiView, Prefs, a Tools drawer,
separators and a live clock. Names appear on hover; feedback appears briefly
in a popup.

| Action | Control |
|---|---|
| Launch an application or open a drawer | Click its icon |
| Pin an application from Wanderer | Drop it on the bar, a group icon, or an open group grid |
| Create a curated group | Drag an application onto the center of another root application, hold for about half a second until highlighted, then release |
| Open or close a group grid | Click its group icon |
| Reorder items | Drag to an insertion edge in the bar or grid |
| Move an item into a group | Drop into its open grid, or dwell over the center of its group icon before releasing |
| Extract a group child | Drag it from the grid to an insertion edge on the bar |
| Move the dock | Drag the left grip; release to save the position (not while locked) |
| Edit a launcher, drawer or group | Shift-click it, or right-click it while the dock or grid is active |
| Edit the open group | Click **Edit** in the grid header |
| Cancel a drag or editor | Press Escape; the editor also has **Cancel** |
| Close a group grid | Click outside it, click its group icon again, or press Escape |
| Open the dock menu | Click the dotted grip without dragging, or right-click it while the dock is active |
| Preferences | Dock menu → Preferences...: icon size, corner radius, lock position; Save writes `dock.prefs` |
| Exit the dock | Dock menu → Quit; Escape when no drag, grid or editor is active; Shell Ctrl-C and `--seconds` |

A drag starts after six pixels of movement. Once dragging has started, returning
to the start position does not turn it into a launch click. A group holds only
application shortcuts; groups cannot nest. Empty groups remain available for
new items. Up to eight groups are supported, with 24 items per group and 24
root items including drawers, separators and the clock. Available screen width
can impose a lower root limit. Position is clamped to the screen, reserving the
top 20 pixels.

The Zune/MUI editor changes **Name** and **Icon**. An icon override accepts a native
`.info` path or an SVG path; leave it blank for automatic imagery. Groups
normally show a preview of up to four children. **Remove** removes the selected
shortcut; for a group it removes the group and all shortcuts inside it. These
operations do not delete, rename or move the underlying applications or files.
There is no Delete-key binding.

## Options and saved configuration

| Option | Default | Purpose |
|---|---|---|
| `--icon-size N` | from `dock.prefs`, else `40` | Icon box size 32–72 px for this run only |
| `--seconds N` | `0` | Exit after N seconds; 0 runs until closed |
| `--log PATH` | None | Write diagnostic output to a file (drawer panel timing included) |
| `--log-entries` | off | With `--log`: list a drawer panel's entries when it closes |
| `--config PATH` | `PROGDIR:dock.config` | Complete versioned composition and dock position |
| `--pins PATH` | `PROGDIR:dock.pins` | Legacy pin file imported only when configuration is missing |
| `--svg PATH` | `PROGDIR:assets/folder.svg` | Default folder imagery for Tools and groups |
| `--svg-library PATH` | `PROGDIR:libs/svgicon.library` | Explicit renderer library path |
| `--corner N` | from `dock.prefs`, else `5` | Corner radius 0–8 px for this run only; 0 gives square corners |
| `--prefs PATH` | `PROGDIR:dock.prefs` | Preferences file written by the Preferences window |

The versioned configuration stores root order, groups and children, names, icon
overrides and dock position. Icon size remains a command-line setting. Accepted
composition edits are saved before they replace the visible model. Moving the
dock saves on release; a failed position save restores its previous position.

When no configuration exists, defaults and any legacy one-path-per-line pins
are imported. The next successful edit or position save writes the complete
configuration. Once present, that configuration supersedes legacy pins. The
legacy pin file is never rewritten by the application.

Malformed, unreadable or future-version configuration files are preserved and
editing is disabled for that run. If the primary is missing but a valid `.old`
backup exists, it is loaded read-only. An invalid legacy import also disables
editing. A saved root layout that exceeds the current screen width is preserved;
the application shows defaults read-only. Keep backups before repairing files;
there is no repair or force-overwrite command in the UI.

The format is strict text beginning with `DOCKBAR` and version `1`, followed by
position, root items and group records, and a final `END` record. Fields preserve
spaces and UTF-8 bytes; tabs, newlines and other control characters are rejected
inside names and paths. The complete file is validated before loading.

## Icon rendering

SVG support is optional at runtime. An unavailable application-local renderer,
or missing/invalid SVG, falls back to a native icon or placeholder. SVG is
rasterized directly at its requested size.

The installed icon library decodes native icons. Original ARGB and indexed
palette images use application-local Lanczos3 filtering in linear light with
premultiplied-alpha intermediates. Normal and selected states are cached
separately, preserving aspect ratio without enlargement. Pure planar icons
retain native drawing and system nearest-sample downscaling. The opaque dock
and group panel blend icon alpha against their own backgrounds; the desktop
does not show through their surfaces. Rounded window corners use stock Intuition
shape regions. The region keeps whole pixels,
so the outer edge cannot be smoothed against the desktop; a small default
radius keeps the steps unobtrusive. The 1 px border and the hover highlights are
smoothed against the dock's own background. The bar and group grid retain
custom drawing; the item editor uses native Zune layout, strings and buttons.

## Checks and verified scope

`./scripts/test-host.sh` builds and runs model, composition/configuration,
interaction, resampler and window-frame tests in a temporary directory, then removes it.
Coverage includes normalization, duplicates, capacities, group movement and
atomic failed mutations, strict configuration and backup handling, position
clamping, drag thresholds and dwell/cancellation, reduction energy, alpha
edges and source ownership. Host tests do not establish native GUI or
Workbench behavior.

The finite native probes and tests can be run separately:

```text
native_drop_probe RAM:dock-drop-probe.log
test_composition RAM:dock-composition-test.config
test_interaction
test_resample
```

`backdrop_probe LEFT TOP WIDTH HEIGHT RRGGBB SECONDS` opens a plain backdrop
window of one colour, for judging the dock on light and dark ground without
changing the Wanderer wallpaper.

Give `test_composition` a disposable path: it deliberately replaces that file
and its backup while exercising persistence. Redirect console output to a
chosen log when collecting native test results. The drop probe uses real Exec
ports and DOS locks with synthetic AppMessages; actual Wanderer dragging is a
separate integration check.

On 2026-10-13 the application compiled and linked against the x86_64 ABIv11
2026.09 SDK and ran on a stock QEMU guest. Scoped native checks confirmed
application drops, duplicate alias detection, launches, inactive hover, live
clock, SVG rendering at 48 and 64 px, missing/invalid SVG and missing-library
fallbacks, and clean exit. The native drop and resampler probes passed.

The groups/position revision additionally exercised creating groups with the
dwell gesture, editing a group name, extracting a child, reordering root and
grid items, restoring composition and a moved dock position across application
restarts, a real Wanderer Clock drop into an open group, duplicate rejection,
launching Clock from the grid, a custom SVG group icon, child removal and
creation of a second group, moving between groups, deleting an empty group,
right-edge clamping and a panel below the dock at the top of the screen. A read-only
configuration destination rejected grouping without changing the model and rolled
back a failed position save. Native composition/gesture tests and the updated
AppMessage-coordinate probe passed. These checks do not establish every possible
interaction or icon format.

The visual/Zune revision additionally exercised rounded dock/group windows,
the Zune item editor (Save, Cancel, Escape, close gadget and Remove), saved
names, SVG and explicit `.info` overrides, automatic group previews after clearing
the override, a real Wanderer drop into the compact grid, and moved-position
restoration after restarting the application. The bar/grid use custom Intuition
rendering; this revision starts Zune adoption with the item editor.

The corner revision reduced the radius from 12 px to a shared 5 px with a 1 px
border. Both 5 px and square corners were shown on the stock wallpaper and on
light and dark backdrop windows, with the group panel open and a clean exit.

Running-app discovery and further visual effects remain future work. Mainline ABIv1, Raspberry Pi, macAROS and i386 have
not been built or run by this project.

## Native icon quality verification

The application-local filter was exercised on stock x86_64 ABIv11 on
2026-10-13. Shell, MultiView, Prefs and Clock were compared against stock
ScaleBox at 32/40/48/64 px, normal and selected, over light and dark backgrounds.
Their source sizes were 64×64, 64×64, 82×60 and 68×68 respectively. All used
ARGB; indexed and pure planar paths have no dedicated native visual coverage
from that comparison. Reduced stock selected images showed horizontal-strip
corruption; independent dock caches displayed both states correctly in the
tested matrix. The compact 40 px dock and pressed state were also checked.
Host resampler and composition tests passed ASan/UBSan.

To repeat the finite 90-second visual comparison:

```text
icon_scaling_probe RAM:icons-normal.log 0 0
icon_scaling_probe RAM:icons-selected.log 1 0
icon_scaling_probe RAM:icons-light.log 0 1
icon_scaling_probe RAM:icons-selected-light.log 1 1
```

Escape or window close exits early. Each column contains stock `old` and
application-local `new` images; arguments select pressed state and light
background. The reference avoids enlargement. Inspect screenshots as well as
logs: `failed_cells=0` establishes successful loading, not visual quality.

## Experimental external applets

The grip menu now offers **Add widget...**. See [Applets and Weather](applets.md) for package installation, per-instance settings, provider builds and protocol limits. Configuration version 7 carries package and instance identity; earlier configuration versions remain readable. Weather is built and distributed independently of the host.


## Bundled Weather build

The standard ABIv11 `scripts/build.sh` also builds WeatherProvider with the same
`AROS_CC` and `AROS_SDK` and installs a complete `Widgets/org.aros.weather` beside
Dockbar. The SDK must supply curl, OpenSSL, zstd and zlib static link dependencies.
`WEATHER_TARGET_LIBS` overrides the target link libraries when needed. A provider
build failure fails the whole build. `scripts/package.sh` refuses an ABIv11 archive
without the bundled provider/metadata; copying only the Dockbar executable is not
a complete installation. Deploy the whole `build/x86_64-abiv11/package` drawer.
Existing user configuration and WidgetState must be preserved during updates.
Weather is available through Add applet; existing dock compositions are not changed.

Public builds ship the original SVG placeholders. The purchased animated pack
lives in the widget's own untracked drawer `applets/org.aros.weather/purchased/`
(ignored by Git; the extracted pack with Day/, Night/ and LICENSE.txt), and the
bundling step uses it whenever it is there. `WEATHER_ASSETS_DIR` overrides that
location (set but empty: placeholders only, as the tests do), and an ignored
`local-weather-assets.json` (see `local-weather-assets.example.json`) can name
another one; `WEATHER_ASSETS_CONFIG` overrides that file's path. A configured
incomplete or missing pack fails the build instead of silently dropping artwork.
Without the drawer or any configuration the build uses the SVG placeholders and
says so. The nine 128px
day/night sprite pairs go only into generated packages, and only inside
`assets/sprites.pak`, a private pack that Dockbar alone reads
(`scripts/dockpak.py`, `src/pak.c`): the pack's licence allows the icons inside
the program but not as loose files that others could extract and reuse. The
licence text itself is not shipped; NOTICES and `assets/README-icons.txt` name
the author and the restriction. Never commit the purchased sheets, the pack or
the local path.

Inputs of a release build (`DOCK_TARGET=x86_64-abiv11` or `x86_64-v1`;
`scripts/target.sh` lists the targets, and the build stops when `AROS_CC` and
`AROS_SDK` are not both of the target's ABI):

| input | needed for | when missing |
|---|---|---|
| `DOCK_TARGET` | everything: compiler/SDK ABI check, `build/<target>/`, widget provider name, package name | build stops |
| `AROS_CC`, `AROS_SDK`: ABIv11 GCC 10.5 with SDK-202609, or mainline GCC 10.5 with a mainline SDK of 2026-10-24 or later (window compositor) and contrib curl/OpenSSL; both with curl/OpenSSL/zstd/zlib | everything | build stops |
| `SVGICON_INCLUDE`, `SVGICON_LIBRARY` | SVG icons (`none` for no renderer) | build stops |
| `SVGICON_REVISION`, `SVGICON_LICENSE_FILE` | NOTICES of the SVG package | `package.sh` stops (use `DOCK_SVG=0` for .info-only only) |
| `applets/org.aros.weather/purchased/` (or `WEATHER_ASSETS_DIR`, `local-weather-assets.json`) | animated weather icons | SVG placeholders; a configured but incomplete pack stops the build |
| bundled Weather in `build/<target>/package/Widgets` | x86_64 packages | `package.sh` refuses the archive |

Other targets: `DOCK_TARGET=aarch64-raspi-v1` (Pi) builds no Weather; `build-macaros.sh` never does. Host/core text/ABI checks do
not establish native Weather runtime or network validation. Macaros has no Weather
provider variant yet and its separate build does not bundle this ABIv11 executable.
