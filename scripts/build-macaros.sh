#!/bin/sh
# Dockbar for Macaros (hosted aarch64 AROS on macOS, mainline v1 based).
# No FreeType there: Smooth text falls back to the system font. No SVG icons.
# Set BUILD to the Macaros build tree and XT to its LLVM toolchain.
set -eu
: "${BUILD:?Source the Macaros env.sh first}"
: "${XT:?Source the Macaros env.sh first}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${DOCK_BUILD_DIR:-"$repo/build/aarch64-macaros"}
T=$BUILD/bin/darwin-aarch64
DEV=$T/AROS/Developer
L=$DEV/lib
: "${SVGICON_INCLUDE:?Set SVGICON_INCLUDE to the generated svgicon headers}"
mkdir -p "$output/package" "$output/obj"
# The card controls are Tabler outline icons rasterised here, so that every
# package draws them alike, with or without svgicon.library.
python3 "$repo/scripts/tabler-masks.py" "$repo/assets/tabler" 20 "$output/gen/tabler_masks.h" \
 adjustments-horizontal x chevron-left chevron-right refresh
rm -f "$output/obj/"*.o
for f in main migrate fade sprite pak drawer_panel dirlist icon_cache model config interaction group_panel chrome frame prefs prefs_window prefs_strings grip_menu about item_list layout magnify hover \
         applets/view applets/cards_aros applets/cache applets/json applets/package applets/snapshot applets/instance applets/settings applets/worker_aros applets/purge applets/panel_aros docks widget item_editor native_icons svg_icons resample snapshot theme textrender_none fontfile command; do
  "$XT/bin/clang" --target=aarch64-unknown-aros -mcmodel=large -ffixed-x18 -D__arm64__ -DDOCK_NO_FREETYPE -DDOCK_NO_PNG -DDOCK_APPLET_VARIANT=\"aarch64-macaros\" -O2 \
    -std=gnu99 -Wall -Wextra -Werror -Winfinite-recursion -Wno-unused-command-line-argument \
    -isystem "$DEV/include/aros/posixc" -isystem "$DEV/include/aros/stdc" -I"$DEV/include" -I"$SVGICON_INCLUDE" -I"$output/gen" -I"$repo/src" \
    -c "$repo/src/$f.c" -o "$output/obj/$(printf '%s' "$f" | tr / _).o"
done
COMPILER_PATH=$XT/bin "$T/tools/collect-aros" --eh-frame-hdr --allow-multiple-definition \
  -L"$L" -L"$XT/lib/generic" -o "$output/package/Dockbar" "$L/startup.o" "$output/obj/"*.o \
  -\( -lworkbench -licon -lcybergraphics -lmuimaster -lmui -lm -lamiga -larossupport -lintuition -lgraphics \
      -lgadtools -llayers -lutility -ldos -laros -llibinit -lautoinit -lposixc -lstdcio -lstdc -lexec -\) \
  -lclang_rt.builtins-aarch64
"$XT/bin/llvm-strip" --strip-debug "$output/package/Dockbar"
echo "Application: $output/package"

mkdir -p "$output/package/assets/tabler"
cp "$repo"/assets/tabler/*.svg "$repo/assets/tabler/LICENSE" "$repo/assets/tabler/provenance.json" "$output/package/assets/tabler/"
