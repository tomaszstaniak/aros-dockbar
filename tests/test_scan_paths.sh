#!/bin/sh
# Negative and positive cases for scripts/scan-paths.sh.
set -u
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
scan="$repo/scripts/scan-paths.sh"
t=$(mktemp -d "${TMPDIR:-/tmp}/scan-test.XXXXXX") || exit 1
trap 'chmod -R u+rwx "$t" 2>/dev/null; rm -rf "$t"' EXIT
fail=0
expect() { # want description command...
  want=$1 what=$2; shift 2
  "$@" >/dev/null 2>&1; got=$?
  if [ "$got" = "$want" ]; then :; else echo "scan-paths: $what: exit $got, expected $want"; fail=1; fi
}
mkdir -p "$t/clean/Dockbar" && printf 'plain text\n' > "$t/clean/Dockbar/ReadMe"
expect 0 "clean directory" sh "$scan" "$t/clean"
(cd "$t/clean" && zip -q -r "$t/clean.zip" Dockbar)
expect 0 "clean archive" sh "$scan" "$t/clean.zip"
mkdir -p "$t/text/Dockbar" && printf 'built in /Volumes/disk/x\n' > "$t/text/Dockbar/NOTICES"
expect 1 "path in a text file" sh "$scan" "$t/text"
mkdir -p "$t/bin/Dockbar" && printf '\177ELF\000\001%s/src/a.c\000\002' "$HOME" > "$t/bin/Dockbar/Dockbar"
expect 1 "home directory in a binary" sh "$scan" "$t/bin"
(cd "$t/bin" && zip -q -r "$t/bin.zip" Dockbar)
expect 1 "home directory in a binary inside a ZIP" sh "$scan" "$t/bin.zip"
mkdir -p "$t/name/Dockbar/Users/x" && printf 'ok\n' > "$t/name/Dockbar/Users/x/f"
(cd "$t/name" && zip -q -r "$t/name.zip" Dockbar)
expect 1 "path in an entry name" sh "$scan" "$t/name.zip"
mkdir -p "$t/locked/Dockbar" && printf 'secret\n' > "$t/locked/Dockbar/f" && chmod 000 "$t/locked/Dockbar/f"
expect 2 "unreadable file" sh "$scan" "$t/locked"
expect 2 "missing archive" sh "$scan" "$t/absent.zip"
printf 'not a zip' > "$t/broken.zip"
expect 2 "broken archive" sh "$scan" "$t/broken.zip"
[ $fail = 0 ] && echo "scan-paths: clean, text, binary, entry name, unreadable, missing and broken cases passed"
exit $fail
