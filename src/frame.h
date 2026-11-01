#ifndef DOCK_FRAME_H
#define DOCK_FRAME_H

/* Shared by the bar and the group panel. The window outline is a whole-pixel
   shape region, so larger radii show visible steps against the desktop. */
#define DOCK_CORNER_DEFAULT 5
#define DOCK_CORNER_MAX 8

struct DockFrameStyle {
    unsigned long fill_top, fill_bottom, border_top, border_bottom;
};

/* Signed distance from the centre of pixel (x,y) to the rounded outline,
   positive inside. */
float dock_edge_distance(int x, int y, int width, int height, int radius);
/* First pixel of row y that the shape region keeps; the row is symmetric. */
int dock_row_inset(int y, int width, int height, int radius);
/* 0..1 area estimate of pixel (x,y) inside the rounded rectangle. */
float dock_round_coverage(int x, int y, int width, int height, int radius);
unsigned long dock_mix(unsigned long a, unsigned long b, float t);
/* 0..1 weight of the 1 px border at pixel (x,y). */
float dock_frame_border(int x, int y, int width, int height, int radius);
/* Opaque 0xffRRGGBB background with a 1 px border on the kept outline. */
unsigned long dock_frame_pixel(const struct DockFrameStyle *style, int x, int y,
                               int width, int height, int radius);
#endif
