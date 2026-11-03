#!/bin/sh
# scan-paths.sh DIR|ARCHIVE.zip [EXTRA-STRING...]
# Refuses build-machine paths in what Dockbar ships: /Users/, /Volumes/, the
# home directory, the workspace (two levels above this repository), the user
# name as a path component, and any EXTRA-STRING. A directory is scanned file
# by file; a ZIP archive by its entry names and by its unpacked content (the
# compressed bytes would hide a match).
# Exit 0: nothing found. Exit 1: a forbidden string was found (listed on
# stderr). Exit 2: the input could not be read or a tool failed.
set -u
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[ $# -ge 1 ] || { echo "usage: scan-paths.sh DIR|ARCHIVE.zip [STRING...]" >&2; exit 2; }
target=$1; shift
ws=$(dirname "$(dirname "$repo")")
user=$(id -un) || { echo "scan-paths: id -un failed" >&2; exit 2; }
[ -n "$user" ] && [ -n "${HOME:-}" ] || { echo "scan-paths: no user name or HOME" >&2; exit 2; }
patterns=$(mktemp "${TMPDIR:-/tmp}/scan-paths.XXXXXX") || exit 2
tmp=""
cleanup() { rm -f "$patterns"; [ -z "$tmp" ] || rm -rf "$tmp"; }
trap cleanup EXIT
printf '%s\n' /Users/ /Volumes/ "$HOME" "$ws" "/$user/" "$@" > "$patterns" || exit 2

# grep -r: 0 found, 1 none, 2 error (an unreadable file counts as an error).
scan_tree() {
  LC_ALL=C grep -r -l -a -F -f "$patterns" "$1"
  rc=$?
  case $rc in
    0) echo "scan-paths: forbidden build paths in the files above ($1)" >&2; return 1 ;;
    1) return 0 ;;
    *) echo "scan-paths: grep could not read $1 (rc $rc)" >&2; return 2 ;;
  esac
}

if [ -d "$target" ]; then
  scan_tree "$target" >&2; exit $?
fi
[ -f "$target" ] && [ -r "$target" ] || { echo "scan-paths: cannot read $target" >&2; exit 2; }
case "$target" in
  *.zip) ;;
  *) echo "scan-paths: $target is neither a directory nor a .zip" >&2; exit 2 ;;
esac
unzip -tq "$target" >/dev/null 2>&1 || { echo "scan-paths: $target is not a valid ZIP" >&2; exit 2; }
names=$(unzip -Z1 "$target") || { echo "scan-paths: cannot list $target" >&2; exit 2; }
[ -n "$names" ] || { echo "scan-paths: $target has no entries" >&2; exit 2; }
hits=$(printf '%s\n' "$names" | LC_ALL=C grep -a -F -f "$patterns")
case $? in
  0) echo "scan-paths: forbidden build paths in entry names of $target:" >&2; printf '%s\n' "$hits" >&2; exit 1 ;;
  1) ;;
  *) echo "scan-paths: grep failed on the entry names of $target" >&2; exit 2 ;;
esac
tmp=$(mktemp -d "${TMPDIR:-/tmp}/scan-paths-zip.XXXXXX") || exit 2
unzip -q "$target" -d "$tmp" || { echo "scan-paths: cannot unpack $target" >&2; exit 2; }
scan_tree "$tmp" >&2; rc=$?
[ $rc -ne 1 ] || echo "scan-paths: (in $target)" >&2
exit $rc
