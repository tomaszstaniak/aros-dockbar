#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run=$(mktemp -d "${TMPDIR:-/tmp}/dockbar-test.XXXXXX")
trap 'rm -rf "$run"' EXIT HUP INT TERM
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_model.c" "$repo/src/model.c" -o "$run/test-model"
"$run/test-model" "$run/pins"

printf 'hello\n' > "$run/a.txt"
python3 -c 'import sys; open(sys.argv[1],"wb").write(bytes(range(256)))' "$run/b.bin"
python3 "$repo/scripts/dockpak.py" "$run/test.pak" "$run/a.txt" "$run/b.bin"
! grep -q "hello" "$run/test.pak"  # scrambled, not stored as is
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_pak.c" "$repo/src/pak.c" -o "$run/test-pak"
"$run/test-pak" "$run/test.pak"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_icon_cache.c" "$repo/src/icon_cache.c" -o "$run/test-icon-cache"
"$run/test-icon-cache"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_resample.c" "$repo/src/resample.c" -lm -o "$run/test-resample"
"$run/test-resample"

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_interaction.c" "$repo/src/interaction.c" -o "$run/test-interaction"
"$run/test-interaction"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_composition.c" "$repo/src/model.c" "$repo/src/config.c" -o "$run/test-composition"
"$run/test-composition" "$run/config"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_frame.c" "$repo/src/frame.c" -lm -o "$run/test-frame"
"$run/test-frame"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_prefs.c" "$repo/src/prefs.c" -o "$run/test-prefs"
"$run/test-prefs" "$run/prefs"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_layout.c" "$repo/src/layout.c" "$repo/src/model.c" -o "$run/test-layout"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_magnify.c" "$repo/src/magnify.c" "$repo/src/layout.c" "$repo/src/model.c" -o "$run/test-magnify"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_hover.c" "$repo/src/hover.c" "$repo/src/magnify.c" "$repo/src/layout.c" "$repo/src/model.c" -o "$run/test-hover"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_alpha.c" "$repo/src/alpha.c" "$repo/src/frame.c" -lm -o "$run/test-alpha"
"$run/test-alpha"
"$run/test-hover"
"$run/test-magnify"
"$run/test-layout"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_docks.c" "$repo/src/docks.c" "$repo/src/config.c" "$repo/src/model.c" "$repo/src/layout.c" "$repo/src/theme.c" -lm -o "$run/test-docks"
"$run/test-docks" "$run"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_widget.c" "$repo/src/widget.c" -o "$run/test-widget"
"$run/test-widget"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_snapshot.c" "$repo/src/snapshot.c" -o "$run/test-snapshot"
"$run/test-snapshot"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_theme.c" "$repo/src/theme.c" -lm -o "$run/test-theme"
"$run/test-theme"
# The prefs reader takes damaged input: check it with AddressSanitizer.
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I"$repo/src" \
 "$repo/tests/test_wallprefs.c" "$repo/src/wallprefs.c" -o "$run/test-wallprefs"
"$run/test-wallprefs"
# Text renderer: host FreeType (pkg-config freetype2) and the TrueType files
# of the ABIv11 system (DOCK_TEST_FONTS, default: the SDK's ISO copy).
fonts=${DOCK_TEST_FONTS:-$HOME/Work/AROS-dev/toolchains/abiv11-2026.09/iso-files/Fonts/TrueType}
if pkg-config --exists freetype2 2>/dev/null; then
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" $(pkg-config --cflags freetype2) \
 "$repo/tests/test_textrender.c" "$repo/src/textrender.c" $(pkg-config --libs freetype2) -o "$run/test-textrender"
"$run/test-textrender" "$fonts"
else echo "SKIP textrender: no host FreeType"; fi
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$repo/src" \
 "$repo/tests/test_fontfile.c" "$repo/src/fontfile.c" -o "$run/test-fontfile"
"$run/test-fontfile" "$fonts/.."
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$repo/src" \
 "$repo/tests/test_command.c" "$repo/src/command.c" -o "$run/test-command"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$repo/src" \
 "$repo/tests/test_dirlist.c" "$repo/src/dirlist.c" -o "$run/test-dirlist"
"$run/test-dirlist"
"$run/test-command"

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I"$repo/src" \
 "$repo/tests/test_elapsed.c" "$repo/src/elapsed.c" "$repo/src/applets/instance.c" -o "$run/test-elapsed"
"$run/test-elapsed"
"$repo/scripts/test-applets.sh"
"$repo/scripts/test-sprite.sh"

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$repo/src" "$repo/tests/test_fade.c" "$repo/src/fade.c" -o "$run/test-fade"
"$run/test-fade"

sh "$repo/tests/test_scan_paths.sh"
python3 "$repo/tests/test_weather_bundle.py"
