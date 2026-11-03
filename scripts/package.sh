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
base="Dockbar-$version-$dock_archive"
[ -x "$build/package/Dockbar" ] || { echo "Run scripts/build.sh first" >&2; exit 1; }
if [ "$svg" = 1 ]; then
  # The SVG package ships the renderer: it must be there, built for this
  # target, from the revision named in NOTICES, with its licence text.
  lib="$build/package/libs/svgicon.library"
  [ -f "$lib" ] || { echo "No $lib: build with SVGICON_LIBRARY set, or package with DOCK_SVG=0" >&2; exit 1; }
  dock_check_svgicon "$lib" "$build/svgicon.BUILDINFO"
  # Revision and renderer: from the library's BUILDINFO, or, for a library
  # built before BUILDINFO existed, from a pinned entry for exactly its bytes.
  prov="$repo/packaging/svg-provenance.txt"
  [ -f "$prov" ] || { echo "Missing $prov" >&2; exit 1; }
  pin_commit=$(sed -n 's/^lunasvg_commit=//p' "$prov")
  pin_licence=$(sed -n 's/^lunasvg_license_sha256=//p' "$prov")
  [ -n "$pin_commit" ] && [ -n "$pin_licence" ] || { echo "$prov names no renderer commit or licence checksum" >&2; exit 1; }
  lib_sum=$(shasum -a 256 "$lib" | cut -d' ' -f1)
  if [ -f "$build/svgicon.BUILDINFO" ]; then
    info="$build/svgicon.BUILDINFO"
    case "$(sed -n 's/^target=//p' "$info")" in abiv11) info_abi=v11 ;; v1|raspi) info_abi=v1 ;; *) info_abi=unknown ;; esac
    [ "$info_abi" = "$dock_abi" ] || { echo "$info is for another target than $dock_target" >&2; exit 1; }
    info_rev=$(sed -n 's/^revision=//p' "$info")
    [ "$info_rev" = "$svgrev" ] || {
      echo "svgicon.library was built from $info_rev, SVGICON_REVISION is $svgrev" >&2; exit 1; }
    [ "$(sed -n 's/^sha256=//p' "$info")" = "$lib_sum" ] ||
      { echo "$lib differs from the library its BUILDINFO describes" >&2; exit 1; }
    [ -z "$(sed -n 's/^variant=//p' "$info")" ] ||
      { echo "$info describes a test variant of the library, not svgicon.library" >&2; exit 1; }
    [ "$(sed -n 's/^lunasvg=//p' "$info")" = "$pin_commit" ] ||
      { echo "$info names LunaSVG $(sed -n 's/^lunasvg=//p' "$info"), $prov pins $pin_commit" >&2; exit 1; }
    if [ "$(sed -n 's/^target=//p' "$info")" = raspi ]; then
      # The C++ runtime inside the Pi library: the pinned archives and licence.
      for k in libcxx:libc++.a libunwind:libunwind.a; do
        pin=$(sed -n "s/^raspi_cxxrt_${k%%:*}_sha256=//p" "$prov")
        got=$(sed -n "s/^cxxrt_${k#*:}=//p" "$info")
        [ -n "$pin" ] && [ "$got" = "$pin" ] ||
          { echo "$info names ${k#*:} ${got:-none}, $prov pins ${pin:-none}" >&2; exit 1; }
      done
      [ "$(shasum -a 256 "$repo/packaging/licenses/llvm-runtime-LICENSE.TXT" | cut -d' ' -f1)" = "$(sed -n 's/^raspi_cxxrt_license_sha256=//p' "$prov")" ] ||
        { echo "packaging/licenses/llvm-runtime-LICENSE.TXT is not the pinned LLVM runtime licence" >&2; exit 1; }
      [ -f "$repo/packaging/svg-runtime-notice-$target.txt" ] ||
        { echo "Missing packaging/svg-runtime-notice-$target.txt for the C++ runtime in the library" >&2; exit 1; }
    fi
  else
    legacy=$(sed -n "s/^legacy=$dock_target $lib_sum //p" "$prov")
    [ -n "$legacy" ] || {
      echo "No BUILDINFO for $lib (sha256 $lib_sum), and it is not a pinned legacy library for $dock_target in $prov" >&2; exit 1; }
    [ "$legacy" = "$svgrev" ] || {
      echo "Pinned legacy library $lib_sum is revision $legacy, SVGICON_REVISION is $svgrev" >&2; exit 1; }
    echo "Note: $lib is the pinned legacy library for $dock_target (no BUILDINFO); revision $legacy from $prov" >&2
  fi
  # Licence: the exact LICENSE of the pinned renderer commit, checked by its
  # bytes and, when it sits in a checkout, by that checkout's commit; the
  # MIT wording is checked separately so a wrong pin cannot pass silently.
  [ -s "$svglicence" ] || { echo "SVGICON_LICENSE_FILE is missing or empty: $svglicence" >&2; exit 1; }
  [ "$(shasum -a 256 "$svglicence" | cut -d' ' -f1)" = "$pin_licence" ] ||
    { echo "SVGICON_LICENSE_FILE differs from the pinned LunaSVG licence text ($pin_licence): $svglicence" >&2; exit 1; }
  licdir=$(dirname "$svglicence")
  if git -C "$licdir" rev-parse --git-dir >/dev/null 2>&1; then
    [ "$(git -C "$licdir" rev-parse HEAD)" = "$pin_commit" ] ||
      { echo "$licdir is at $(git -C "$licdir" rev-parse HEAD), $prov pins $pin_commit" >&2; exit 1; }
    [ -z "$(git -C "$licdir" status --porcelain -- LICENSE plutovg/LICENSE)" ] ||
      { echo "LICENSE files in $licdir are modified" >&2; exit 1; }
  fi
  [ ! -f "$licdir/plutovg/LICENSE" ] || cmp -s "$svglicence" "$licdir/plutovg/LICENSE" ||
    { echo "$licdir/plutovg/LICENSE differs from LunaSVG's: NOTICES would need both" >&2; exit 1; }
  grep -q "Permission is hereby granted, free of charge" "$svglicence" ||
    { echo "SVGICON_LICENSE_FILE is not MIT licence text: $svglicence" >&2; exit 1; }
  [ -f "$repo/packaging/svg-notice.txt" ] || { echo "Missing packaging/svg-notice.txt" >&2; exit 1; }
  grep -q "$pin_commit" "$repo/packaging/svg-notice.txt" ||
    { echo "packaging/svg-notice.txt does not name the pinned LunaSVG commit $pin_commit" >&2; exit 1; }
  # Shipped for users to choose as item icons.
  [ -s "$repo/assets/runtime/folder.svg" ] || { echo "Missing assets/runtime/folder.svg" >&2; exit 1; }
fi
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
    # A target whose provider links other library versions than the PC
    # packages adds its paragraph to the shared list here, before the
    # licence sources; the PC packages keep the list as it is.
    wlic="$repo/packaging/weather-licenses-$target.txt"
    if [ -f "$wlic" ]; then
      readme_w="$app/Widgets/org.aros.weather/licenses/README.txt"
      [ -f "$readme_w" ] || { echo "Missing $readme_w for $wlic" >&2; exit 1; }
      python3 -c 'import sys
t=open(sys.argv[1]).read(); add=open(sys.argv[2]).read(); key="License texts were retrieved"
if key not in t: sys.exit("no licence-source line in " + sys.argv[1])
open(sys.argv[1],"w").write(t.replace(key, add + key, 1))' "$readme_w" "$wlic" || exit 1
    fi
  fi
  cp "$work/icons/Dockbar.info" "$app/Dockbar.info"
  cp "$work/icons/drawer.info" "$root/Dockbar.info"
  cp "$work/icons/ReadMe.info" "$app/ReadMe.info"
  cp "$work/icons/ReadMe.info" "$app/NOTICES.info"
  cp "$work/icons/ReadMe.info" "$app/Changes.info"
  cp "$repo/LICENSE" "$app/LICENSE"
  cp "$work/icons/ReadMe.info" "$app/LICENSE.info"
  cp "$repo/packaging/Changes.txt" "$app/Changes"
  mkdir -p "$app/Examples"
  cp "$repo/packaging/Examples/ListFiles.script" "$app/Examples/"
  if [ "$2" = 1 ]; then
    mkdir -p "$app/assets" "$app/libs"
    cp "$repo"/assets/runtime/*.svg "$app/assets/"
    cp "$build/package/libs/svgicon.library" "$app/libs/"
    note="- SVG icons are drawn by libs/svgicon.library. Keep the libs and assets\n  drawers next to the program."
    notice=$(sed "s/@SVGREV@/$svgrev/" "$repo/packaging/svg-notice.txt"; cat "$svglicence")
    # A library that links its own C++ runtime (Raspberry Pi: LLVM libc++,
    # libc++abi, libunwind) carries that runtime's licence as well.
    if [ -f "$repo/packaging/svg-runtime-notice-$target.txt" ]; then
      notice=$(printf '%s\n\n' "$notice"; cat "$repo/packaging/svg-runtime-notice-$target.txt" "$repo/packaging/licenses/llvm-runtime-LICENSE.TXT")
    fi
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
  # The ReadMe's Weather entry names the PC packages; the Pi package has
  # Weather too. Only the Pi ReadMe changes, so the PC packages stay as built.
  if [ "$target" = aarch64-raspi-v1 ]; then
    python3 -c 'import sys
t=open(sys.argv[1]).read(); old="- Weather (x86_64 package only):"
if old not in t: sys.exit("ReadMe has no \"" + old + "\" line")
open(sys.argv[1],"w").write(t.replace(old, "- Weather:", 1))' "$app/ReadMe" || exit 1
  fi
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
