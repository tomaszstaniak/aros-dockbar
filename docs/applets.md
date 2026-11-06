---
type: guide
updated: 2026-11-06
---

# Widgets and Weather (experimental API 1 and 2)

A widget is a directory containing metadata, a view binding, settings, assets
and an architecture/ABI-specific provider executable. Dockbar scans packages
without running them. A configured active instance starts its provider only
when data are needed. Weather is the first package using this path; the existing
clock remains built into Dockbar.

This API is experimental. Screenbar integration, a market, automatic updates and
additional view components are future work. The file-worker adapter is not a
stable binary plugin ABI.

## Install and use

The standard builds for PC v1, PC ABIv11 and Pi include Weather in `Widgets` beside
Dockbar. Deploy the complete package drawer; copying only the executable omits
bundled widgets and other assets. Build/asset configuration is described in
[development](development.md#bundled-weather-build). Separate widget archives
remain supported for installing or updating independently.

Unpack the Weather archive and copy the `org.aros.weather` directory into
`Widgets` beside Dockbar. Keep the package directory intact. Open the grip menu,
choose **Add widget...**, **Rescan**, select Weather and **Add**. The settings
window opens after the item has been saved.

Search for a city, select a result and click **Use selected city**. Alternatively,
enter a location name, latitude and longitude directly. Choose Celsius or
Fahrenheit, wind units, and a refresh interval of 15–60 minutes. **Save** applies
only this instance. Adding another Weather item creates independent settings.
No network request starts until a location is configured.

The tile shows the condition icon and temperature; its tooltip identifies the location. Click it for current conditions, the next 24 forecast hours and five daily forecasts. **Refresh** requests new data (30-second manual
cooldown); **Settings** edits that instance. Escape, Close or another click on
the tile closes the panel. Right-click the tile to open its settings; **Remove
widget** removes the instance for good: the shortcut, its settings and its
cached forecast.

A `*` on a tile and **stale** in the panel identify retained cached data.
Unavailable measurements display `--`. A network or certificate error retains
the last good data. Forecast-local time labels come from the API and do not
use the workstation timezone. Forecast values are model estimates.

Weather data by [Open-Meteo](https://open-meteo.com/),
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The free endpoint
has its own service terms; this package includes no commercial subscription
or API key. Forecast and city search use HTTPS with certificate and hostname
verification enabled.

On AROS, start the system TCP/IP stack before requesting weather. The provider
verifies servers with the CA bundle shipped in the widget (`certs/ca-bundle.crt`,
Mozilla's store), found through the package directory Dockbar names in the
request, wherever the Dockbar drawer is. If that fails it tries the file named
by `CURL_CA_BUNDLE` (when set), then the system's
`ENV:SYS/Certificates/ca-bundle.crt`. An implausibly early system date produces a hint to check the clock; other failures give the verification code, and the message
says when the bundled file could not be read. Each request is logged in
`WidgetState/weather.log` (bundled file found or not, curl code, verification
result, CA file used). Verification is never turned off.
The distributed package's `licenses/` directory identifies its linked libraries.

## Directories and compatibility

Defaults:

```text
PROGDIR:Widgets/<package-id>/
PROGDIR:WidgetState/<package-id>/<instance-id>/settings.json
PROGDIR:WidgetState/<package-id>/<instance-id>/last-good.json
```

Removing a widget (**Remove widget** in its settings, or Remove in the item
editor or in **Edit items...**, also for a group holding it) deletes that
instance's drawer, but only after the configuration without the instance
was saved, and only once no provider runs for it any more, so a late answer
cannot write the cache back. If any item still uses the instance (also a
copy of it that has no settings of its own yet), or Preferences is open and
could still put a set with it back, nothing is deleted. Exactly the files
Dockbar writes there are deleted: `settings.json` and `last-good.json`, each
with its `.new` and `.old`; then the drawer, if it is empty. An unknown
file keeps the drawer. Failures are written to the log; a failed save of
the configuration leaves the data as it was. Moving, rebuilding a dock, a
screen change, Quit and Preferences Apply/Cancel never delete widget data.
A removal still waiting when Dockbar quits keeps its data. The shared
`weather.log`, the session drawers and the installed widget in
`PROGDIR:Widgets` are never touched. Drawers left by instances removed in
releases before 0.5-test9 stay where they are.

Widgets were called applets before 0.5-test3. When `PROGDIR:Widgets` (or
`PROGDIR:WidgetState`) does not exist and `PROGDIR:Applets` (`AppletState`)
from an earlier build does, the old drawer is used.

`--widgets PATH` and `--widget-state PATH` (old names `--applets`, `--applet-state`) override these roots. The state root
must be writable and its parent must already exist. Package contents are not
modified while running. `.old` settings are recovered only when the primary is
missing; an invalid existing primary is preserved. Save in Settings restores
a recovered primary. Cache includes the exact settings and snapshot in one
transaction, preventing another location's data from being reused.

Configuration version 7 adds `DOCK_APPLET`. Older configurations remain readable;
older Dockbar versions must leave a version-7 configuration untouched. Package ID
and instance ID survive reordering and moves. Duplicating a dock creates separate
widget IDs and copies settings on first activation. Very long externally chosen
instance IDs can prevent further dock duplication; generated IDs fit normally.

Providers are selected by exact target: the release supplies `x86_64-v1`, `x86_64-abiv11` and `aarch64-raspi-v1` variants. A host must not load a provider built for another target. Unavailable packages remain visible in the
picker with a reason. Duplicate IDs disable both conflicting packages. The
catalog is capped at 64 packages and reports overflow. Rescan reconciles existing
instances after their outstanding worker finishes.

## Author a package

Weather sources live in `applets/org.aros.weather`; a small provider without GUI
or networking dependencies lives in `examples/applets/org.aros.sample`.
Source and release repositories are independent of package identity.

Required manifest keys:

```json
{
  "schema": 1,
  "id": "org.example.sample",
  "name": "Sample",
  "version": "0.1.0",
  "description": "An example widget.",
  "author": "Example author",
  "license": "MIT",
  "api": {"min": 1, "max": 1},
  "hosts": ["dockbar"],
  "capabilities": [],
  "settings": "settings.json",
  "views": "views.json",
  "refreshOperation": "update",
  "entryPoints": {"x86_64-abiv11": "bin/x86_64-abiv11/SampleProvider"}
}
```

Include `LICENSE`. Relative paths cannot contain device names, absolute paths,
parent traversal or symlinks. Native executables are trusted code; capability
metadata and separate processes do not provide memory isolation on AROS.

API 1's view binding is intentionally small:

```json
{
  "schema": 1,
  "compact": {"primary": "value", "secondary": "caption", "tooltip": "summary"},
  "expanded": {"component": "rows", "chart": true}
}
```

The compact binding names snapshot fields. Expanded rows contain `label`,
`value`, optional `icon`, and optional numeric `plot`; values with `plot` form
the temperature-style line chart. The loader does not know weather codes or
Open-Meteo fields. Other required expanded components are unavailable in API 1.

`settings.json` declares revision 1 and at most eight fields. Supported types:
`text`, `number`, `integer`, `choice`. Each has `key`, `label`, optional `default`;
number/integer fields may declare `min`/`max`, text may declare `maxLength`
(up to 159 bytes), and choices declare up to eight `choices`. The common optional
`refreshMinutes` setting selects the scheduling interval. A schema can declare
a search operation and map its result label/coordinates to named fields; Weather
uses this for geocoding. A provider must also validate settings itself.

## Action widgets (API 2)

A package that declares `"api": {"min": 2, ...}` needs Dockbar 0.6.0 or newer;
older versions list it as "Unsupported widget API". Dockbar 0.6.0 also keeps
loading API 1 packages and asks them in API 1.

API 2 adds two manifest keys:

```json
"refresh": "none",
"actions": [{"id": "area", "label": "Capture an area"},
            {"id": "screen", "label": "Capture the screen"}]
```

- `refresh` is `schedule` (the API 1 behaviour, default) or `none`. With
  `none` Dockbar never starts the provider by itself: not at start-up, on a
  schedule, after a failure, after Rescan or after a settings change. It runs
  only when the user chooses one of the actions.
- `actions` holds 1 to 8 entries; `id` follows the package ID rules, `label`
  is up to 39 bytes. `refresh: none` requires actions.

The tile of such a widget shows only its icon (from `assets/conditions.json`,
the `unknown` entry when there is no answer yet). A click opens a small
window with one button per action, the last answer's `message`, Settings
when the settings form has fields, and Close. Choosing an action closes the
window first, then Dockbar writes a request with `"operation": "action"` and
`"action": "<id>"` and starts the provider once. One action per widget runs
at a time; a failed action is not retried. The answer is read as for API 1;
its `message` appears in the window and the tooltip. A provider must answer
within the normal timeout: long work, such as a program with its own window,
is started by the provider as a separate program.

## Installing a widget from another program

Dockbar 0.6.0 and later have a public message port, `Dockbar.Widgets`. The
message format and constants are in
[include/dockbar/widgets.h](../include/dockbar/widgets.h) (MIT); copy the
header into your project. The port's existence is the capability check.
Never send messages to the port named `Dockbar`: it only opens Preferences
and is also present in older versions.

- **Query** fills in the Dockbar version, the widget API range, the build
  variant (`x86_64-abiv11`, `x86_64-v1`, `aarch64-raspi-v1`) and the widget
  drawer in use, which may have been changed with `--widgets`.
- **Install** names a package drawer. Dockbar loads it like a package in
  its catalogue; a package for another architecture or API is refused with
  the same reason the picker would show. The copy is made beside the
  installed packages and put in place only when complete. An installed
  package with the same ID is replaced in its drawer (an update); if the old
  copy cannot be deleted yet because something still uses it, it is moved
  aside under a dot name and deleted by a later install. Settings, caches and
  the dock configuration are not touched. While a provider of that package
  runs, the answer is "busy"; try again.
- With the offer flag, a newly installed package that is on no dock yet is
  offered: "Add it to the dock?", with a dock choice when there are several.
  The offer appears after the answer, so the installing program does not wait
  for the user. While Preferences or an item editor is open there is no
  offer; the answer text points to **Add widget**.

Results: 0 installed, 1 updated, 10 bad request, 11 refused (not a package for
this Dockbar), 12 busy, 13 copy failed (the installed package is kept).

## Provider protocol

The host launches `Provider "ABSOLUTE-REQUEST-PATH"` asynchronously. It passes
only a quoted executable and request-file path through the DOS Shell. All
settings and city names are serialized inside the request, never interpolated
into a command line.

The request contains `api` (1, or 2 for an API 2 package), `package`, `packageRoot` (the installed package
directory, resolved to a full path when the request is written; if that
fails, Dockbar logs it and passes the path as configured, and Weather then
also tries its own program drawer),
`instance`, `session`, positive `generation`, `operation`, for API 2 actions
`action`, `query` and a `settings` object. A provider finds its own files from `packageRoot`, not from
the drawer or current directory of the process the Shell runs it in. The worker performs
one operation, serializes a response into `<request>.tmp`, closes it, then
renames it to the unique previously absent `<request>.done`. It exits after
publication. Do not replace a result belonging to another request.

A response carries `api` (1 or 2), repeats the four identity fields and includes `status` (`ready`,
`error`, `loading`, `stale`), `updated` and `sourceTime` UTC seconds, `fingerprint`,
`title`, optional `icon`, `message`, `attribution`, `url`, a `fields` object and
optional `rows`. Each field has `value` (string, finite number or null) and an
optional `unit`. `src/applets/snapshot.h` defines capacities; its writer and
parser are shared by the host, Weather and sample provider.

The host rejects wrong package/instance/session/generation and preserves its
last good snapshot on errors. Settings edits invalidate the old generation.
The host retains at most four outstanding workers, one per instance. An
instance removed from every dock is freed once no window shows it and no
request of it is out; moving a widget keeps the same instance and its data. Scheduling
uses the GetUpTime-based elapsed clock, with a guarded fallback when timer.device is unavailable. Calendar time is used separately to label acquisitions.
Failures back off 1, 5 and 15 minutes.

A request that is not answered within 60 seconds is ended: the instance is told
(a search gets "No answer from the provider", a refresh counts as a failure and
backs off), and the provider process gets Ctrl-C. The process is found among
the Shell processes by its arguments, which hold the request file's unique path
(SystemTags replaces a process name given with NP_Name), so Dockbar can see
whether it still runs; the slot stays taken until that process is gone, so a retry never
runs next to it. A late answer is discarded when it arrives; it does not free the slot,
because the provider may still be working after writing it. A provider that
ignores Ctrl-C and never ends keeps its slot until Dockbar quits. Weather checks
Ctrl-C during its transfers. Running workers own their file data and process-local memory;
they hold no pointers into Dockbar. Abandoned session directories are retained;
remove them manually only after Dockbar and all its provider processes exit.
No automatic old-session pruning is enabled yet.

Limits: metadata 64 KiB each, nesting depth 16, 16,384 JSON tokens, forecast HTTP
512 KiB, geocoding HTTP 64 KiB, snapshot 128 KiB, 24 fields and 64 rows.
HTTPS connect timeout is 10 seconds and total timeout is 25 seconds.

## Replace weather artwork

`assets/conditions.json` maps semantic condition IDs to local resource filenames.
The provider emits keys such as `clear-day`, `clear-night`, `rain`, `snow` and
`thunderstorm`. The host treats them as opaque IDs. Replacing artwork or changing
the mapping does not require a provider rebuild. Unknown or unavailable images
use the host fallback. The supplied SVG placeholders are original MIT artwork;
they use the existing optional SVG renderer. Without it, the host placeholder
still leaves temperature and city readable.

Animated sprite sheets are supported as described below. Keep semantic condition IDs stable when replacing artwork.

## Build and package

Build the host-only provider with `./scripts/build-weather.sh`. Set `WEATHER_CC`,
`WEATHER_CFLAGS`, `WEATHER_LIBS`, `WEATHER_BUILD_DIR` for a target build. For the
ABIv11 2026.09 SDK, static link inputs are `-lcurl -lssl -lcrypto -lzstd
-lz.static -lm`. The host build requires curl development headers and libraries; its default link flags are `-lcurl -lm`.

Package a built variant independently:

```sh
python3 scripts/package-applet.py applets/org.aros.weather \
  --binary build/weather-v11/bin/WeatherProvider \
  --target x86_64-abiv11 --output dist/applets
```

The ZIP contains the package folder, metadata, assets, license notices and the
selected provider. It excludes provider sources and mutable state. The script
emits SHA-256 and refuses to overwrite an existing release with different bytes.
This is the same artifact a future market can index. Manual installation needs
no catalog service or account.

`./scripts/test-host.sh` includes `./scripts/test-applets.sh`. New parser, cache,
settings, package and provider tests run under ASan/UBSan on the host. A successful
host HTTPS fetch does not establish AROS networking; native results require their
own ABI, SDK, machine and run evidence. Additional targets and artwork formats
must be verified independently.

### Sprite sheets

A condition may include optional `sprite` metadata beside its static `resource`:

```json
{
  "resource": "clear-day.svg",
  "sprite": {
    "resource": "clear-sky-sprite.png",
    "frameWidth": 128,
    "frameHeight": 128,
    "columns": 11,
    "frames": 120,
    "fps": 30
  }
}
```

Frames run left to right, then top to bottom; unused cells at the end are ignored.
The host decodes the PNG once, converts it to straight ARGB and resizes each frame
to 32×32 using the existing icon resampler. It keeps only these small frames.
Playback uses monotonic elapsed time, skips missed frames and loops indefinitely.
The static resource remains the fallback for invalid or unavailable sprites.
Bounds: 4096×4096 sheet, 512×512 frame, 32 columns, 256 frames and 60 FPS.
Configured FPS is a target; actual playback depends on the host and rendering load.
The current Macaros build disables PNG decoding and uses the static resource.

Compact widgets reserve space for their primary value and icon. Secondary information is available in the tooltip and details panel.
This stable compact width avoids resizing the dock whenever a value changes.

Purchased artwork is supplied separately at build time and ships only inside
`assets/sprites.pak` (see development.md). The public weather sources retain the
original SVG placeholders and do not require that pack.
Run `scripts/test-sprite.sh` for generated-fixture decoder/timing tests; it requires
Python 3 and libpng development files (`libpng-config`, overridable by PNG_CONFIG).

### Dock card panels

`expanded.component: "cards"` selects a borderless panel anchored beside the widget,
using the dock theme and rounded frame. A second click, Escape, close control or
click outside dismisses it. The settings control opens the existing schema-driven
settings editor. Packages with the `rows` component retain their generic view.

The card layout binds fields through `title`, `value`, `subtitle` and `range`.
`metrics` contains up to two `{ "label": "...", "field": "..." }` bindings.
`strip` and `list` select row sections; `stripTitle` and `listTitle` supply their
headings. Six strip items are shown at once, with previous/next controls; the list
shows five rows. Rows may add optional `section`, `caption` and `detail` strings to
the existing label/value/icon/plot contract. Older snapshots still parse; the rich
sections become available after a provider refresh. API version remains 1.

The current weather package supplies local hour labels, named weekdays, readable
condition descriptions and separate rain probabilities. Display metadata stays in
its views.json; the host card renderer contains no weather provider or WMO logic.
The main image uses the optional sprite animation; row icons can use sprite frames, with static resources as a fallback.
FreeType uses the system Vera Sans file when available, otherwise native text.
The initial panel is 464×488 and requires sufficient screen space; smaller screens
retain the generic details fallback. The panel buffers complete frames to avoid
flicker and closes before screen detach or dock rebuild. Settings remain native Zune.

The card panel uses shared 24px content margins, centered strip columns, right-aligned list values, 32px row icons and an 84px hero. Header sliders open settings; the circular footer arrow refreshes. Pending requests show a compact Updating status.

### Host data transitions

Dockbar owns a 400 ms smoothstep crossfade for changed widget presentations. Providers
publish ordinary snapshots; no provider animation logic or protocol change is
required. Compact views compare their primary display string and icon. Card panels
compare bound fields and the currently visible row sections. Timestamp, generation,
connection-message and status-only updates do not trigger a transition. Page changes
remain direct user actions.

The host captures its own last-presented pixels and blends toward freshly rendered
content in premultiplied alpha. No desktop capture is needed. The outgoing state is
a still image during the brief transition; the incoming sprite keeps animating.
A newer update starts from the last presented blend, avoiding a jump. Allocation or
readback failure falls back to a direct update. Closing/rebuilding drops the cache.

`--no-transitions` disables these fades globally; sprite playback is separate and
is not disabled by that flag. The preference currently exists as a host CLI option.
The test-only `tests/applet_transition_probe.c` alternates two controlled snapshots
on request for recordings; it is not included in the real weather package.
