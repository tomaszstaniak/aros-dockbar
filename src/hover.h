#ifndef DOCK_HOVER_H
#define DOCK_HOVER_H
#include "magnify.h"

/* Hover effects on top of a finished DockLayout: Lift moves the icon under
 * the pointer a few pixels towards the inside of the screen, Magnify (see
 * magnify.h) grows it and its neighbours. Both work on the same frame
 * (struct DockMagnifyFrame) and keep every icon inside its own slot, so
 * clicks, drops and marks use the rest layout and the window's size and
 * shape never change. Only plain icons take part (dock_hover_item). */

/* Lift: at most this many pixels, less where the bar has no room. */
#define DOCK_LIFT_MAX 4
/* A full change of an effect, in or out, takes this long; a change of
 * direction on the way continues from where the effect is. */
#define DOCK_HOVER_RAMP_MS 130

/* Which way icons lift, across the bar: -1 towards smaller coordinates (up
 * on a horizontal bar, left on a vertical one), +1 the other way. 'placement'
 * is a DockPlacement: a bottom dock lifts up, a top one down, a dock at the
 * left edge to the right, at the right edge to the left; a free dock (and a
 * bar that does not lie along its edge) up when horizontal, right when
 * vertical. */
int dock_lift_sign(int placement, int orientation);

/* Pixels slot 'index' can lift by in direction 'sign': up to DOCK_LIFT_MAX,
 * never past DOCK_HIGHLIGHT_MARGIN from the bar's edge or into an
 * always-shown label. 0 for items that do not take part. */
int dock_lift_room(const struct DockLayout *layout, int index, int sign);

/* One Lift frame. progress[i] is slot i's eased progress, 0..1000; each
 * icon keeps its size and moves by its room times that progress, rounded.
 * Highlights stay where the rest layout has them. 0 for invalid arguments. */
int dock_lift_frame(const struct DockLayout *layout, int sign, const int progress[DOCK_MAX_ITEMS],
                    struct DockMagnifyFrame *frame);

/* Progress 0..1000 moved towards target (0 or 1000) by elapsed_ms at the
 * rate of one full change per DOCK_HOVER_RAMP_MS. */
int dock_hover_ramp(int current, int target, unsigned elapsed_ms);
/* Smoothstep of a progress 0..1000: starts and ends gently. */
int dock_hover_ease(int progress);

/* What has to be painted again when slot 'index' goes from frame a to frame
 * b: the union of its icon and highlight boxes in both. Zero size when they
 * are the same. Layout coordinates. */
struct DockRect dock_hover_damage(const struct DockMagnifyFrame *a, const struct DockMagnifyFrame *b, int index);

#endif
