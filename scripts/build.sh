#!/bin/sh
set -eu
: "${AROS_CC:?Set AROS_CC to the target compiler}"
: "${AROS_SDK:?Set AROS_SDK to the matching SDK sysroot}"
: "${SVGICON_INCLUDE:?Set SVGICON_INCLUDE to the generated svgicon headers}"
: "${SVGICON_LIBRARY:?Set SVGICON_LIBRARY to the matching svgicon.library, or none (no SVG icons)}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/scripts/target.sh"
case "$dock_target" in x86_64-*) dock_check_abi ;; esac
# The generated svgicon headers differ between the ABIs (library base access).
# When they come from an aros-svg-icons build, its BUILDINFO names the target.
inc_info=$(dirname "$(dirname "$SVGICON_INCLUDE")")/BUILDINFO
if [ -f "$inc_info" ]; then
  case "$(sed -n 's/^target=//p' "$inc_info")" in abiv11) inc_abi=v11 ;; v1|raspi) inc_abi=v1 ;; *) inc_abi=unknown ;; esac
  [ "$inc_abi" = "$dock_abi" ] || { echo "SVGICON_INCLUDE is from a build for another ABI ($inc_info)" >&2; exit 1; }
fi
if [ "$SVGICON_LIBRARY" != none ]; then
  dock_check_svgicon "$SVGICON_LIBRARY"
fi
output=$dock_build
variant=$dock_target
# Window composition (real per-pixel transparency) is compiled in when the
# SDK has the compositor's window tags (mainline from 2026-10-24); it is a
# requirement of the x86_64-v1 target, whose minimum system has them.
if grep -a -q 'define WA_AlphaSurface' "$AROS_SDK/include/intuition/intuition.h"; then
  present_src="$repo/src/present_compose.c"; present_def=-DDOCK_COMPOSE=1
else
  [ "$dock_target" != x86_64-v1 ] || { echo "x86_64-v1 needs an SDK with WA_AlphaSurface (mainline 2026-10-24 or later): $AROS_SDK" >&2; exit 1; }
  present_src="$repo/src/present_compose_none.c"; present_def=-DDOCK_COMPOSE=0
fi
mkdir -p "$output/package/assets" "$output/package/libs" "$output/dev"
# An endless self-call (0.5-test2 development: open_object calling itself)
# compiles silently with GCC 10.5, which has no -Winfinite-recursion; clang
# (the Pi toolchain) has it. Use it wherever it exists.
# The card controls are Tabler outline icons rasterised here, so that every
# package draws them alike, with or without svgicon.library.
python3 "$repo/scripts/tabler-masks.py" "$repo/assets/tabler" 20 "$output/gen/tabler_masks.h" \
 adjustments-horizontal x chevron-left chevron-right refresh
warn=
if echo 'int dock_probe;' | "$AROS_CC" --sysroot="$AROS_SDK" -Winfinite-recursion -Werror -x c -c -o /dev/null - 2>/dev/null; then
  warn=-Winfinite-recursion
fi
app_src="$repo/src/applets/view.c $repo/src/applets/cards_aros.c $repo/src/applets/cache.c $repo/src/applets/json.c $repo/src/applets/package.c $repo/src/applets/snapshot.c $repo/src/applets/instance.c $repo/src/applets/settings.c $repo/src/applets/worker_aros.c $repo/src/applets/purge.c $repo/src/applets/install.c $repo/src/applets/panel_aros.c $repo/src/sprite.c $repo/src/pak.c $repo/src/fade.c $repo/src/main.c $repo/src/migrate.c $repo/src/autostart.c $repo/src/autostart_plan.c $repo/src/elapsed.c $repo/src/elapsed_aros.c $repo/src/model.c $repo/src/config.c $repo/src/interaction.c $repo/src/group_panel.c $repo/src/chrome.c $repo/src/frame.c $repo/src/prefs.c $repo/src/prefs_window.c $repo/src/prefs_strings.c $repo/src/grip_menu.c $repo/src/about.c $repo/src/item_list.c $repo/src/layout.c $repo/src/magnify.c $repo/src/hover.c $repo/src/docks.c $repo/src/widget.c $repo/src/item_editor.c $repo/src/native_icons.c $repo/src/svg_icons.c $repo/src/resample.c $repo/src/snapshot.c $repo/src/theme.c $repo/src/textrender.c $repo/src/fontfile.c $repo/src/command.c $repo/src/drawer_panel.c $repo/src/dirlist.c $repo/src/icon_cache.c $repo/src/alpha.c $present_src"
# shellcheck disable=SC2086  # $app_src: paths without spaces (checked below)
case "$repo" in *" "*) echo "The repository path must not contain spaces" >&2; exit 1 ;; esac
# Packaged binaries carry no build-machine paths: source and SDK paths are
# mapped to fixed names, and the debug sections of the linked toolchain and
# SDK libraries are stripped below (scripts/package.sh refuses any left).
pmap="-ffile-prefix-map=$repo=dockbar -ffile-prefix-map=$AROS_SDK=sdk"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn $pmap \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/package/Dockbar" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
strip=${AROS_CC%gcc}strip; [ -x "$strip" ] || strip=${AROS_CC%cc}strip
[ -x "$strip" ] || { echo "No $strip to strip the debug sections" >&2; exit 1; }
"$strip" --strip-debug "$output/package/Dockbar"
# Test build that keeps a dock closed 4 s while taking its desktop picture.
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_TEST_MIN_HOLD_MS=4000u \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-holdtest" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
# Test build where every window is refused its alpha surface (fallback to the
# desktop picture), for targets that compose.
if [ "$present_def" = -DDOCK_COMPOSE=1 ]; then
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_TEST_COMPOSE_REFUSE=1 \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-refuse" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
fi
# Test build whose first screen attach finds no screen (waits for the open notice).
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_TEST_NO_SCREEN_AT_START=1 \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-noscreen" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
# Test builds whose resizes take 600 ms (clicks during one) and 1500 ms (the reopen fallback).
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_TEST_RESIZE_DELAY_MS=600u \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-resize600" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_TEST_RESIZE_DELAY_MS=1500u \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-resize1500" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_DRAGDIAG=1u \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-dragdiag1" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror $warn -DDOCK_DRAGDIAG=2u \
 -DDOCK_APPLET_VARIANT=\"$variant\" $present_def -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
 $app_src -I"$AROS_SDK/include/freetype" \
 -o "$output/dev/Dockbar-dragdiag2" -lworkbench -licon -lcybergraphics -lmuimaster "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/native_drop_probe.c" "$repo/src/native_icons.c" "$repo/src/elapsed.c" "$repo/src/elapsed_aros.c" "$repo/src/resample.c" \
 -o "$output/dev/native_drop_probe" -lworkbench -licon -lcybergraphics -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/icon_scaling_probe.c" "$repo/src/native_icons.c" "$repo/src/elapsed.c" "$repo/src/elapsed_aros.c" "$repo/src/resample.c" \
 -o "$output/dev/icon_scaling_probe" -lworkbench -licon -lcybergraphics -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/test_resample.c" "$repo/src/resample.c" \
 -o "$output/dev/test_resample" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/test_composition.c" "$repo/src/model.c" "$repo/src/config.c" \
 -o "$output/dev/test_composition"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/test_interaction.c" "$repo/src/interaction.c" \
 -o "$output/dev/test_interaction"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/backdrop_probe.c" -o "$output/dev/backdrop_probe" -lcybergraphics
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 -I"$repo/src" "$repo/tests/same_probe.c" "$repo/src/native_icons.c" "$repo/src/elapsed.c" "$repo/src/elapsed_aros.c" "$repo/src/resample.c" \
 -o "$output/dev/same_probe" -lworkbench -licon -lcybergraphics -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/slowpaint_probe.c" -o "$output/dev/slowpaint_probe" -lcybergraphics
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/mark_probe.c" -o "$output/dev/mark_probe" -lcybergraphics
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/later_probe.c" -o "$output/dev/later_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/wallrender_probe.c" "$repo/src/wallprefs.c" -I"$repo/src" -o "$output/dev/wallrender_probe" -lcybergraphics -ldatatypes
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/shape_probe.c" -o "$output/dev/shape_probe" -lcybergraphics
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/expose_probe.c" -o "$output/dev/expose_probe" -lcybergraphics
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/cover_probe.c" -o "$output/dev/cover_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/running_probe.c" -o "$output/dev/running_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror -I"$repo/src" -I"$AROS_SDK/include/freetype" \
 "$repo/tests/text_probe.c" "$repo/src/textrender.c" "$repo/src/fontfile.c" -o "$output/dev/text_probe" \
 -lcybergraphics "$AROS_SDK/lib/libfreetype2.static.a" "$AROS_SDK/lib/libpng_nostdio.a" "$AROS_SDK/lib/libz.static.a" -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/varargs_probe.c" -o "$output/dev/varargs_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/seq_probe.c" -o "$output/dev/seq_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/memmark.c" -o "$output/dev/memmark"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/cpuload.c" -o "$output/dev/cpuload"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/slow_provider.c" -o "$output/dev/slow_provider"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/wbargs_probe.c" -o "$output/dev/wbargs_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/exall_probe.c" -o "$output/dev/exall_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/mkfiles.c" -o "$output/dev/mkfiles"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/mkicon.c" -o "$output/dev/mkicon" -licon
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/drop_send.c" -o "$output/dev/drop_send" -lworkbench
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/click_burst.c" -o "$output/dev/click_burst"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/where_probe.c" -o "$output/dev/where_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/cmd_probe.c" -o "$output/dev/cmd_probe"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
 "$repo/tests/prefs_mockup.c" -o "$output/dev/prefs_mockup" -lmui
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/prefs_poll_probe.c" "$repo/src/prefs_window.c" "$repo/src/prefs_strings.c" "$repo/src/docks.c" "$repo/src/model.c" "$repo/src/config.c" "$repo/src/layout.c" "$repo/src/magnify.c" "$repo/src/hover.c" "$repo/src/frame.c" \
 -o "$output/dev/prefs_poll_probe" -lmui
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
 "$repo/tests/shadow_probe.c" -o "$output/dev/shadow_probe" -lcybergraphics -lm
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/applet_worker_probe.c" "$repo/src/applets/json.c" "$repo/src/applets/snapshot.c" \
 -o "$output/dev/applet_worker_probe" -lm
# A library left from an earlier build must not reach a package built now.
rm -f "$output/package/libs/svgicon.library" "$output/svgicon.BUILDINFO"
if [ "$SVGICON_LIBRARY" = none ]; then
 echo "No svgicon.library for this target: SVG icons fall back to .info icons" >&2
else
 cp "$SVGICON_LIBRARY" "$output/package/libs/svgicon.library"
 # Kept outside the package, for scripts/package.sh to check the revision.
 [ ! -f "$(dirname "$SVGICON_LIBRARY")/BUILDINFO" ] || cp "$(dirname "$SVGICON_LIBRARY")/BUILDINFO" "$output/svgicon.BUILDINFO"
fi
cp "$repo"/assets/runtime/*.svg "$output/package/assets/"
printf 'Application: %s\nDeveloper probes: %s\n' "$output/package" "$output/dev"

mkdir -p "$output/package/assets/tabler"
cp "$repo"/assets/tabler/*.svg "$repo/assets/tabler/LICENSE" "$repo/assets/tabler/provenance.json" "$output/package/assets/tabler/"

"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/applet_transition_probe.c" "$repo/src/applets/json.c" "$repo/src/applets/snapshot.c" \
 -o "$output/dev/applet_transition_probe" -lm

# Weather is a standard bundled widget of both x86_64 builds, not a
# manually assembled demo; a missing dependency stops the build there.
if [ "$dock_weather" = yes ]; then
  sh "$repo/scripts/build-bundled-applets.sh"
else
  echo "No bundled Weather for $dock_target (no provider for this target)"
fi
