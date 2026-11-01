#include "drawer_panel.h"
#include "dirlist.h"
#include "elapsed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <dos/notify.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/* Work per step, in milliseconds of the dock's main loop: enough to read a
 * few hundred names or several icons, short enough that the dock answers. */
#define SCAN_BUDGET_MS 20
#define ICON_BUDGET_MS 25
#define DRAW_EVERY_MS 80
#define FOOTER_EVERY_MS 250
/* A drawer that keeps changing is read again at most this often. */
#define NOTICE_GAP_MS 500
/* Pictures kept between openings. A 48 px own icon costs about 30 KB with
 * icon.library's own copy; a default (identified) icon less. */
#define CACHE_BUDGET (3UL * 1024 * 1024)

static struct DockIconCache cache;

static void free_cached(void *value)
{
    dock_native_free(value);
    free(value);
}

static struct DockIconCache *the_cache(void)
{
    if (!cache.buckets) dock_cache_init(&cache, CACHE_BUDGET, free_cached);
    return cache.buckets ? &cache : NULL;
}

void dock_drawer_cache_flush(void)
{
    if (cache.buckets) dock_cache_flush(&cache);
}

const struct DockIconCache *dock_drawer_cache(void)
{
    return &cache;
}

static long pen_for(struct Screen *screen, unsigned long rgb)
{
    return ObtainBestPen(screen->ViewPort.ColorMap, ((rgb >> 16) & 255) * 0x01010101UL,
                         ((rgb >> 8) & 255) * 0x01010101UL, (rgb & 255) * 0x01010101UL, TAG_DONE);
}

static int clamp(int value, int low, int high)
{
    if (value < low) return low;
    return value > high ? high : value;
}

static void text(struct RastPort *rp, int x, int y, const char *value, long pen)
{
    SetAPen(rp, (ULONG)pen);
    SetDrMd(rp, JAM1);
    Move(rp, x, y);
    Text(rp, (CONST_STRPTR)value, (ULONG)strlen(value));
}

static void fit_label(struct RastPort *rp, char *out, size_t capacity, const char *value, int width)
{
    size_t length;
    snprintf(out, capacity, "%s", value);
    length = strlen(out);
    if (TextLength(rp, (CONST_STRPTR)out, (ULONG)length) <= width) return;
    while (length > 0) {
        out[--length] = '\0';
        if (length + 3 < capacity) {
            memcpy(out + length, "...", 4);
            if (TextLength(rp, (CONST_STRPTR)out, (ULONG)(length + 3)) <= width) return;
            out[length] = '\0';
        }
    }
}

/* A missing disk or assign must not stop the dock behind an "insert volume"
 * requester while the panel reads; the panel says so instead. */
static APTR quiet_begin(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    return old;
}

static void quiet_end(APTR old)
{
    ((struct Process *)FindTask(NULL))->pr_WindowPtr = old;
}

static unsigned int now_ms(void)
{
    return (unsigned int)dock_elapsed_ms();
}

static void timing_begin(struct DockDrawerPanel *panel, int reason)
{
    struct DockDrawerTiming *t = &panel->timing;
    memset(t, 0, sizeof(*t));
    t->reason = reason;
    t->started = now_ms();
    t->base_loads = dock_native_timing.loads;
    t->base_read_us = dock_native_timing.read_us;
    t->base_scale_us = dock_native_timing.scale_us;
}

static int visible_range(const struct DockDrawerPanel *panel, int *first, int *last)
{
    *first = panel->top_row * panel->columns;
    *last = *first + panel->rows * panel->columns;
    if (*last > panel->count) *last = panel->count;
    return *last > *first;
}

/* Milestones of the current measurement, checked after every step. */
static void timing_check(struct DockDrawerPanel *panel)
{
    struct DockDrawerTiming *t = &panel->timing;
    unsigned int elapsed;
    int first, last, waiting = 0, shown = 0;
    if (!t->reason || t->visible) return;
    elapsed = now_ms() - t->started;
    if (!elapsed) elapsed = 1;
    if (!t->list && (panel->count > 0 || (!panel->scanning && !panel->stale))) t->list = elapsed;
    if (!t->listed && !panel->scanning && !panel->stale && !panel->notice_waiting) t->listed = elapsed;
    visible_range(panel, &first, &last);
    for (int i = first; i < last; ++i) {
        if (panel->entries[i].icon_state == 1) shown = 1;
        else if (panel->entries[i].icon_state == 0) waiting = 1;
    }
    if (!t->first_icon && shown) t->first_icon = elapsed;
    if (!waiting && t->listed) {
        t->visible = elapsed;
        t->loads = dock_native_timing.loads - t->base_loads;
        t->read_us = dock_native_timing.read_us - t->base_read_us;
        t->scale_us = dock_native_timing.scale_us - t->base_scale_us;
    }
}

static void free_entries(struct DockDrawerPanel *panel)
{
    panel->count = 0;
    panel->truncated = 0;
}

static void stop_scan(struct DockDrawerPanel *panel)
{
    if (panel->lock) UnLock((BPTR)panel->lock);
    panel->lock = 0;
    panel->scanning = 0;
    panel->next_count = 0;
    panel->next_truncated = 0;
    panel->info_count = 0;
}

static void stop_notify(struct DockDrawerPanel *panel)
{
    struct Message *message;
    if (panel->notifying) EndNotify((struct NotifyRequest *)panel->notify);
    panel->notifying = 0;
    panel->notice_waiting = 0;
    if (panel->notify_port)
        while ((message = GetMsg(panel->notify_port))) ReplyMsg(message);
}

static void start_notify(struct DockDrawerPanel *panel)
{
    struct NotifyRequest *request = panel->notify;
    stop_notify(panel);
    if (!request || !panel->notify_port) return;
    memset(request, 0, sizeof(*request));
    request->nr_Name = (STRPTR)panel->path;
    request->nr_Flags = NRF_SEND_MESSAGE;
    request->nr_stuff.nr_Msg.nr_Port = panel->notify_port;
    panel->notifying = StartNotify(request) ? 1 : 0;
}

static int compare_entries(const void *a, const void *b)
{
    const struct DockDrawerEntry *x = a, *y = b;
    return dock_dir_compare(x->is_dir, x->name, y->is_dir, y->name);
}

static int fold(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* Names without regard to case only: an object and its .info match so. */
static int compare_folded(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    for (; *p && fold(*p) == fold(*q); p++, q++) {}
    return fold(*p) - fold(*q);
}

static int compare_infos(const void *a, const void *b)
{
    return compare_folded(((const struct DockDrawerInfo *)a)->name, ((const struct DockDrawerInfo *)b)->name);
}

/* What a picture is made from: the .info file when there is one, else the
 * object itself (its type decides the default icon). */
static void make_sig(struct DockCacheSig *sig, int own, int is_dir, const struct DateStamp *date, long size)
{
    sig->a = (unsigned long)(own ? 1 : 2) | (unsigned long)(is_dir ? 16 : 0);
    sig->b = (unsigned long)date->ds_Days;
    sig->c = (unsigned long)date->ds_Minute * 3000UL + (unsigned long)date->ds_Tick;
    sig->d = (unsigned long)size;
}

static int grow(struct DockDrawerEntry **array, int *capacity, int count)
{
    if (count < *capacity) return 1;
    int wanted = *capacity ? *capacity * 2 : 64;
    struct DockDrawerEntry *grown = realloc(*array, sizeof(*grown) * (size_t)wanted);
    if (!grown) return 0;
    *array = grown;
    *capacity = wanted;
    return 1;
}

static void begin_scan(struct DockDrawerPanel *panel, int keep_shown)
{
    struct FileInfoBlock *fib = panel->fib;
    APTR old;
    stop_scan(panel);
    panel->error = 0;
    panel->stale = 0;
    panel->notice_waiting = 0;
    /* With nothing on show there is nothing to keep: read straight into
     * the list, so that the first names appear at once. */
    panel->into_shown = !keep_shown || panel->count == 0;
    if (panel->into_shown) {
        free_entries(panel);
        panel->top_row = 0;
        panel->hover = panel->pressed = -1;
    }
    panel->dirty = 1;
    if (!fib) { panel->error = ERROR_NO_FREE_STORE; return; }
    old = quiet_begin();
    panel->lock = (long)Lock((CONST_STRPTR)panel->path, SHARED_LOCK);
    if (!panel->lock || !Examine((BPTR)panel->lock, fib)) panel->error = IoErr();
    else if (fib->fib_DirEntryType <= 0) panel->error = ERROR_OBJECT_WRONG_TYPE;
    quiet_end(old);
    if (panel->error) {
        stop_scan(panel);
        free_entries(panel);
        panel->top_row = 0;
        panel->hover = panel->pressed = -1;
        return;
    }
    panel->scanning = 1;
}

/* The listing is complete: give every entry the source of its picture from
 * the .info files seen, sort, and put it on show. */
static void finish_scan(struct DockDrawerPanel *panel)
{
    struct DockDrawerEntry *list = panel->into_shown ? panel->entries : panel->next;
    int count = panel->into_shown ? panel->count : panel->next_count;
    qsort(panel->infos, (size_t)panel->info_count, sizeof(*panel->infos), compare_infos);
    for (int i = 0; i < count; ++i) {
        struct DockDrawerInfo key, *info;
        snprintf(key.name, sizeof(key.name), "%s", list[i].name);
        info = panel->info_count ? bsearch(&key, panel->infos, (size_t)panel->info_count,
                                           sizeof(*panel->infos), compare_infos) : NULL;
        if (info) {
            info->sig.a |= list[i].is_dir ? 16 : 0;
            if (memcmp(&info->sig, &list[i].sig, sizeof(info->sig))) list[i].icon_state = 0;
            list[i].sig = info->sig;
        } else if (list[i].sig.a & 1) {
            /* Looked up early with an .info that this listing did not see. */
            list[i].sig.a &= ~1UL;
            list[i].sig.a |= 2;
            list[i].icon_state = 0;
        }
        list[i].source_known = 1;
    }
    qsort(list, (size_t)count, sizeof(*list), compare_entries);
    if (!panel->into_shown) {
        struct DockDrawerEntry *old = panel->entries;
        int old_capacity = panel->capacity;
        panel->entries = panel->next;
        panel->capacity = panel->next_capacity;
        panel->count = panel->next_count;
        panel->truncated = panel->next_truncated;
        panel->next = old;
        panel->next_capacity = old_capacity;
        /* Indexes point into the old list. */
        panel->hover = panel->pressed = -1;
    }
    panel->next_count = 0;
    panel->info_count = 0;
    panel->into_shown = 0;
    if (panel->lock) UnLock((BPTR)panel->lock);
    panel->lock = 0;
    panel->scanning = 0;
    panel->read_end_at = now_ms();
    panel->dirty = 1;
}

/* Names with ExNext for up to SCAN_BUDGET_MS. Not ExAll: on RAM: (whose
 * handler has no EXAMINE_ALL, so dos.library emulates it) one name was lost
 * at every buffer boundary, 12 of 1500 (tests/exall_probe.c). */
static void scan_step(struct DockDrawerPanel *panel)
{
    struct FileInfoBlock *fib = panel->fib;
    unsigned int start = now_ms();
    int added = 0, done = 0;
    APTR old;
    if (!panel->scanning) return;
    old = quiet_begin();
    for (int n = 0; !done; ++n) {
        if ((n & 15) == 15 && now_ms() - start >= SCAN_BUDGET_MS) break;
        if (!ExNext((BPTR)panel->lock, fib)) {
            if (IoErr() != ERROR_NO_MORE_ENTRIES) panel->error = IoErr();
            done = 1;
            break;
        }
        const char *name = (const char *)fib->fib_FileName;
        char base[108];
        int is_dir = fib->fib_DirEntryType > 0 && fib->fib_DirEntryType != ST_SOFTLINK;
        if (dock_dir_info_base(name, base, sizeof(base))) {
            if (panel->info_count == panel->info_capacity) {
                int wanted = panel->info_capacity ? panel->info_capacity * 2 : 64;
                struct DockDrawerInfo *grown = realloc(panel->infos, sizeof(*grown) * (size_t)wanted);
                if (!grown) continue;  /* that entry keeps its object's source */
                panel->infos = grown;
                panel->info_capacity = wanted;
            }
            struct DockDrawerInfo *info = &panel->infos[panel->info_count++];
            snprintf(info->name, sizeof(info->name), "%s", base);
            make_sig(&info->sig, 1, 0, &fib->fib_Date, fib->fib_Size);
            continue;
        }
        if (!dock_dir_listed(name) || strlen(name) >= sizeof(panel->entries[0].name)) continue;
        struct DockDrawerEntry **list = panel->into_shown ? &panel->entries : &panel->next;
        int *count = panel->into_shown ? &panel->count : &panel->next_count;
        int *capacity = panel->into_shown ? &panel->capacity : &panel->next_capacity;
        int *truncated = panel->into_shown ? &panel->truncated : &panel->next_truncated;
        if (*count >= DOCK_DRAWER_MAX_ENTRIES || !grow(list, capacity, *count)) { *truncated = 1; continue; }
        struct DockDrawerEntry *e = &(*list)[(*count)++];
        memset(e, 0, sizeof(*e));
        snprintf(e->name, sizeof(e->name), "%s", name);
        e->is_dir = is_dir;
        make_sig(&e->sig, 0, is_dir, &fib->fib_Date, fib->fib_Size);
        ++added;
    }
    quiet_end(old);
    if (done) { finish_scan(panel); return; }
    if (added && panel->into_shown) {
        /* Sorted as it grows, so that the top rows settle early; loaded
         * pictures move with their entries. */
        qsort(panel->entries, (size_t)panel->count, sizeof(*panel->entries), compare_entries);
        panel->hover = panel->pressed = -1;
        panel->dirty = 1;
    }
}

/* Source of an entry's picture before the listing is complete: the .info
 * may not have been seen yet, so look at it directly. */
static void early_sig(struct DockDrawerPanel *panel, struct DockDrawerEntry *e, const char *path)
{
    char info[DOCK_PATH + 130];
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    BPTR lock;
    if (!fib) return;
    snprintf(info, sizeof(info), "%s.info", path);
    if ((lock = Lock((CONST_STRPTR)info, SHARED_LOCK))) {
        if (Examine(lock, fib)) make_sig(&e->sig, 1, e->is_dir, &fib->fib_Date, fib->fib_Size);
        UnLock(lock);
    }
    FreeDosObject(DOS_FIB, fib);
    (void)panel;
}

static unsigned long icon_cost(const DockNativeIcon *icon)
{
    unsigned long cost = sizeof(*icon) + 2048;
    for (int s = 0; s < 2; ++s)
        if (icon->pixels[s]) cost += (unsigned long)icon->pixel_width[s] * (unsigned long)icon->pixel_height[s] * 4UL;
    /* icon.library keeps the decoded images of both states. */
    cost += (unsigned long)icon->source_width * (unsigned long)icon->source_height * 8UL;
    return cost;
}

/* The picture for entry i: from the cache, or made now when allowed.
 * Returns 1 when something new is to draw. */
static int fetch_icon(struct DockDrawerPanel *panel, struct DockIconCache *c, int i, int may_load)
{
    char path[DOCK_PATH + 120];
    struct DockDrawerEntry *e = &panel->entries[i];
    if (e->icon_state == 2) return 0;
    if (!dock_dir_join(panel->path, e->name, path, sizeof(path))) { e->icon_state = 2; return 1; }
    /* Before the listing is complete the .info may not have been seen:
     * asking the cache with the object's own source would throw away a
     * picture made from the .info. */
    if (!e->source_known && !e->looked) { early_sig(panel, e, path); e->looked = 1; }
    if (dock_cache_get(c, path, panel->size, &e->sig, panel->stamp)) {
        int fresh = e->icon_state != 1;
        if (fresh) panel->timing.hits++;
        e->icon_state = 1;
        return fresh;
    }
    if (e->icon_state == 1) e->icon_state = 0;  /* taken out of the cache meanwhile */
    if (!may_load) return 0;
    DockNativeIcon *icon = calloc(1, sizeof(*icon));
    if (icon && dock_native_load(icon, path, panel->screen, panel->size) &&
        dock_cache_put(c, path, panel->size, &e->sig, icon, icon_cost(icon), panel->stamp)) {
        e->icon_state = 1;
        return 1;
    }
    if (icon) { dock_native_free(icon); free(icon); }
    e->icon_state = 2;
    return 1;
}

/* Rows on show first, then a page below and a page above, until the time
 * for this step is used up. Cached pictures cost almost nothing. */
static int icon_step(struct DockDrawerPanel *panel)
{
    struct DockIconCache *c = the_cache();
    unsigned int start = now_ms();
    int first, last, changed = 0, page;
    APTR old;
    if (!c || !panel->count || !visible_range(panel, &first, &last)) return 0;
    page = panel->rows * panel->columns;
    old = quiet_begin();
    /* Keep what is on show from being evicted by this step's loads. */
    for (int i = first; i < last; ++i) changed |= fetch_icon(panel, c, i, 0);
    for (int pass = 0; pass < 3; ++pass) {
        int from = pass == 0 ? first : pass == 1 ? last : first - page;
        int to = pass == 0 ? last : pass == 1 ? last + page : first;
        if (from < 0) from = 0;
        if (to > panel->count) to = panel->count;
        /* Pages around wait for a complete listing: they still move. */
        if (pass > 0 && panel->into_shown) break;
        for (int i = from; i < to; ++i) {
            if (panel->entries[i].icon_state) continue;
            if (now_ms() - start >= ICON_BUDGET_MS && (changed || pass > 0)) goto out;
            int fresh = fetch_icon(panel, c, i, 1);
            if (pass == 0) changed |= fresh;
        }
    }
out:
    quiet_end(old);
    return changed;
}

int dock_drawer_busy(const struct DockDrawerPanel *panel)
{
    int first, last, page;
    if (!panel || !panel->window) return 0;
    if (panel->scanning || panel->notice_waiting || panel->stale || panel->dirty) return 1;
    if (!visible_range(panel, &first, &last)) return 0;
    page = panel->rows * panel->columns;
    first = first - page < 0 ? 0 : first - page;
    last = last + page > panel->count ? panel->count : last + page;
    for (int i = first; i < last; ++i)
        if (!panel->entries[i].icon_state) return 1;
    return 0;
}

void dock_drawer_close(struct DockDrawerPanel *panel)
{
    if (!panel) return;
    stop_scan(panel);
    stop_notify(panel);
    if (panel->notify_port) dock_quiet_port_free(panel->notify_port);
    free(panel->notify);
    free_entries(panel);
    free(panel->entries);
    free(panel->next);
    free(panel->infos);
    if (panel->fib) FreeDosObject(DOS_FIB, panel->fib);
    if (panel->window) {
        struct Message *message;
        while ((message = GetMsg(panel->window->UserPort))) ReplyMsg(message);
        CloseWindow(panel->window);
    }
    if (panel->shape) DisposeRegion(panel->shape);
    dock_buffer_close(&panel->buffer);
    if (panel->screen) {
        if (panel->text_pen >= 0) ReleasePen(panel->screen->ViewPort.ColorMap, (ULONG)panel->text_pen);
        if (panel->muted_pen >= 0) ReleasePen(panel->screen->ViewPort.ColorMap, (ULONG)panel->muted_pen);
    }
    memset(panel, 0, sizeof(*panel));
    panel->text_pen = panel->muted_pen = -1;
    panel->hover = panel->pressed = panel->hot = -1;
}

int dock_drawer_open(struct DockDrawerPanel *panel, struct Screen *screen, const char *path,
                     int size, int anchor_x, int anchor_y, int anchor_width, int anchor_height,
                     int vertical, const struct DockColors *colors)
{
    int font_height, header, footer, left, top, max_rows;
    if (!panel || !screen || !path || !*path || strlen(path) >= sizeof(panel->path) ||
        size < 16 || size > 128) return 0;
    dock_drawer_close(panel);
    panel->screen = screen;
    snprintf(panel->root, sizeof(panel->root), "%s", path);
    snprintf(panel->path, sizeof(panel->path), "%s", path);
    panel->size = size > 48 ? 48 : size;
    panel->columns = 5;
    font_height = screen->RastPort.Font->tf_YSize;
    header = font_height + 22;
    footer = font_height + 14;
    panel->cell_width = panel->size + 40;
    if (panel->cell_width < 84) panel->cell_width = 84;
    panel->cell_height = panel->size + font_height + 18;
    max_rows = (screen->Height * 3 / 5 - header - footer) / panel->cell_height;
    panel->rows = clamp(max_rows, 1, 5);
    panel->width = panel->columns * panel->cell_width + 24 + 14;
    if (panel->width > screen->Width - 16) {
        panel->columns = (screen->Width - 16 - 38) / panel->cell_width;
        if (panel->columns < 2) return 0;
        panel->width = panel->columns * panel->cell_width + 38;
    }
    panel->height = header + panel->rows * panel->cell_height + footer;
    panel->grid_x = 12;
    panel->grid_y = header;
    panel->footer_y = header + panel->rows * panel->cell_height;
    panel->hover = panel->pressed = panel->hot = -1;
    panel->colors = colors ? colors : dock_colors(0);
    panel->text_pen = pen_for(screen, panel->colors->text);
    panel->muted_pen = pen_for(screen, panel->colors->text_muted);
    panel->fib = AllocDosObject(DOS_FIB, NULL);
    panel->notify = calloc(1, sizeof(struct NotifyRequest));
    panel->notify_port = dock_quiet_port();
    if (panel->text_pen < 0 || panel->muted_pen < 0 || !panel->fib) goto failed;
    if (vertical) {
        left = anchor_x + anchor_width + 8;
        if (left + panel->width > screen->Width - 8) left = anchor_x - panel->width - 8;
        top = anchor_y + anchor_height / 2 - panel->height / 2;
    } else {
        left = anchor_x + anchor_width / 2 - panel->width / 2;
        top = anchor_y - panel->height - 8;
        if (top < screen->BarHeight + 8) top = anchor_y + anchor_height + 8;
    }
    left = clamp(left, 8, screen->Width - panel->width - 8);
    top = clamp(top, screen->BarHeight + 8, screen->Height - panel->height - 8);
    panel->shape = dock_shape(panel->width, panel->height, dock_corner_radius);
    if (!panel->shape || !dock_buffer_open(&panel->buffer, screen, panel->width, panel->height)) goto failed;
    panel->window = OpenWindowTags(NULL, WA_PubScreen, (IPTR)screen,
                                   WA_Left, left, WA_Top, top,
                                   WA_Width, panel->width, WA_Height, panel->height,
                                   WA_ShapeRegion, (IPTR)panel->shape,
                                   WA_Borderless, TRUE, WA_Activate, TRUE,
                                   WA_RMBTrap, TRUE, WA_ReportMouse, TRUE,
                                   WA_SimpleRefresh, TRUE,
                                   WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
                                   IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW |
                                   IDCMP_REFRESHWINDOW,
                                   TAG_DONE);
    if (!panel->window) goto failed;
    SetFont(panel->window->RPort, screen->RastPort.Font);
    SetFont(&panel->buffer.rp, screen->RastPort.Font);
    timing_begin(panel, DOCK_DRAWER_TIME_OPEN);
    begin_scan(panel, 0);
    start_notify(panel);
    dock_drawer_draw(panel);
    return 1;
failed:
    dock_drawer_close(panel);
    return 0;
}

int dock_drawer_show(struct DockDrawerPanel *panel, const char *path)
{
    if (!panel || !panel->window || !path || strlen(path) >= sizeof(panel->path)) return 0;
    snprintf(panel->path, sizeof(panel->path), "%s", path);
    timing_begin(panel, DOCK_DRAWER_TIME_OPEN);
    begin_scan(panel, 0);
    start_notify(panel);
    dock_drawer_draw(panel);
    return 1;
}

static void reread(struct DockDrawerPanel *panel);

void dock_drawer_refresh(struct DockDrawerPanel *panel)
{
    if (!panel || !panel->window) return;
    timing_begin(panel, DOCK_DRAWER_TIME_REFRESH);
    reread(panel);
}

static void reread(struct DockDrawerPanel *panel)
{
    /* The list on show stays, where the user was looking, until the new
     * listing is complete. */
    begin_scan(panel, 1);
}

int dock_drawer_step(struct DockDrawerPanel *panel)
{
    struct Message *message;
    int total_rows;
    unsigned int now;
    if (!panel || !panel->window) return 0;
    ++panel->stamp;
    if (panel->notify_port) {
        while ((message = GetMsg(panel->notify_port))) { ReplyMsg(message); panel->notice_waiting = 1; }
    }
    now = now_ms();
    if (panel->notice_waiting) {
        if (panel->scanning) { panel->stale = 1; panel->notice_waiting = 0; }
        else if (now - panel->read_end_at >= NOTICE_GAP_MS) {
            panel->notice_waiting = 0;
            if (!panel->timing.reason || panel->timing.visible) timing_begin(panel, DOCK_DRAWER_TIME_NOTIFY);
            reread(panel);
        }
    }
    /* Pictures first: a drawer that keeps changing is read again and
     * again, and its icons must not wait for a quiet moment. */
    panel->dirty |= icon_step(panel);
    if (panel->scanning) {
        scan_step(panel);
        if (!panel->scanning) {
            ++panel->reads;
            panel->read_count = panel->count;
            if (panel->stale) { panel->stale = 0; ++panel->stale_reads; reread(panel); }
        }
        if (panel->into_shown && now - panel->drawn_at >= FOOTER_EVERY_MS) panel->dirty = 1;
    }
    total_rows = (panel->count + panel->columns - 1) / panel->columns;
    if (!panel->scanning && panel->top_row > 0 && panel->top_row > total_rows - panel->rows) {
        panel->top_row = total_rows > panel->rows ? total_rows - panel->rows : 0;
        panel->dirty = 1;
    }
    timing_check(panel);
    if (!panel->dirty) return 0;
    /* Drawing the whole panel costs more than loading a few icons: draw at
     * most every DRAW_EVERY_MS while work goes on. */
    if (now_ms() - panel->drawn_at < DRAW_EVERY_MS && dock_drawer_busy(panel)) return 0;
    return 1;
}

static void button(struct DockDrawerPanel *panel, struct RastPort *rp, int x, int y, const char *label, int hot)
{
    int w = TextLength(rp, (CONST_STRPTR)label, (ULONG)strlen(label)) + 12;
    if (hot) dock_round_fill(rp, x - 6, y - 3, w, rp->Font->tf_YSize + 6, 5,
                             panel->colors->hover, panel->colors->popup_fill);
    text(rp, x, y + rp->Font->tf_Baseline, label, hot ? panel->text_pen : panel->muted_pen);
}

/* Header buttons from the right: Wanderer, Refresh; Back at the left. */
static int button_x(const struct DockDrawerPanel *panel, struct RastPort *rp, int which)
{
    int wanderer = TextLength(rp, (CONST_STRPTR)"Open in Wanderer", 16);
    int refresh = TextLength(rp, (CONST_STRPTR)"Refresh", 7);
    if (which == DOCK_DRAWER_HIT_WANDERER) return panel->width - 14 - wanderer;
    if (which == DOCK_DRAWER_HIT_REFRESH) return panel->width - 14 - wanderer - 18 - refresh;
    return 14;
}

void dock_drawer_draw(struct DockDrawerPanel *panel)
{
    struct RastPort *rp;
    char caption[DOCK_PATH];
    int baseline, total_rows, deeper;
    const struct DockColors *c;
    if (!panel || !panel->window) return;
    unsigned int draw_start = now_ms();
    unsigned long draw_us = 0;
    draw_us = (unsigned long)dock_elapsed_us();
    rp = &panel->buffer.rp;
    c = panel->colors;
    baseline = rp->Font->tf_Baseline;
    struct DockFrameStyle frame = { c->popup_fill, c->popup_fill, c->border_top, c->border_bottom };
    dock_frame_draw(rp, &frame, panel->width, panel->height, dock_corner_radius);
    deeper = strcmp(panel->path, panel->root) != 0;
    int title_x = 14;
    if (deeper) {
        button(panel, rp, 14, 9, "< Back", panel->hot == DOCK_DRAWER_HIT_BACK);
        title_x += TextLength(rp, (CONST_STRPTR)"< Back", 6) + 18;
    }
    const char *name = (const char *)FilePart((CONST_STRPTR)panel->path);
    fit_label(rp, caption, sizeof(caption), *name ? name : panel->path,
              button_x(panel, rp, DOCK_DRAWER_HIT_REFRESH) - 16 - title_x);
    text(rp, title_x, 9 + baseline, caption, panel->text_pen);
    button(panel, rp, button_x(panel, rp, DOCK_DRAWER_HIT_REFRESH), 9, "Refresh", panel->hot == DOCK_DRAWER_HIT_REFRESH);
    button(panel, rp, button_x(panel, rp, DOCK_DRAWER_HIT_WANDERER), 9, "Open in Wanderer", panel->hot == DOCK_DRAWER_HIT_WANDERER);
    for (int r = 0; r < panel->rows; ++r)
        for (int col = 0; col < panel->columns; ++col) {
            int i = (panel->top_row + r) * panel->columns + col;
            int x = panel->grid_x + col * panel->cell_width, y = panel->grid_y + r * panel->cell_height;
            struct DockDrawerEntry *e;
            if (i >= panel->count) break;
            e = &panel->entries[i];
            if (i == panel->hover || i == panel->pressed)
                dock_round_fill(rp, x + 3, y + 2, panel->cell_width - 6, panel->cell_height - 4, 6,
                                i == panel->pressed ? c->pressed : c->hover, c->popup_fill);
            DockNativeIcon *icon = NULL;
            char path[DOCK_PATH + 120];
            if (e->icon_state == 1 && cache.buckets && dock_dir_join(panel->path, e->name, path, sizeof(path))) {
                icon = dock_cache_get(&cache, path, panel->size, &e->sig, panel->stamp);
                if (!icon) e->icon_state = 0;
            }
            if (icon)
                dock_native_draw(icon, rp, x + (panel->cell_width - icon->width) / 2,
                                 y + 6 + (panel->size - icon->height) / 2, i == panel->pressed);
            else
                FillPixelArray(rp, x + (panel->cell_width - 20) / 2, y + 6 + (panel->size - 20) / 2,
                               20, 20, c->preview_border);
            fit_label(rp, caption, sizeof(caption), e->name, panel->cell_width - 8);
            text(rp, x + (panel->cell_width - TextLength(rp, (CONST_STRPTR)caption, (ULONG)strlen(caption))) / 2,
                 y + panel->size + 11 + baseline, caption, panel->text_pen);
        }
    /* Scroll bar: a track with a thumb; a click above or below pages. */
    total_rows = (panel->count + panel->columns - 1) / panel->columns;
    if (total_rows > panel->rows) {
        int track_x = panel->width - 14, track_h = panel->rows * panel->cell_height - 8;
        int thumb_h = clamp(track_h * panel->rows / total_rows, 16, track_h);
        int thumb_y = panel->grid_y + 4 + (track_h - thumb_h) * panel->top_row / (total_rows - panel->rows);
        FillPixelArray(rp, track_x, panel->grid_y + 4, 6, track_h, c->popup_hover);
        FillPixelArray(rp, track_x, thumb_y, 6, thumb_h, c->text_muted);
    }
    /* Footer: the name under the pointer in full, or what the panel knows. */
    if (panel->hover >= 0 && panel->hover < panel->count)
        snprintf(caption, sizeof(caption), "%s%s", panel->entries[panel->hover].name,
                 panel->entries[panel->hover].is_dir ? "  (drawer)" : "");
    else if (panel->error) {
        char reason[80] = "";
        Fault(panel->error, NULL, (STRPTR)reason, sizeof(reason));
        snprintf(caption, sizeof(caption), "Cannot read this drawer: %s", reason);
    } else if (panel->scanning)
        snprintf(caption, sizeof(caption), "Reading... %d", panel->count);
    else if (!panel->count)
        snprintf(caption, sizeof(caption), "This drawer is empty");
    else
        snprintf(caption, sizeof(caption), "%d item%s%s", panel->count, panel->count == 1 ? "" : "s",
                 panel->truncated ? " (more not shown)" : "");
    char shown[DOCK_PATH];
    fit_label(rp, shown, sizeof(shown), caption, panel->width - 28);
    text(rp, 14, panel->footer_y + 6 + baseline, shown, panel->muted_pen);
    dock_buffer_show(&panel->buffer, panel->window);
    panel->timing.draw_us += (unsigned long)dock_elapsed_us() - draw_us;
    panel->timing.draws++;
    panel->dirty = 0;
    panel->drawn_at = draw_start;
}

int dock_drawer_hit(const struct DockDrawerPanel *panel, int x, int y, int *index)
{
    struct RastPort *rp;
    int col, row, i, total_rows;
    if (!panel || !panel->window) return DOCK_DRAWER_HIT_NONE;
    rp = (struct RastPort *)&panel->buffer.rp;
    if (y < panel->grid_y) {
        int fh = rp->Font->tf_YSize;
        int wx = button_x(panel, rp, DOCK_DRAWER_HIT_WANDERER), rx = button_x(panel, rp, DOCK_DRAWER_HIT_REFRESH);
        if (y < 4 || y > 9 + fh + 4) return DOCK_DRAWER_HIT_NONE;
        if (x >= wx - 6) return DOCK_DRAWER_HIT_WANDERER;
        if (x >= rx - 6 && x < rx + TextLength(rp, (CONST_STRPTR)"Refresh", 7) + 6) return DOCK_DRAWER_HIT_REFRESH;
        if (strcmp(panel->path, panel->root) && x < 14 + TextLength(rp, (CONST_STRPTR)"< Back", 6) + 6)
            return DOCK_DRAWER_HIT_BACK;
        return DOCK_DRAWER_HIT_NONE;
    }
    total_rows = (panel->count + panel->columns - 1) / panel->columns;
    if (x >= panel->width - 18 && y < panel->footer_y && total_rows > panel->rows) {
        int track_h = panel->rows * panel->cell_height - 8;
        int thumb_y = panel->grid_y + 4 + (track_h - clamp(track_h * panel->rows / total_rows, 16, track_h)) *
                                          panel->top_row / (total_rows - panel->rows);
        return y < thumb_y ? DOCK_DRAWER_HIT_PAGE_UP : DOCK_DRAWER_HIT_PAGE_DOWN;
    }
    if (x < panel->grid_x || y >= panel->footer_y || x >= panel->grid_x + panel->columns * panel->cell_width)
        return DOCK_DRAWER_HIT_NONE;
    col = (x - panel->grid_x) / panel->cell_width;
    row = (y - panel->grid_y) / panel->cell_height;
    i = (panel->top_row + row) * panel->columns + col;
    if (i >= panel->count) return DOCK_DRAWER_HIT_NONE;
    if (index) *index = i;
    return DOCK_DRAWER_HIT_ENTRY;
}

void dock_drawer_scroll(struct DockDrawerPanel *panel, int rows)
{
    int total_rows, top;
    if (!panel || !panel->window) return;
    total_rows = (panel->count + panel->columns - 1) / panel->columns;
    top = clamp(panel->top_row + rows, 0, total_rows > panel->rows ? total_rows - panel->rows : 0);
    if (top != panel->top_row) {
        panel->top_row = top;
        panel->hover = -1;
        if (!panel->timing.reason || panel->timing.visible) timing_begin(panel, DOCK_DRAWER_TIME_SCROLL);
        /* Drawn by the next step: several wheel clicks in one pass of the
         * main loop cost one drawing, not one each. */
        panel->dirty = 1;
        panel->drawn_at = 0;
    }
}

int dock_drawer_entry_path(const struct DockDrawerPanel *panel, int index, char *out, unsigned long capacity)
{
    if (!panel || index < 0 || index >= panel->count) return 0;
    return dock_dir_join(panel->path, panel->entries[index].name, out, capacity);
}
