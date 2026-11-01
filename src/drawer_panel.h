#ifndef DOCK_DRAWER_PANEL_H
#define DOCK_DRAWER_PANEL_H

#include "model.h"
#include "native_icons.h"
#include "chrome.h"
#include "theme.h"
#include "icon_cache.h"

struct MsgPort;

/* A drawer's current contents in a panel beside its dock item. Nothing of it
 * is saved: the panel reads the drawer each time it opens, one level only.
 * Reading and icon loading go in steps limited by time (dock_drawer_step) so
 * that a large drawer does not stop the dock. No lock on the drawer is kept
 * once reading is done; the panel accepts no drops (it neither copies nor
 * moves files). Zero-initialize before first use.
 *
 * Reading again (Refresh, a change notice) keeps the current list on show
 * until the new one is complete. Pictures live in a cache shared by all
 * openings of the panel, within a memory budget; an entry is made again only
 * when its source changed (the .info file's date or size, as read with the
 * listing), the icon size changed, or the screen did (dock_drawer_cache_flush).
 * Rows on show come first, then a page below and above. */
struct DockDrawerEntry {
    char name[108];
    int is_dir;
    int icon_state;  /* 0 not looked up, 1 in the cache, 2 could not be loaded */
    int source_known;  /* sig comes from a complete listing */
    int looked;  /* sig looked up early, before the listing was complete */
    struct DockCacheSig sig;  /* the picture's source, see above */
};
struct DockDrawerInfo { char name[108]; struct DockCacheSig sig; };

enum DockDrawerHit {
    DOCK_DRAWER_HIT_NONE = 0,
    DOCK_DRAWER_HIT_ENTRY,
    DOCK_DRAWER_HIT_BACK,
    DOCK_DRAWER_HIT_REFRESH,
    DOCK_DRAWER_HIT_WANDERER,
    DOCK_DRAWER_HIT_PAGE_UP,
    DOCK_DRAWER_HIT_PAGE_DOWN
};

#define DOCK_DRAWER_MAX_ENTRIES 4000

enum DockDrawerTimingReason {
    DOCK_DRAWER_TIME_OPEN = 1,  /* panel opened, or another drawer shown in it */
    DOCK_DRAWER_TIME_REFRESH,   /* Refresh pressed */
    DOCK_DRAWER_TIME_NOTIFY,    /* read again after a change notice */
    DOCK_DRAWER_TIME_SCROLL
};

/* How long the panel took to show something, from the moment it was asked:
 * the first entries, the first real icon, and every icon on show. Times in
 * milliseconds after started; 0 not reached yet. For the log only. */
struct DockDrawerTiming {
    int reason;
    unsigned int started, list, first_icon, visible;
    unsigned int listed;  /* the new listing complete */
    unsigned long loads, hits, read_us, scale_us, draws, draw_us;  /* during this measurement */
    unsigned long base_loads, base_read_us, base_scale_us;
    int reported;
};

struct DockDrawerPanel {
    struct Window *window;
    struct Screen *screen;
    struct DockBuffer buffer;
    struct Region *shape;
    const struct DockColors *colors;
    long text_pen, muted_pen;
    char root[DOCK_PATH], path[DOCK_PATH];
    struct DockDrawerEntry *entries;  /* on show */
    int count, capacity, truncated;
    /* Being read while the old list stays on show; empty when reading
     * straight into entries (first read, another drawer). */
    struct DockDrawerEntry *next;
    int next_count, next_capacity, next_truncated, into_shown;
    struct DockDrawerInfo *infos;  /* .info files seen by the current read */
    int info_count, info_capacity;
    unsigned long stamp;  /* step counter for the cache */
    int dirty;  /* something new to draw */
    unsigned int drawn_at, read_end_at;
    int notice_waiting;  /* a change notice not acted on yet (too soon after a read) */
    /* Reading: a lock while scanning, nothing after. */
    long lock;  /* BPTR */
    void *fib;  /* struct FileInfoBlock, while scanning and between scans */
    int scanning;
    long error;  /* IoErr() of a failed read, 0 when fine */
    /* Change notification on the shown drawer, when its file system has it. */
    struct MsgPort *notify_port;
    void *notify;  /* struct NotifyRequest */
    int notifying;
    /* A change reported while the drawer was being read: the part read
     * already may be stale, so it is read once more when this read ends.
     * Any number of notices meanwhile cost one more read, not a restart. */
    int stale;
    unsigned long reads;  /* completed reads, for tests and the log */
    unsigned long stale_reads;  /* of those, reads again after a change during reading */
    int read_count;  /* entries of the last completed read, before any read again */
    struct DockDrawerTiming timing;
    int size, width, height, columns, rows, cell_width, cell_height;
    int grid_x, grid_y, footer_y, top_row, hover, pressed, hot;
};

int dock_drawer_open(struct DockDrawerPanel *panel, struct Screen *screen, const char *path,
                     int size, int anchor_x, int anchor_y, int anchor_width, int anchor_height,
                     int vertical, const struct DockColors *colors);
void dock_drawer_close(struct DockDrawerPanel *panel);
/* Shows another drawer in the same panel (a subdrawer, or Back). */
int dock_drawer_show(struct DockDrawerPanel *panel, const char *path);
/* Reads the shown drawer again from the start. */
void dock_drawer_refresh(struct DockDrawerPanel *panel);
/* One step of reading and icon loading; 1 when something new is to draw
 * (at most every 80 ms while work goes on). Also answers change
 * notifications (then reads the drawer again, at most twice a second). */
int dock_drawer_step(struct DockDrawerPanel *panel);
/* 1 while reading or icons on show (or the page around) are still to come:
 * the caller should step again soon instead of idling. */
int dock_drawer_busy(const struct DockDrawerPanel *panel);
/* The shared picture cache: flush when the screen changes, or after the
 * panel has been closed a while, to give the memory back. */
void dock_drawer_cache_flush(void);
const struct DockIconCache *dock_drawer_cache(void);
void dock_drawer_draw(struct DockDrawerPanel *panel);
/* Coordinates relative to the panel window. *index is set for entries. */
int dock_drawer_hit(const struct DockDrawerPanel *panel, int x, int y, int *index);
void dock_drawer_scroll(struct DockDrawerPanel *panel, int rows);
/* Full path of an entry; 0 when it does not fit. */
int dock_drawer_entry_path(const struct DockDrawerPanel *panel, int index, char *out, unsigned long capacity);

#endif
