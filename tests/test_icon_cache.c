#include "icon_cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int freed;
static void count_free(void *value) { freed++; free(value); }
static void *value(void) { return malloc(1); }

int main(void)
{
    struct DockIconCache c;
    struct DockCacheSig s1 = { 1, 2, 3, 4 }, s2 = { 1, 2, 3, 5 };
    char path[32];
    assert(dock_cache_init(&c, 1000, count_free));
    /* Hit by path and size with the same source; another size is another entry. */
    assert(dock_cache_put(&c, "RAM:a", 48, &s1, value(), 100, 1));
    assert(dock_cache_get(&c, "RAM:a", 48, &s1, 2) && !dock_cache_get(&c, "RAM:a", 32, &s1, 2));
    assert(c.hits == 1 && c.misses == 1);
    /* A changed source (date or size of the .info) drops the old picture. */
    assert(!dock_cache_get(&c, "RAM:a", 48, &s2, 3) && c.stale == 1 && freed == 1 && c.count == 0 && c.bytes == 0);
    /* Budget: the least recently used goes first. */
    for (int i = 0; i < 10; i++) { snprintf(path, sizeof path, "RAM:f%d", i); assert(dock_cache_put(&c, path, 48, &s1, value(), 100, 10)); }
    assert(c.count == 10 && c.bytes == 1000);
    assert(dock_cache_get(&c, "RAM:f0", 48, &s1, 11));  /* f0 now newest */
    assert(dock_cache_put(&c, "RAM:new", 48, &s1, value(), 100, 11));
    assert(c.count == 10 && c.evicted == 1 && !dock_cache_get(&c, "RAM:f1", 48, &s1, 12) && dock_cache_get(&c, "RAM:f0", 48, &s1, 12));
    /* What the current step uses is never evicted, even over budget. */
    for (int i = 2; i < 10; i++) { snprintf(path, sizeof path, "RAM:f%d", i); assert(dock_cache_get(&c, path, 48, &s1, 20)); }
    assert(dock_cache_get(&c, "RAM:f0", 48, &s1, 20) && dock_cache_get(&c, "RAM:new", 48, &s1, 20));
    assert(dock_cache_put(&c, "RAM:over", 48, &s1, value(), 100, 20));
    assert(c.count == 11 && c.bytes == 1100);
    for (int i = 2; i < 10; i++) { snprintf(path, sizeof path, "RAM:f%d", i); assert(dock_cache_get(&c, path, 48, &s1, 20)); }
    /* Next step: the excess goes at the next insertion. */
    assert(dock_cache_put(&c, "RAM:later", 48, &s1, value(), 100, 21) && c.bytes <= 1000);
    /* Replacing a path frees the old value; too costly a value is refused. */
    unsigned long before = (unsigned long)freed;
    assert(dock_cache_put(&c, "RAM:later", 48, &s2, value(), 100, 22) && (unsigned long)freed == before + 1);
    void *big = value();
    assert(!dock_cache_put(&c, "RAM:big", 48, &s1, big, 2000, 22));
    free(big);
    /* Flush (screen change) and close free everything. */
    dock_cache_flush(&c);
    assert(c.count == 0 && c.bytes == 0 && !dock_cache_get(&c, "RAM:later", 48, &s2, 23));
    assert(dock_cache_put(&c, "RAM:x", 48, &s1, value(), 1, 24));
    dock_cache_close(&c);
    puts("PASS icon_cache: path+size+source key, stale source dropped, LRU within budget, current step spared, flush");
    return 0;
}
