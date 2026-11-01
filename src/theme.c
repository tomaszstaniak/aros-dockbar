#include "theme.h"
#include <math.h>

/* Dark is the look of 0.1 and 0.2: existing docks keep it. */
static const struct DockColors dark = {
    0x393e42, 0x2d3236, 0x767d82, 0x202427,
    0xedf3fa, 0xaabbcd, 0x15181b,
    0x555d63, 0x485159, 0x5a6f80, 0x48786f, 0x9bdbd4,
    0x727b82, 0x202427, 0xd8dde0,
    0x727c83, 0x525e67,
    0x30353a, 0x767d82, 0x485159,
    0x41474c,
};

static const struct DockColors light = {
    0xeef1f4, 0xdde2e7, 0x9aa3ab, 0x7d868e,
    0x1d2328, 0x4d5964, 0xf6f8fa,
    0xa8b0b7, 0xc9d1d8, 0xb3c2cf, 0xa9d3c9, 0x1f7f73,
    0x8e979f, 0xffffff, 0x3c454c,
    0x8d979f, 0xbcc5cc,
    0xf2f4f6, 0x9aa3ab, 0xd3dae0,
    0xe3e8ed,
};

const struct DockColors *dock_colors(int is_light)
{
    return is_light ? &light : &dark;
}

int dock_theme_light(int choice, int luminance, int previous)
{
    if (choice == DOCK_THEME_LIGHT) return 1;
    if (choice != DOCK_THEME_AUTO || luminance < 0) return 0;
    if (luminance >= 150) return 1;
    if (luminance <= 120) return 0;
    return previous == 1;
}

int dock_luminance(const unsigned char *argb, const unsigned char *mask, int count)
{
    double sum = 0;
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (mask && mask[i]) continue;
        const unsigned char *p = argb + 4 * i;
        sum += 0.2126 * p[1] + 0.7152 * p[2] + 0.0722 * p[3];
        n++;
    }
    return n ? (int)(sum / n + 0.5) : -1;
}

static double channel(unsigned long c)
{
    double v = (double)c / 255.0;
    return v <= 0.03928 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double relative(unsigned long rgb)
{
    return 0.2126 * channel((rgb >> 16) & 255) + 0.7152 * channel((rgb >> 8) & 255) + 0.0722 * channel(rgb & 255);
}

int dock_contrast100(unsigned long a, unsigned long b)
{
    double la = relative(a), lb = relative(b);
    if (la < lb) { double t = la; la = lb; lb = t; }
    return (int)((la + 0.05) / (lb + 0.05) * 100.0);
}
