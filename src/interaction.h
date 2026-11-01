#ifndef DOCK_INTERACTION_H
#define DOCK_INTERACTION_H

struct DockDrag {
    int kind, container, index, start_x, start_y, active;
    int target_container, target_index, armed;
    unsigned int since;
};

/* kind: 0 none, 1 item, 2 bar. A gesture becomes active when either axis
 * moves at least 6 pixels. Pass positions in one consistent coordinate space. */
void dock_drag_begin(struct DockDrag *drag, int kind, int container,
                     int index, int x, int y);
/* An active item drag arms grouping after 500 ms over the same eligible
 * target center. index < 0, own source, or center == 0 clears the dwell.
 * Container IDs are opaque (including -1); target_index == -1 means no dwell.
 * now_ms is an unsigned millisecond counter; normal counter wrap is supported.
 * Call periodically while stationary too. No state/model mutation occurs. */
void dock_drag_update(struct DockDrag *drag, int x, int y,
                      int target_container, int target_index, int center,
                      unsigned int now_ms);
void dock_drag_cancel(struct DockDrag *drag);

/* Every drag the docks have to know about, in one record: Dockbar's own (an
 * item or a dock being moved) and one it does not own (the left button held
 * elsewhere, typically an icon dragged from Wanderer). Hover effects,
 * tooltips, desktop pictures and redraws all ask this record, and every drag
 * ends through dock_drag_state_end, so no part keeps a state of its own. */
struct DockDragState {
    struct DockDrag own;
    int foreign;        /* the button held for a drag that is not ours, as of the last poll */
    /* A group held under a foreign drag opens after a while: where and since when. */
    int spring_view, spring_item;
    unsigned int spring_since;
};
void dock_drag_state_init(struct DockDragState *state);
/* Once per pass of the main loop, with the left button as it is now. Returns 1
 * when a foreign drag ended since the last call: the drag image may have put
 * back old pixels over the docks, so they are painted once more. */
int dock_drag_state_poll(struct DockDragState *state, int left_held);
/* 1 while something is being dragged: an own drag past its threshold (a
 * press that has not moved is a click, not a drag) or a foreign one. Hover
 * effects go back to rest and stay there meanwhile. */
int dock_drag_state_busy(const struct DockDragState *state);
/* The one way a drag ends, whether dropped, cancelled or cut short by a
 * screen change. Returns the own drag as it was, so the caller can undo what
 * it had shown (a moved window, pressed or target marks). */
struct DockDrag dock_drag_state_end(struct DockDragState *state);

/* Which dock a point belongs to, from the frontmost layer the system finds
 * there (WhichLayer(), the same answer that decides where a click goes).
 * layers[i] is dock i's window layer (NULL for a dock without a window); own
 * is a window of ours that must never take a point away from a dock, the
 * tooltip. Returns the dock's index, -1 for anything else in front (another
 * window, no layer), or DOCK_FRONT_OWN for own. Pointers are only compared. */
#define DOCK_FRONT_OWN (-2)
int dock_front_pick(const void *front, const void *const *layers, int count,
                    const void *own);

#endif
