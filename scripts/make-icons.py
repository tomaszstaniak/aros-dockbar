#!/usr/bin/env python3
"""Write the package's AROS PNG icons (.info) from original drawings.

AROS reads a PNG whose icOn chunk carries the DiskObject attributes
(workbench/libs/icon/diskobjPNGio.c). Drawing in code keeps the artwork
reproducible without a host SVG renderer.

usage: make-icons.py OUTPUT_DIR [TOOLTYPE...]
"""
import math
import struct
import sys
import zlib
from pathlib import Path

SIZE = 64
SS = 4  # supersampling per axis

ATTR_DRAWERX, ATTR_DRAWERY = 0x80001003, 0x80001004
ATTR_DRAWERWIDTH, ATTR_DRAWERHEIGHT = 0x80001005, 0x80001006
ATTR_STACKSIZE, ATTR_DEFAULTTOOL = 0x80001009, 0x8000100A
ATTR_TOOLTYPE = 0x8000100B
ATTR_TYPE, ATTR_FRAMELESS = 0x8000100F, 0x80001010
WBDRAWER, WBTOOL, WBPROJECT = 2, 3, 4


def rounded(px, py, x0, y0, x1, y1, r):
    """Signed distance to a rounded rectangle, negative inside."""
    cx, cy = min(max(px, x0 + r), x1 - r), min(max(py, y0 + r), y1 - r)
    if x0 + r <= px <= x1 - r or y0 + r <= py <= y1 - r:
        return max(x0 - px, px - x1, y0 - py, py - y1)
    return math.hypot(px - cx, py - cy) - r


def render(shapes):
    """shapes: list of (distance_fn, (r,g,b,a)) painted in order."""
    out = bytearray()
    n = SS * SS
    for y in range(SIZE):
        out.append(0)
        for x in range(SIZE):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    px, py = x + (sx + .5) / SS, y + (sy + .5) / SS
                    c = [0.0, 0.0, 0.0, 0.0]
                    for dist, (r, g, b, a) in shapes:
                        if dist(px, py) <= 0:
                            fa = a / 255
                            c = [r * fa + c[0] * (1 - fa), g * fa + c[1] * (1 - fa),
                                 b * fa + c[2] * (1 - fa), fa + c[3] * (1 - fa)]
                    for i in range(4):
                        acc[i] += c[i]
            a = acc[3] / n
            if a > 0:
                out += bytes(int(min(255, acc[i] / n / a + .5)) for i in range(3))
            else:
                out += b"\0\0\0"
            out.append(int(a * 255 + .5))
    return bytes(out)


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def icon_png(pixels, attrs):
    body = b""
    for tag, value in attrs:
        body += struct.pack(">I", tag)
        body += value.encode("latin-1") + b"\0" if isinstance(value, str) else struct.pack(">I", value)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
            + chunk(b"icOn", body) + chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))


def rr(x0, y0, x1, y1, r):
    return lambda px, py: rounded(px, py, x0, y0, x1, y1, r)


def dock_parts(top):
    """The bar with three launchers, shared by the tool and drawer icons."""
    tiles = [(0x4f, 0xa8, 0xe8), (0xf0, 0xb4, 0x3c), (0x6c, 0xc4, 0x7c)]
    parts = [(rr(5, top + 2, 61, top + 22, 5), (0, 0, 0, 70)),
             (rr(4, top, 60, top + 20, 5), (0x76, 0x7d, 0x82, 255)),
             (rr(5, top + 1, 59, top + 19, 4), (0x34, 0x39, 0x3e, 255))]
    for i, colour in enumerate(tiles):
        x = 11 + i * 15
        parts.append((rr(x, top + 4, x + 12, top + 16, 3), colour + (255,)))
    return parts


def tool_icon():
    return render(dock_parts(30) + [(rr(9, 52, 55, 53.5, .7), (0x9b, 0xdb, 0xd4, 255))])


def drawer_icon():
    back = (0x2f, 0x7f, 0xc4, 255)
    front = (0x4f, 0xa8, 0xe8, 255)
    return render([(rr(6, 10, 30, 22, 3), back), (rr(6, 14, 58, 54, 4), back),
                   (rr(6, 20, 58, 54, 4), front)] +
                  dock_parts(28)[1:])


def readme_icon():
    page = (0xf4, 0xf4, 0xf0, 255)
    lines = [(rr(18, y, 46 - (8 if y > 40 else 0), y + 2, 1), (0x80, 0x88, 0x90, 255)) for y in range(18, 50, 6)]
    return render([(rr(13, 7, 53, 59, 3), (0, 0, 0, 60)), (rr(12, 6, 52, 58, 3), (0x90, 0x96, 0x9c, 255)),
                   (rr(13, 7, 51, 57, 2), page)] + lines)


def main():
    out = Path(sys.argv[1])
    # Optional extra tool types for the program icon, e.g. LOG=RAM:dock.log
    # for a diagnostic build; the release icons carry none.
    extra = [(ATTR_TOOLTYPE, t) for t in sys.argv[2:]]
    out.mkdir(parents=True, exist_ok=True)
    icons = {
        "Dockbar.info": (tool_icon(), [(ATTR_TYPE, WBTOOL), (ATTR_STACKSIZE, 65536), (ATTR_FRAMELESS, 1)] + extra),
        "drawer.info": (drawer_icon(), [(ATTR_TYPE, WBDRAWER), (ATTR_FRAMELESS, 1),
                                        (ATTR_DRAWERX, 80), (ATTR_DRAWERY, 80),
                                        (ATTR_DRAWERWIDTH, 460), (ATTR_DRAWERHEIGHT, 220)]),
        "ReadMe.info": (readme_icon(), [(ATTR_TYPE, WBPROJECT), (ATTR_FRAMELESS, 1),
                                        (ATTR_DEFAULTTOOL, "SYS:Utilities/MultiView")]),
    }
    for name, (pixels, attrs) in icons.items():
        (out / name).write_bytes(icon_png(pixels, attrs))
        print(out / name)


if __name__ == "__main__":
    main()
