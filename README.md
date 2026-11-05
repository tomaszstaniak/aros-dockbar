# AROS Dockbar

An Amiga-style dock for your applications, files and widgets.

![Dockbar with the Weather card open on AROS One](docs/screenshots/weather-aros-one.png)

*Your launchers and the forecast, together on the desktop.*

## Get Dockbar

[Download Dockbar 0.5.2](https://github.com/tomaszstaniak/aros-dockbar/releases/tag/v0.5.2)
for mainline AROS on PC, AROS One / ABIv11 on PC, or Raspberry Pi.
Every package includes SVG icons and Weather.

Unpack the archive and open **Dockbar**. Keep the whole Dockbar drawer together.

You can also install it with [arospkg](https://github.com/tomaszstaniak/arospkg):

```sh
apkg update
apkg install dockbar
```

## Make it yours

Drop an application from Wanderer onto the dock to add it. Click to launch,
or drag items to rearrange them. Hold one application over another to create
a group. Drawers can open as a panel, so their contents are always close by.

Add a clock or Weather, choose your icons, and arrange docks along your desktop.
Use a background, transparency, or just the icons.

Drag the grip to move a dock. Click it for Preferences and the menu, or
Shift-click an item to edit it. Removing a shortcut leaves the original
application or file untouched.

## A closer look

| Weather | Drawers |
| --- | --- |
| ![Light Weather card on mainline AROS](docs/screenshots/weather-mainline.png) | ![Preferences drawer opened as an icon grid](docs/screenshots/drawer-panel.png) |
| Current conditions, an hourly forecast and the next five days. | Browse a drawer, open its files, or continue in Wanderer. |

Weather screenshots show 0.5.2; the drawer panel is from 0.5-test12.

## Help and development

- **ReadMe** in the download covers setup and starting Dockbar with the system.
- [Release notes](https://github.com/tomaszstaniak/aros-dockbar/releases/tag/v0.5.2) cover changes and system requirements.
- [Development guide](docs/development.md) covers building, configuration and runtime options.
- [SVG dependency](docs/svg-dependency.md) documents the renderer and packaging.

Dockbar is MIT-licensed. Libraries and Weather artwork have their own terms,
listed in the package's **NOTICES**. Public builds use placeholder Weather icons;
the downloads include licensed artwork.
