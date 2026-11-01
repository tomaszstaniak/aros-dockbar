#include "icon_cache.h"
#include <stdlib.h>
#include <string.h>

#define BUCKETS 1024

struct DockCacheEntry {
    struct DockCacheEntry *next_in_bucket, *newer, *older;
    struct DockCacheSig sig;
    void *value;
    unsigned long cost, stamp, hash;
    int size;
    char path[1];
};

static unsigned long hash_of(const char *path, int size)
{
    unsigned long h = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)path; *p; p++) h = (h ^ *p) * 16777619UL;
    return (h ^ (unsigned long)size) * 16777619UL;
}

static void unlink_lru(struct DockIconCache *c, struct DockCacheEntry *e)
{
    if (e->newer) e->newer->older = e->older; else c->newest = e->older;
    if (e->older) e->older->newer = e->newer; else c->oldest = e->newer;
    e->newer = e->older = NULL;
}

static void link_newest(struct DockIconCache *c, struct DockCacheEntry *e)
{
    e->older = c->newest;
    e->newer = NULL;
    if (c->newest) c->newest->newer = e; else c->oldest = e;
    c->newest = e;
}

static void drop(struct DockIconCache *c, struct DockCacheEntry *e)
{
    struct DockCacheEntry **link = &c->buckets[e->hash % BUCKETS];
    while (*link != e) link = &(*link)->next_in_bucket;
    *link = e->next_in_bucket;
    unlink_lru(c, e);
    c->bytes -= e->cost;
    c->count--;
    if (c->free_value) c->free_value(e->value);
    free(e);
}

static struct DockCacheEntry *find(struct DockIconCache *c, const char *path, int size, unsigned long hash)
{
    for (struct DockCacheEntry *e = c->buckets[hash % BUCKETS]; e; e = e->next_in_bucket)
        if (e->hash == hash && e->size == size && !strcmp(e->path, path)) return e;
    return NULL;
}

int dock_cache_init(struct DockIconCache *c, unsigned long budget, DockCacheFree free_value)
{
    memset(c, 0, sizeof(*c));
    c->buckets = calloc(BUCKETS, sizeof(*c->buckets));
    c->budget = budget;
    c->free_value = free_value;
    return c->buckets != NULL;
}

void dock_cache_flush(struct DockIconCache *c)
{
    if (!c->buckets) return;
    while (c->oldest) drop(c, c->oldest);
}

void dock_cache_close(struct DockIconCache *c)
{
    dock_cache_flush(c);
    free(c->buckets);
    memset(c, 0, sizeof(*c));
}

void *dock_cache_get(struct DockIconCache *c, const char *path, int size,
                     const struct DockCacheSig *sig, unsigned long stamp)
{
    struct DockCacheEntry *e;
    if (!c->buckets || !path) return NULL;
    e = find(c, path, size, hash_of(path, size));
    if (e && memcmp(&e->sig, sig, sizeof(*sig))) { drop(c, e); c->stale++; e = NULL; }
    if (!e) { c->misses++; return NULL; }
    c->hits++;
    e->stamp = stamp;
    unlink_lru(c, e);
    link_newest(c, e);
    return e->value;
}

int dock_cache_put(struct DockIconCache *c, const char *path, int size,
                   const struct DockCacheSig *sig, void *value, unsigned long cost,
                   unsigned long stamp)
{
    unsigned long hash;
    struct DockCacheEntry *e, *old;
    size_t length;
    if (!c->buckets || !path || cost > c->budget) return 0;
    hash = hash_of(path, size);
    if ((old = find(c, path, size, hash))) drop(c, old);
    /* Make room from the least recently used end, sparing what this step uses. */
    for (e = c->oldest; e && c->bytes + cost > c->budget;) {
        struct DockCacheEntry *newer = e->newer;
        if (e->stamp != stamp) { drop(c, e); c->evicted++; }
        e = newer;
    }
    length = strlen(path);
    if (!(e = malloc(sizeof(*e) + length))) return 0;
    memset(e, 0, sizeof(*e));
    memcpy(e->path, path, length + 1);
    e->sig = *sig;
    e->value = value;
    e->cost = cost;
    e->stamp = stamp;
    e->size = size;
    e->hash = hash;
    e->next_in_bucket = c->buckets[hash % BUCKETS];
    c->buckets[hash % BUCKETS] = e;
    link_newest(c, e);
    c->bytes += cost;
    c->count++;
    return 1;
}
