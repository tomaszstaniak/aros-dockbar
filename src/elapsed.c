#include "elapsed.h"

uint64_t dock_elapsed_feed(struct DockElapsed *e, uint64_t raw, uint64_t max_step)
{
    if (!e->started) {
        e->started = 1;
        e->last = raw;
        return e->total;
    }
    if (raw > e->last && (!max_step || raw - e->last <= max_step))
        e->total += raw - e->last;
    e->last = raw;
    return e->total;
}
