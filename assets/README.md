# Project assets

The three SVG files in `runtime/` are original artwork created for AROS
Dockbar. These files are the editable sources and runtime inputs; there is no
separate raster export step.

| Source | Design | Runtime use |
|---|---|---|
| `runtime/folder.svg` | Blue collection folder | Included SVG for item icons |
| `runtime/terminal.svg` | Terminal window | Included alternate SVG for testing |
| `runtime/compass.svg` | Compass explorer | Included alternate SVG for testing |

The build copies them into the package's `assets/` drawer. They are shown only
where the user chooses one as an item's icon (Edit, Icon); items without a chosen
icon always show their own `.info` icon, with or without `svgicon.library`.

Each uses a 96×96 view box and transparent margins. There are no imported icon
fragments, raster images, fonts, scripts or external resources. Host renderer
checks covered 24, 48, 72 and 128 px; native application verification currently
covers 48 px. Native application `.info` icons are loaded from the target system
and are not redistributed in this asset directory.

No project-wide license has yet been selected for these original assets.
Dependency notices are described in [the SVG guide](../docs/svg-dependency.md).

## Tabler UI controls

`tabler/` contains five unmodified outline SVGs from Tabler Icons, by Paweł Kuna,
under the MIT license in `tabler/LICENSE`. The upstream revision and selected file
names are recorded in `tabler/provenance.json`. These licensed assets are separate
from the original artwork above. All distributions copy the complete notice and
license; this does not select or change the license of Dockbar itself.

The build rasterises these SVGs into 20 px coverage masks
(`scripts/tabler-masks.py`, compiled into Dockbar), and the card tints them to
the dock theme. So the controls look the same in every package, with or without
`svgicon.library`, and nothing is read from disk when a card is drawn. Source
SVG geometry is unchanged; the SVGs, the license and the provenance file still
ship in `assets/tabler/` as the record of where the pictures come from.
