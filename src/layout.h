#ifndef DOCK_LAYOUT_H
#define DOCK_LAYOUT_H
#include "model.h"

/* One geometry for a dock: painting, hover, clicks, drops, insertion marks
 * and popup anchors all read this result, so they cannot disagree. It is
 * computed on a main axis (along the bar) and a cross axis, then mapped to
 * window coordinates for the orientation. */

enum DockOrientation { DOCK_HORIZONTAL = 0, DOCK_VERTICAL = 1 };
enum DockLabels { DOCK_LABELS_NONE = 0, DOCK_LABELS_HOVER = 1, DOCK_LABELS_ALWAYS = 2 };

struct DockStyle {
    int icon;          /* icon box, 32..72 */
    int orientation;
    int spacing;       /* extra space between items, 0..16 */
    int labels;
    int text_height;   /* screen font height, for labels */
    int hide_grip;     /* 1: no grip; the bar starts with a plain margin */
    /* Text block of each widget item (clock), measured by the caller in
     * pixels; 0 for other items. */
    int content_w[DOCK_MAX_ITEMS], content_h[DOCK_MAX_ITEMS];
    /* Per item: DockPresentation already resolved against the dock
     * (DOCK_PRESENT_ICON, _TEXT or _ICON_TEXT; 0 counts as icon), and for
     * text the width of the caption as it will be drawn (fitted to
     * DOCK_TEXT_MAX by the caller). Ignored for separators, spaces, clocks. */
    int present[DOCK_MAX_ITEMS], text_w[DOCK_MAX_ITEMS];
    int beside;        /* icon and text: 1 caption right of the icon, 0 under it */
};

/* Text buttons: the caption with this room inside the button, at most this
 * wide (longer names end in "..." and show in full as a tooltip), a gap to
 * the next button, and room for the mark that tells a group from a program. */
#define DOCK_TEXT_PAD_X 10
#define DOCK_TEXT_PAD_Y 5
#define DOCK_TEXT_MAX 180
#define DOCK_BUTTON_GAP 3
#define DOCK_GROUP_MARK 12
#define DOCK_ICON_TEXT_GAP 6
#define DOCK_ICON_TEXT_GAP_UNDER 2

/* Highlights (hover, pressed, drop target) of every item: the content with
 * this much room around it, this radius, and never closer than the margin to
 * the bar's edge, so they cannot touch the frame. */
#define DOCK_HIGHLIGHT_PAD 4
#define DOCK_HIGHLIGHT_RADIUS 6
#define DOCK_HIGHLIGHT_MARGIN 3

struct DockRect { int x, y, w, h; };

struct DockSlot {
    struct DockRect slot;  /* whole item, including padding */
    struct DockRect icon;  /* square icon box; zero size for non-icon items */
    struct DockRect label; /* text line under the icon when labels are always on */
    struct DockRect content;   /* what is drawn: icon box or widget text block */
    struct DockRect highlight; /* hover/pressed/target box around the content;
                                * for a text button the button itself */
    struct DockRect text;  /* caption of a text button; zero size otherwise */
    struct DockRect mark;  /* group mark inside a text button; zero size otherwise */
    int button;            /* 1 for a text or icon-and-text button */
    int kind;
    int hittable;          /* separators and spacers are not targets */
};

#define DOCK_GRIP 20
#define DOCK_EDGE_PAD 8

struct DockLayout {
    int width, height, orientation, count;
    int start;             /* main-axis position of the first item */
    struct DockRect grip;  /* zero size when hidden */
    struct DockSlot slots[DOCK_MAX_ITEMS];
};

/* 1 on success; 0 for invalid style or model. */
int dock_layout(const struct DockModel *model, const struct DockStyle *style,
                struct DockLayout *layout);
/* Window coordinates. Returns the item index, or -1 (grip, gaps, separators):
 * what a click starts, hover shows and a drop lands on. */
int dock_layout_hit(const struct DockLayout *layout, int x, int y);
/* Any item under (x, y), separators and spaces included: what a right
 * click or Shift-click edits and a drag moves. -1 on the grip and gaps. */
int dock_layout_item(const struct DockLayout *layout, int x, int y);
/* Insertion index 0..count for a drop at (x, y), by the main axis only. */
int dock_layout_before(const struct DockLayout *layout, int x, int y);
/* 1 when (x, y) is in the middle half of a hittable slot (grouping dwell). */
int dock_layout_center(const struct DockLayout *layout, int index, int x, int y);
/* Main-axis position of the insertion mark before item 'before'. */
int dock_layout_mark(const struct DockLayout *layout, int before);
int dock_layout_in_grip(const struct DockLayout *layout, int x, int y);
/* Moves every rectangle of a slot by (dx, dy): how a scrolled dock draws
 * it. One place, so that no part (caption, mark, icon) stays behind. */
void dock_layout_shift(struct DockSlot *slot, int dx, int dy);

#endif
