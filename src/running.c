#include "running.h"
#include <strings.h>

int dock_running_match(const struct DockRunTarget *t, const struct DockRunProcess *seen, int count)
{
    if (!t || !t->usable || !t->file[0]) return 0;
    for (int i = 0; i < count; i++) {
        const struct DockRunProcess *p = &seen[i];
        if (p->has_home && p->volume == t->volume && p->port == t->port && p->key == t->key &&
            p->name[0] && !strcasecmp(p->name, t->file))
            return 1;
    }
    return 0;
}

int dock_running_keys_usable(unsigned long key, int has_parent, unsigned long parent_key)
{
    return key != 0 && (!has_parent || parent_key != key);
}

int dock_running_step(struct DockRunState *s, int found)
{
    int was = s->shown;
    if (found) {
        s->shown = 1;
        s->missed = 0;
    } else if (s->shown && ++s->missed >= 2) {
        s->shown = 0;
        s->missed = 0;
    }
    return s->shown != was;
}
