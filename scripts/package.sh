#!/bin/sh
# Builds the tester archives from an existing scripts/build.sh output.
# Two variants: with the SVG renderer, and .info-only (no SVG renderer;
# FreeType for smooth text is in both).
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/scripts/target.sh"
build=$dock_build
dist=${DOCK_DIST_DIR:-"$repo/dist"}
mkdir -p "$dist"
dist=$(CDPATH= cd -- "$dist" && pwd)  # zip runs from inside the work tree
version=$(sed -n 's/^#define DOCK_VERSION "\(.*\)"$/\1/p' "$repo/src/version.h")
[ -n "$version" ] || { echo "No DOCK_VERSION in src/version.h" >&2; exit 1; }
# DOCK_SVG=0 builds only the .info-only archive, without the renderer inputs.
svg=${DOCK_SVG:-1}
if [ "$svg" = 1 ]; then
  svgrev=${SVGICON_REVISION:?Set SVGICON_REVISION to the aros-svg-icons commit the library was built from}
  svglicence=${SVGICON_LICENSE_FILE:?Set SVGICON_LICENSE_FILE to the LunaSVG LICENSE of that build}
fi
stamp=202610131200
export TZ=UTC  # same archive bytes on every host
# The packaging name (archive name, ReadMe requirements, NOTICES).
target=$dock_pack
[ -f "$repo/packaging/requirements-$target.txt" ] && [ -f "$repo/packaging/build-$target.txt" ] ||
  { echo "No packaging/requirements-$target.txt or build-$target.txt" >&2; exit 1; }
base="Dockbar-$version-$target"
[ -x "$build/package/Dockbar" ] || { echo "Run scripts/build.sh first" >&2; exit 1; }
if [ "$dock_weather" = yes ]; then
  weather="$build/package/Widgets/org.aros.weather"
  [ -f "$weather/applet.json" ] && [ -f "$weather/assets/conditions.json" ] &&
    [ -x "$weather/bin/$dock_target/WeatherProvider" ] || {
      echo "Missing bundled Weather. Re-run scripts/build.sh; refusing an incomplete Dockbar package." >&2
      exit 1
    }
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/dockbar-pack.XXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'rm -rf "$work"; exit 130' HUP INT TERM
python3 "$repo/scripts/make-icons.py" "$work/icons" >/dev/null

# fill TEMPLATE MARKER TEXT: replaces @VERSION@ and a line holding only MARKER.
fill() {
  python3 -c 'import sys
version, marker, text = sys.argv[2], sys.argv[3], sys.argv[4].replace("\\n", "\n")
for line in open(sys.argv[1], encoding="utf-8"):
    line = line.replace("@VERSION@", version)
    sys.stdout.write(text + "\n" if line.rstrip("\n") == marker else line)' "$1" "$version" "$2" "$3"
}

variant() { # name svg(0|1)
  root="$work/$1"; app="$root/Dockbar"
  mkdir -p "$app"
  cp "$build/package/Dockbar" "$app/Dockbar"
  if [ -d "$build/package/Widgets" ]; then
    cp -R "$build/package/Widgets" "$app/Widgets"
    cp "$repo/docs/applets.md" "$app/Widgets-ReadMe.md"
  fi
  cp "$work/icons/Dockbar.info" "$app/Dockbar.info"
  cp "$work/icons/drawer.info" "$root/Dockbar.info"
  cp "$work/icons/ReadMe.info" "$app/ReadMe.info"
  cp "$work/icons/ReadMe.info" "$app/NOTICES.info"
  cp "$work/icons/ReadMe.info" "$app/Changes.info"
  cp "$repo/packaging/Changes.txt" "$app/Changes"
  mkdir -p "$app/Examples"
  cp "$repo/packaging/Examples/ListFiles.script" "$app/Examples/"
  if [ "$2" = 1 ]; then
    mkdir -p "$app/assets" "$app/libs"
    cp "$repo"/assets/runtime/*.svg "$app/assets/"
    cp "$build/package/libs/svgicon.library" "$app/libs/"
    note="- SVG icons are drawn by libs/svgicon.library. Keep the libs and assets\n  drawers next to the program."
    notice=$(sed "s/@SVGREV@/$svgrev/" "$repo/packaging/svg-notice.txt"; cat "$svglicence")
  else
    if [ "$svg" = 1 ]; then
      note="- This package has no SVG support. .svg icons are replaced by the\n  program's own .info icon. Use the other package for SVG."
    else
      # No package with the renderer exists for this target.
      note="- This package has no SVG support. .svg icons are replaced by the\n  program's own .info icon."
    fi
    notice=""
  fi
  fill "$repo/packaging/ReadMe.txt" @SVGNOTE@ "$note" > "$work/readme1"
  # Targets without their own test line describe the x86_64 ABIv11 build
  # (their requirements text says so).
  tested="$repo/packaging/tested-$target.txt"
  [ -f "$tested" ] || tested="$repo/packaging/tested-x86_64-abiv11.txt"
  fill "$work/readme1" @REQUIREMENTS@ "$(cat "$repo/packaging/requirements-$target.txt")" > "$work/readme2"
  fill "$work/readme2" @TESTED@ "$(cat "$tested")" > "$app/ReadMe"
  libs_text=$(cut -d'|' -f2 "$repo/packaging/build-$target.txt" | tail -1)
  fill "$repo/packaging/NOTICES.txt" @SVGNOTICE@ "$notice" | sed "s|@LIBS@|$libs_text|" > "$work/notices1"
  # A target without FreeType (libraries "-") has no Smooth text section.
  if [ "$libs_text" = "-" ]; then
    python3 -c 'import sys,re; t=open(sys.argv[1]).read(); open(sys.argv[1],"w").write(re.sub(r"Smooth text \(Preferences.*?FONTS:TrueType/\)\.\n", "", t, flags=re.S))' "$work/notices1"
  fi
  python3 -c 'import sys; t=open(sys.argv[1]).read(); b=open(sys.argv[2]).read().split("|")[0]; sys.stdout.write(t.replace("@BUILD@", b))' "$work/notices1" "$repo/packaging/build-$target.txt" > "$app/NOTICES"
  if [ -d "$app/Widgets/org.aros.weather" ]; then
    # The provider's libraries differ per target.
    notice_w="$repo/packaging/weather-notice-$target.txt"
    [ -f "$notice_w" ] || notice_w="$repo/packaging/weather-notice.txt"
    cat "$notice_w" >> "$app/NOTICES"
    [ -f "$app/Widgets/org.aros.weather/assets/sprites.pak" ] &&
      cat "$repo/packaging/weather-sprites-notice.txt" >> "$app/NOTICES"
  fi
  mkdir -p "$app/assets/tabler"
  cp "$repo"/assets/tabler/*.svg "$repo/assets/tabler/LICENSE" "$repo/assets/tabler/provenance.json" "$app/assets/tabler/"
  printf '\nTabler Icons — MIT. Original SVGs from tabler/tabler-icons; see assets/tabler/provenance.json.\n\n' >> "$app/NOTICES"
  cat "$repo/assets/tabler/LICENSE" >> "$app/NOTICES"
  chmod 755 "$app/Dockbar"
  find "$root" -exec touch -t "$stamp" {} +
  (cd "$root" && zip -X -q -r "$work/$1.zip" Dockbar Dockbar.info)
  # dist/ is where the user picks archives up: an archive already there may
  # have been handed out, so it is never replaced by different bytes.
  if [ -e "$dist/$1.zip" ]; then
    if cmp -s "$work/$1.zip" "$dist/$1.zip"; then
      echo "$1.zip already in dist/ with the same bytes"
    else
      echo "$1.zip is already in dist/ with different bytes; not replaced. Use a new version name." >&2
      exit 1
    fi
  else
    cp "$work/$1.zip" "$dist/$1.zip"
  fi
}
archives="$base-info-only.zip"
variant "$base-info-only" 0
if [ "$svg" = 1 ]; then variant "$base" 1; archives="$base.zip $archives"; fi
# shellcheck disable=SC2086  # archive names contain no spaces
# SHA256SUMS covers every archive in dist/, not only this run's.
(cd "$dist" && shasum -a 256 *.zip > SHA256SUMS && grep -F -f /dev/stdin SHA256SUMS) <<EOF2
$(printf '%s\n' $archives)
EOF2
