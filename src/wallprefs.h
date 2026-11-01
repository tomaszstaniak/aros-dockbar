#ifndef DOCK_WALLPREFS_H
#define DOCK_WALLPREFS_H
#include <stddef.h>

/* Reader for the desktop background in Wanderer's prefs file
 * (ENV:SYS/Wanderer/global.prefs). The format is not a public interface:
 * anything unexpected, damaged or of an unknown kind is refused, and the
 * caller falls back to reading the screen. Pure: no AROS calls. */

enum DockWallKind { DOCK_WALL_PICTURE = 5, DOCK_WALL_COLOUR = 2 };
enum DockWallMode { DOCK_WALL_TILED = 1, DOCK_WALL_SCALED = 2 };

struct DockWallPrefs {
    int kind;                 /* DockWallKind */
    char path[256];           /* picture */
    unsigned long rgb;        /* colour, 0xRRGGBB */
    int mode, tile, x, y;     /* render mode, tile mode (1 float, 2 fixed), offsets */
};

/* 1 and *out filled for a picture or colour background of the Workbench
 * view; 0 for anything else (damaged, truncated, other background kinds,
 * values out of range). */
int dock_wallprefs_parse(const unsigned char *data, size_t size, struct DockWallPrefs *out);

#endif
