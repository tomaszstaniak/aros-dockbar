#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run=$(mktemp -d "${TMPDIR:-/tmp}/dockbar-sprite.XXXXXX")
trap 'rm -rf "$run"' EXIT HUP INT TERM
: "${PNG_CONFIG:=libpng-config}"
command -v "$PNG_CONFIG" >/dev/null 2>&1 || { echo 'Install libpng development files (libpng-config required)' >&2; exit 1; }
# Original generated fixture: 120 colored cells plus an unused grid cell.
python3 - "$run/sheet.png" <<'PY'
import sys,struct,zlib
w=h=1408
rows=[]
for y in range(h):
 row=bytearray()
 for x in range(w):
  frame=(y//128)*11+x//128
  row.extend((frame*2%256,frame*3%256,frame*5%256,128))
 rows.append(b'\0'+row)
def chunk(t,d):
 return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))
PY
${CC:-cc} -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -I"$repo/src" $("$PNG_CONFIG" --cflags) "$repo/tests/test_sprite.c" \
 "$repo/src/sprite.c" "$repo/src/pak.c" "$repo/src/resample.c" $("$PNG_CONFIG" --ldflags) -lm -o "$run/test"
"$run/test" "$run/sheet.png"
# The same sheet from a resource pack, as licensed artwork ships.
python3 "$repo/scripts/dockpak.py" "$run/sprites.pak" "$run/sheet.png"
"$run/test" "$run/sprites.pak#sheet.png"
