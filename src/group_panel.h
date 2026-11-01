#ifndef DOCK_GROUP_PANEL_H
#define DOCK_GROUP_PANEL_H

#include "model.h"
#include "native_icons.h"
#include "chrome.h"
#include "theme.h"

struct Gadget;

/* Zero-initialize before first use. Caller keeps Screen, icon.library,
 * CyberGfxBase and the SVG adapter alive. Main owns IDCMP/drop dispatch. */
struct DockPanel {
    struct DockBuffer buffer;
    struct Region *shape;
    struct Window *window;
    struct Screen *screen;
    DockDropTarget drop;
    DockNativeIcon icons[DOCK_MAX_ITEMS];
    unsigned char *svg[DOCK_MAX_ITEMS];
    char labels[DOCK_MAX_ITEMS][DOCK_LABEL];
    char title[DOCK_LABEL];
    int container, count, hover, pressed;
    int size, width, height, columns, rows;
    int grid_x, grid_y, cell_width, cell_height;
    long text_pen, muted_pen;
    const struct DockColors *colors;
};

int dock_panel_open(struct DockPanel *panel, struct Screen *screen,
                    struct DockModel *model, int container, int size,
                    int anchor_x, int anchor_y, int anchor_width, int anchor_height,
                    int vertical, int activate, const struct DockColors *colors);
void dock_panel_close(struct DockPanel *panel);
void dock_panel_draw(struct DockPanel *panel);
/* Coordinates are relative to panel->window, as in an IntuiMessage.
 * Header Edit region: x >= width-64, x < width, 0 <= y < grid_y-5.
 * Main routes that region to the root group editor; hit() returns -1 there. */
int dock_panel_hit(const struct DockPanel *panel, int x, int y);
/* Returns insertion index 0..count; points outside the grid append. */
int dock_panel_before(const struct DockPanel *panel, int x, int y);

/* Zune owns the event queue; main must not GetMsg this window.
 * poll: 0 pending, 1 Save, 2 Remove, -1 Cancel. Save copies current fields into
 * label/icon. Results remain available until explicit close; no model or file
 * is changed by this editor. Zero-initialize before first use. */
struct DockEditor {
    struct Window *window;
    struct Screen *screen;
    struct Library *mui_base;
    void *application, *mui_window, *name_object, *icon_object;
    void *seconds_object, *date_object, *present_object;
    void *command_object, *type_object, *dir_object, *shell_object;
    char label[DOCK_LABEL];
    char icon[DOCK_PATH];
    int kind, clock_options, result;  /* clock_options: DOCK_CLOCK_* flags */
    int present;                      /* DockPresentation of a dock item */
    char command[DOCK_PATH];          /* command item: command line or script */
    char dir[DOCK_DIR];
    int run;                          /* DOCK_RUN_* */
    int on_drop, open;                /* DOCK_DROP_* / DOCK_OPEN_* of the item */
    void *drop_object, *open_object;
    unsigned int signals;             /* Zune's signals, see zune_input.h */
    int again;
    /* Command form: the label and file button that follow the type, the
     * one-line hint for On drop, the error lines under Name and Command,
     * and the Help window. */
    void *command_label, *command_pop, *command_button, *drop_hint, *name_error, *command_error;
    void *help_window, *form_group, *dir_pop, *icon_pop;
    /* Hints are drawn in a pen halfway between the text and the background,
     * so they read as secondary without a smaller font; -1: text pen. */
    long hint_pen;
    char hint_style[24];
};

/* presentation: 1 offers how the item shows (icon, text, both), for items
 * in a dock; group members always show as icons. */
extern int dock_editor_failed;   /* source line of the last failed open */
int dock_editor_open(struct DockEditor *editor, struct Screen *screen,
                     const struct DockItem *item, int x, int y, int presentation);
int dock_editor_poll(struct DockEditor *editor);
/* After a Save the caller refused: the message goes under the field
 * (0 Name, 1 Command), the window stays open with what was typed, and the
 * next poll waits for the user again. */
void dock_editor_refuse(struct DockEditor *editor, int field, const char *message);
void dock_editor_close(struct DockEditor *editor);

#endif
