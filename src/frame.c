#include "frame.h"
#include <math.h>

float dock_edge_distance(int x, int y, int width, int height, int radius)
{
    float px = x + 0.5f, py = y + 0.5f;
    float dx = px < radius ? radius - px : px > width - radius ? px - (width - radius) : 0;
    float dy = py < radius ? radius - py : py > height - radius ? py - (height - radius) : 0;
    if (dx > 0 && dy > 0) return radius - sqrtf(dx * dx + dy * dy);
    return fminf(fminf(px, width - px), fminf(py, height - py));
}

int dock_row_inset(int y, int width, int height, int radius)
{
    int edge = 0;
    while (edge < width / 2 && dock_edge_distance(edge, y, width, height, radius) < 0) ++edge;
    return edge;
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

float dock_round_coverage(int x, int y, int width, int height, int radius)
{
    return clamp01(dock_edge_distance(x, y, width, height, radius) + 0.5f);
}

unsigned long dock_mix(unsigned long a, unsigned long b, float t)
{
    unsigned long out = 0xff000000UL;
    for (int shift = 0; shift <= 16; shift += 8) {
        float ca = (a >> shift) & 0xff, cb = (b >> shift) & 0xff;
        out |= (unsigned long)(ca + (cb - ca) * t + 0.5f) << shift;
    }
    return out;
}

float dock_frame_border(int x, int y, int width, int height, int radius)
{
    return clamp01(1.5f - dock_edge_distance(x, y, width, height, radius));
}

unsigned long dock_frame_pixel(const struct DockFrameStyle *style, int x, int y,
                               int width, int height, int radius)
{
    float t = height > 1 ? (float)y / (height - 1) : 0;
    unsigned long fill = dock_mix(style->fill_top, style->fill_bottom, t);
    unsigned long border = dock_mix(style->border_top, style->border_bottom, t);
    /* The outer edge is a hard region cut: an opaque window cannot blend with
       the desktop behind it. Only the border-to-fill transition is smoothed. */
    float d = dock_edge_distance(x, y, width, height, radius);
    return dock_mix(fill, border, clamp01(1.5f - d));
}
