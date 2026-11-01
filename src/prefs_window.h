#ifndef DOCK_PREFS_WINDOW_H
#define DOCK_PREFS_WINDOW_H
#include "docks.h"
struct Screen;
struct Window;
struct Library;
/* Nonblocking Zune window over a private copy of the dock set. The list
 * holds "Defaults for all docks" first, then the docks; the right side
 * shows the defaults or the selected dock in four tabs (Placement,
 * Content, Appearance, Behaviour).
 * poll: 0 pending, -1 Cancel (also close gadget and Escape), 1 Save,
 * 2 Apply, 3 Help; the edited set is *set of the window. After Save, Apply
 * or Help the caller calls _keep() to keep the window open (Save that
 * failed, Apply, Help). x or y below 0 centres the window. */
enum { DOCK_PREFS_CANCEL = -1, DOCK_PREFS_SAVE = 1, DOCK_PREFS_APPLY = 2, DOCK_PREFS_HELP = 3 };
struct DockPrefsWindow {
    struct Window *window;
    struct Library *mui_base;
    void *application, *mui_window, *list, *pages, *tabs, *header, *help, *duplicate, *remove;
    void *name, *orientation, *placement, *align, *offset, *locked;
    void *icon, *spacing, *corner, *labels, *present, *buttons, *beside;
    /* This dock: each with "Inherit (value)" as its first entry. */
    void *theme, *background, *own_transparency, *transparency, *glass, *grip, *hover, *running;
    /* Defaults for all docks. */
    void *d_theme, *d_background, *d_transparency, *d_glass, *d_grip, *d_text, *d_hover, *d_running;
    /* Entry texts of the Inherit choices, rewritten when the defaults change. */
    char inherit[7][48];
    const char *themes[5], *backgrounds[4], *own[3], *glasses[4], *grips[4], *hovers[5], *runnings[4];
    char header_text[96];
    struct DockSet *set;
    int current;   /* -1: Defaults for all docks; else the dock's index */
    int result;
    unsigned int signals;   /* Zune's signals, see zune_input.h */
    int again;
};
int dock_prefs_window_open(struct DockPrefsWindow *w, struct Screen *screen,
                           const struct DockSet *set, int selected, int x, int y);
extern int dock_prefs_window_failed;
/* Set before opening, per dock index, for the docks that are composed
 * (present.h) where the compositor cannot blur: the frosted glass then
 * blurs a picture of the desktop, which a composed dock does not have, so
 * its choice is kept but cannot be changed there. The defaults' choice is
 * fixed only when every dock is such a dock. */
extern unsigned char dock_prefs_window_composed[DOCK_MAX_DOCKS];
extern int dock_prefs_window_all_composed;
int dock_prefs_window_poll(struct DockPrefsWindow *w);
void dock_prefs_window_keep(struct DockPrefsWindow *w);
/* Shows a message (for example a failed save) in the window's help line,
 * where it stays until another page or tab is shown. Dock messages are not
 * shown while this window is open. */
void dock_prefs_window_message(struct DockPrefsWindow *w, const char *text);
void dock_prefs_window_close(struct DockPrefsWindow *w);
#endif
