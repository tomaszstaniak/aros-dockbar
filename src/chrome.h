#ifndef DOCK_CHROME_H
#define DOCK_CHROME_H
#include "frame.h"
struct Region;
struct RastPort;
struct Screen;
struct Window;
#include <graphics/rastport.h>
/* One radius for the bar and the group panel; 0 gives square corners. */
extern int dock_corner_radius;
/* Caller owns region; retain until its window has closed. */
struct Region *dock_shape(int width, int height, int radius);
/* Background and border of a window shaped by dock_shape(). */
void dock_frame_draw(struct RastPort *rp, const struct DockFrameStyle *style,
                     int width, int height, int radius);
/* Rounded fill whose edge pixels blend into the known colour behind it. */
void dock_round_fill(struct RastPort *rp, int x, int y, int width, int height,
                     int radius, unsigned long color, unsigned long behind);
/* Offscreen surface for a window that redraws in full: painting straight into
 * the window shows the cleared background for a moment on every redraw. */
struct DockBuffer { struct BitMap *bitmap; struct RastPort rp; int width, height; };
int dock_buffer_open(struct DockBuffer *buffer, struct Screen *screen, int width, int height);
void dock_buffer_show(struct DockBuffer *buffer, struct Window *window);
void dock_buffer_close(struct DockBuffer *buffer);
#endif
