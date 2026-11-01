#ifndef DOCK_MAGNIFY_H
#define DOCK_MAGNIFY_H
#include "layout.h"

/* Geometry of icon magnification under the pointer (the hover effect
 * Magnify; Lift is in hover.h), on top of a finished
 * DockLayout. Nothing here moves an item: every icon grows about its own
 * centre and stays inside its own slot, so
 *  - clicks, drops, insertion marks and popup anchors keep using the rest
 *    layout, and the item drawn under the pointer is the item hit there;
 *  - the window size and its shape stay what the rest layout gives (the
 *    shape is never changed at run time, see draw_view in main.c);
 *  - a frame differs from the previous one only in the icons whose size
 *    changed, which bounds what has to be redrawn.
 * Pushing neighbours aside (the macOS dock) would need a larger window or a
 * changing one; that is a separate decision, not part of this geometry.
 *
 * Only plain icons magnify: programs, groups, drawers and commands shown as
 * icons. Text buttons, icon-and-text buttons, widgets, clocks, separators and
 * spaces keep their rest geometry. */

/* Sizes change in steps of this many pixels, so growth splits evenly on
 * both sides of the centre and a slow pointer move changes few icons. */
#define DOCK_MAGNIFY_STEP 2
/* Growth asked for under the pointer; the slot allows less (about 10 px
 * for icons of up to 72 px), so this mostly sets the falloff's slope. */
#define DOCK_MAGNIFY_PERCENT 30

struct DockMagnifyParams {
    int percent;  /* wanted growth of the icon under the pointer, 0..100;
                   * the slot may allow less (see dock_magnify_room) */
    int radius;   /* along the bar, distance from the pointer at which the
                   * growth has fallen to 0; 0 means 2.5 icon boxes */
};

struct DockMagnifyFrame {
    int count;
    /* Per slot: the icon box for this frame (the rest icon box when not
     * magnified or lifted), and the highlight to draw around it. A grown
     * highlight reaches no further towards an always-shown label than the
     * rest highlight does. */
    struct DockRect icon[DOCK_MAX_ITEMS];
    struct DockRect highlight[DOCK_MAX_ITEMS];
    int grown[DOCK_MAX_ITEMS];  /* pixels added to each side length */
};

/* 1 when slot 'index' takes part in hover effects: a plain icon. */
int dock_hover_item(const struct DockLayout *layout, int index);

/* Largest icon box slot 'index' can take: centred on its rest icon, along
 * the bar inside the slot (the spacing between slots stays empty), across
 * the bar at least DOCK_HIGHLIGHT_MARGIN from the bar's edge and clear of
 * an always-shown label. Zero size for items that do not magnify. Also what
 * the shape of a dock without a bar would have to cover. */
struct DockRect dock_magnify_room(const struct DockLayout *layout, const struct DockStyle *style, int index);

/* One frame for a pointer at 'pointer' on the main axis (window
 * coordinates) and an overall strength of 0..1000 (the caller's ramp in and
 * out; 0 gives the rest layout). 0 for invalid arguments. */
int dock_magnify_frame(const struct DockLayout *layout, const struct DockStyle *style,
                       const struct DockMagnifyParams *params, int pointer, int strength,
                       struct DockMagnifyFrame *frame);

/* 1 when slot 'index' differs between two frames (icon or highlight). */
int dock_magnify_changed(const struct DockMagnifyFrame *a, const struct DockMagnifyFrame *b, int index);

#endif
