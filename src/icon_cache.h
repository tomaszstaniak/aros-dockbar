#ifndef DOCK_ICON_CACHE_H
#define DOCK_ICON_CACHE_H
/* Pictures already made for the drawer panel, kept between reads and between
 * openings of the panel, within a memory budget. Pure bookkeeping (no AROS
 * calls): values are opaque and freed through the callback.
 *
 * An entry is found by path and icon size and is used only while its source
 * signature is the same: the caller builds the signature from what it read
 * with the listing (the .info file's date and size, or the object's own when
 * it has no .info), so a changed icon is made again without reading anything
 * extra. Screen changes are the caller's to report (dock_cache_flush).
 *
 * Least recently used entries go first when the budget is exceeded, except
 * those used at the current stamp: the caller passes the same stamp for
 * every call of one step, so what is on show is never taken away. A value
 * returned by dock_cache_get stays valid until a dock_cache_put with a newer
 * stamp, or a flush. */
struct DockCacheSig { unsigned long a, b, c, d; };
typedef void (*DockCacheFree)(void *value);
struct DockCacheEntry;
struct DockIconCache {
    struct DockCacheEntry **buckets;
    struct DockCacheEntry *newest, *oldest;
    unsigned long budget, bytes, count;
    unsigned long hits, misses, stale, evicted;
    DockCacheFree free_value;
};

/* 1 ready, 0 out of memory. Zero-initialize the struct first. */
int dock_cache_init(struct DockIconCache *cache, unsigned long budget, DockCacheFree free_value);
void dock_cache_close(struct DockIconCache *cache);
/* The value, or NULL when absent or made from another source (that entry is
 * then dropped and counted as stale). */
void *dock_cache_get(struct DockIconCache *cache, const char *path, int size,
                     const struct DockCacheSig *sig, unsigned long stamp);
/* Takes the value; 1 kept, 0 refused (no memory, or it alone exceeds the
 * budget): the caller still owns it then. Older entries may be freed. */
int dock_cache_put(struct DockIconCache *cache, const char *path, int size,
                   const struct DockCacheSig *sig, void *value, unsigned long cost,
                   unsigned long stamp);
/* Everything goes, e.g. when the screen and its pens change. */
void dock_cache_flush(struct DockIconCache *cache);
#endif
