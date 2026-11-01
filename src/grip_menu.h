#ifndef DOCK_GRIP_MENU_H
#define DOCK_GRIP_MENU_H
#include "chrome.h"
#include "theme.h"
struct Screen;
struct Window;
struct Region;
struct TextFont;
enum { DOCK_MENU_PREFS, DOCK_MENU_SEPARATOR, DOCK_MENU_SPACER, DOCK_MENU_CLOCK, DOCK_MENU_APPLET, DOCK_MENU_COMMAND,
       DOCK_MENU_ITEMS, DOCK_MENU_REFRESH, DOCK_MENU_ABOUT, DOCK_MENU_QUIT, DOCK_MENU_COUNT };
/* Popup drawn like the dock. Main owns its IDCMP. Zero-initialize. */
struct DockMenu {
    struct DockBuffer buffer;
    struct Window *window;
    struct Region *shape;
    int hover, width, height, row;
    long pen;
    const struct DockColors *colors;
};
/* Opens beside the anchor rectangle (screen coordinates), above if there is room. */
/* Anchor is the grip in screen coordinates; vertical bars get the menu beside. */
int dock_menu_open(struct DockMenu *menu, struct Screen *screen, struct TextFont *font,
                   int anchor_x, int anchor_y, int anchor_width, int anchor_height, int vertical,
                   const struct DockColors *colors);
/* Window-relative coordinates; -1 outside any entry. */
int dock_menu_hit(const struct DockMenu *menu, int x, int y);
void dock_menu_draw(struct DockMenu *menu);
void dock_menu_close(struct DockMenu *menu, struct Screen *screen);
#endif
