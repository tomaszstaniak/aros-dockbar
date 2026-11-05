# AROS Dockbar

An Amiga-style dock for AROS: launchers, groups, a Tools drawer, separators,
hover labels, clocks, a Weather widget and SVG icons in a movable bar.
Download the packages from the
[releases page](https://github.com/tomaszstaniak/aros-dockbar/releases).
Version 0.5.1 has packages for x86_64 mainline AROS (ABIv1, PC), x86_64
ABIv11 (for example AROS One, PC) and AROS for Raspberry Pi (aarch64). Each
target has a package with SVG icons and an `-info-only` package without the
SVG renderer; both include Weather.

Click a launcher to open it, or drop an application icon from Wanderer to pin
it. To create a group, drag an application onto the center of another dock
application, pause for the grouping highlight, and release. Groups show
miniature previews and open a launch grid. Drag items to reorder them or move
them into and out of groups. The Tools item opens `SYS:Tools` in Wanderer; it
is a filesystem drawer, separate from curated groups.

Drag the left grip to move the dock; its position is saved when released.
Click the grip for its menu: Preferences (icon size, corner radius, position
lock), About and Quit. Only one copy runs at a time.
Shift-click an item, right-click it in the active dock or group panel, or use
the panel's **Edit** button to edit a group. The Zune/MUI editor changes the name and
optional `.info` or SVG icon. **Remove** deletes the shortcut; removing a group
also removes its contained shortcuts. Applications and files remain on disk.

Composition and position are saved in `PROGDIR:docks.config` by default. See
[usage and configuration](docs/development.md) for controls, migration from
legacy pins, and recovery behavior.

Native ARGB and indexed icons use an application-local alpha-aware Lanczos
filter with separate normal and selected caches. Small sources are not
enlarged. Pure planar icons retain system drawing and downscaling. SVG icons
are rendered at the requested size. The bar and grid work on a stock system with
no patched system library. Their rounded outlines use Intuition shape regions; the item editor uses stock Zune controls and follows the system theme.

## Build

Configure the matching compiler, SDK and, optionally, the separate SVG
renderer (`SVGICON_LIBRARY=none` builds without it):

```sh
./scripts/test-host.sh
DOCK_TARGET=x86_64-abiv11 ./scripts/build.sh   # or x86_64-v1, aarch64-raspi-v1
```

The test package is written to `build/<target>/package/`; the build stops
when the compiler and SDK are not of the target's ABI. The
[development guide](docs/development.md) lists the environment variables and
runtime options. The public sources contain SVG placeholders for the Weather
icons; the released packages use licensed artwork that is not in this
repository (see the development guide).

## Status

0.5.1 was tested under QEMU, not on real hardware: x86_64 mainline nightly
of 3 November 2026, AROS One 2026.09, and the Raspberry Pi nightly of
29 October 2026 (QEMU raspi3b) with and without SMP. The release notes list
the known limitations. There is one horizontal dock, at most eight one-level
groups and no nested groups.

## Documentation

- [Development and usage](docs/development.md)
- [SVG dependency and packaging scope](docs/svg-dependency.md)
- [Original runtime assets](assets/README.md)

Project code and icons are original and under the MIT license (`LICENSE`).
Bundled dependencies and artwork keep their own terms, listed in the
package's NOTICES; see the SVG dependency notes for the separate renderer's
provenance.
