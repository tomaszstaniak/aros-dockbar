# AROS Dockbar

A development prototype of an Amiga-style dock for stock x86_64 ABIv11. It
combines application launchers, curated groups, a Tools drawer, separators,
hover labels and a live clock in a movable horizontal bar. The default icon
box is 40 px and the bar is 56 px high.

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

Composition and position are saved in `PROGDIR:dock.config` by default. See
[usage and configuration](docs/development.md) for controls, migration from
legacy pins, and recovery behavior.

Native ARGB and indexed icons use an application-local alpha-aware Lanczos
filter with separate normal and selected caches. Small sources are not
enlarged. Pure planar icons retain system drawing and downscaling. SVG icons
are rendered at the requested size. The bar and grid work on a stock system with
no patched system library. Their rounded outlines use Intuition shape regions; the item editor uses stock Zune controls and follows the system theme.

## Build and try

Configure the matching compiler, SDK and separate SVG development inputs:

```sh
./scripts/test-host.sh
DOCK_TARGET=x86_64-abiv11 ./scripts/build.sh   # or x86_64-v1 (mainline)
```

The local test package is written to `build/<target>/package/`; the build
stops when the compiler and SDK are not of the target's ABI. The
[development guide](docs/development.md) lists environment variables and runtime
options. Building requires SVG headers and a library; missing SVG support at
runtime falls back to a native icon or placeholder.

## Status and targets

This is not a public release. On 2026-10-13 it was built and run on stock
x86_64 ABIv11 2026.09 under QEMU. Scoped checks covered native and SVG icons,
normal/selected icon filtering, Wanderer drops and duplicates, application
launching, group creation and renaming, root/grid reordering, extracting a
group child, custom SVG group icons, shortcut removal, and composition and
moved-position restoration across application restarts. The visual iteration
also exercised rounded windows and the Zune editor, including `.info`/SVG
overrides, cancellation and removal. See the development
guide for the tested formats and remaining limits.

There is one horizontal dock, at most eight one-level groups, and no nested
groups. Multiple docks, vertical orientation, position locking and running-app
discovery are not implemented. Mainline x86_64 (ABIv1) is a second
target since 0.5-test12: on a system with window composition (mainline from
2026-10-24) see-through docks are truly transparent, elsewhere they use a
picture of the desktop as on ABIv11. Raspberry Pi, macAROS and i386 remain
unverified targets.

## Documentation

- [Development and usage](docs/development.md)
- [SVG dependency and packaging scope](docs/svg-dependency.md)
- [Original runtime assets](assets/README.md)

Project code and icons are original and under the MIT license (`LICENSE`).
Bundled dependencies and artwork keep their own terms, listed in the
package's NOTICES; see the SVG dependency notes for the separate renderer's
provenance.
