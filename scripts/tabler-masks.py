#!/usr/bin/env python3
"""Write the card controls' coverage masks from the original Tabler SVGs.

The controls must look the same in every package, also without an SVG
renderer on the target, so the outline icons are rasterised here at build
time and compiled into Dockbar; the theme colours them at run time. Only
what these outline icons use is understood: <path d> with M L H V A
(either case), stroke width 2, round caps and joins. Anything else stops
the build rather than drawing a different picture.

usage: tabler-masks.py SVG_DIR SIZE OUTPUT.h NAME...
"""
import math
import re
import sys
from pathlib import Path

SS = 5          # supersampling per axis
VIEW = 24.0     # Tabler's view box
STROKE = 2.0


def arc_points(x1, y1, rx, ry, phi, large, sweep, x2, y2):
    """SVG endpoint arc to a polyline (SVG 1.1 appendix F.6.5)."""
    if rx == 0 or ry == 0:
        return [(x2, y2)]
    rx, ry = abs(rx), abs(ry)
    c, s = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    xp, yp = c * dx + s * dy, -s * dx + c * dy
    lam = (xp * xp) / (rx * rx) + (yp * yp) / (ry * ry)
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * yp * yp - ry * ry * xp * xp
    den = rx * rx * yp * yp + ry * ry * xp * xp
    k = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cxp, cyp = k * rx * yp / ry, -k * ry * xp / rx
    cx = c * cxp - s * cyp + (x1 + x2) / 2
    cy = s * cxp + c * cyp + (y1 + y2) / 2

    def angle(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = angle(1, 0, (xp - cxp) / rx, (yp - cyp) / ry)
    dt = angle((xp - cxp) / rx, (yp - cyp) / ry, (-xp - cxp) / rx, (-yp - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(8, int(abs(dt) / (math.pi / 24)) + 1)
    out = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        ex, ey = rx * math.cos(t), ry * math.sin(t)
        out.append((c * ex - s * ey + cx, s * ex + c * ey + cy))
    return out


def path_segments(d):
    tokens = re.findall(r"[MmLlHhVvAa]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?", d)
    if "".join(tokens).replace("-", "").replace(".", "") == "":
        raise ValueError("empty path")
    rest = re.sub(r"[MmLlHhVvAa]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?|[\s,]", "", d)
    if rest:
        raise ValueError("unsupported path data: %r" % rest)
    segs, x, y, cmd, i = [], 0.0, 0.0, None, 0

    def num():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    while i < len(tokens):
        if tokens[i].isalpha():
            cmd = tokens[i]
            i += 1
        rel = cmd.islower()
        op = cmd.upper()
        if op == "M":
            nx, ny = num(), num()
            x, y = (x + nx, y + ny) if rel else (nx, ny)
            cmd = "l" if rel else "L"  # further pairs are line-tos
            continue
        if op == "L":
            nx, ny = num(), num()
            pts = [(x + nx, y + ny) if rel else (nx, ny)]
        elif op == "H":
            v = num()
            pts = [(x + v if rel else v, y)]
        elif op == "V":
            v = num()
            pts = [(x, y + v if rel else v)]
        elif op == "A":
            rx, ry, phi, large, sweep, nx, ny = (num() for _ in range(7))
            if rel:
                nx, ny = x + nx, y + ny
            pts = arc_points(x, y, rx, ry, phi, int(large), int(sweep), nx, ny)
        else:
            raise ValueError("unsupported command " + cmd)
        for p in pts:
            segs.append((x, y, p[0], p[1]))
            x, y = p
    return segs


def svg_segments(text):
    for bad in ("<circle", "<rect", "<line", "<polyline", "<polygon", "<ellipse", "transform="):
        if bad in text:
            raise ValueError("unsupported element " + bad)
    if 'stroke-width="2"' not in text or 'viewBox="0 0 24 24"' not in text:
        raise ValueError("not a Tabler outline icon")
    segs = []
    for d in re.findall(r'<path[^>]*\sd="([^"]+)"', text):
        segs += path_segments(d)
    if not segs:
        raise ValueError("no paths")
    return segs


def distance(px, py, s):
    x1, y1, x2, y2 = s
    vx, vy = x2 - x1, y2 - y1
    ll = vx * vx + vy * vy
    t = 0.0 if ll == 0 else max(0.0, min(1.0, ((px - x1) * vx + (py - y1) * vy) / ll))
    return math.hypot(px - (x1 + t * vx), py - (y1 + t * vy))


def mask(segs, size):
    scale = VIEW / size
    r = STROKE / 2
    out = []
    for y in range(size):
        for x in range(size):
            hit = 0
            for sy in range(SS):
                for sx in range(SS):
                    px, py = (x + (sx + 0.5) / SS) * scale, (y + (sy + 0.5) / SS) * scale
                    if any(distance(px, py, s) <= r for s in segs):
                        hit += 1
            out.append(round(255 * hit / (SS * SS)))
    return out


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    src, size, target, names = Path(sys.argv[1]), int(sys.argv[2]), Path(sys.argv[3]), sys.argv[4:]
    lines = ["/* Generated by scripts/tabler-masks.py from assets/tabler (Tabler Icons,",
             " * MIT, see assets/tabler/LICENSE and provenance.json). Do not edit. */",
             "#define DOCK_TABLER_SIZE %d" % size,
             "#define DOCK_TABLER_COUNT %d" % len(names),
             "static const unsigned char dock_tabler_masks[%d][%d] = {" % (len(names), size * size)]
    for name in names:
        m = mask(svg_segments((src / (name + ".svg")).read_text()), size)
        if sum(1 for v in m if v > 128) < size:
            sys.exit("%s: almost nothing drawn" % name)
        lines.append(" /* %s */ {" % name)
        for row in range(size):
            lines.append("  " + ",".join(str(v) for v in m[row * size:(row + 1) * size]) + ",")
        lines.append(" },")
    lines.append("};")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
