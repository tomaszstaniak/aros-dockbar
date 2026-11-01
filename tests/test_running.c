/* Running indicators, host side: the match never shows a dot without the
 * same file name and the same home drawer (volume, handler, key), never for
 * a target whose file system's keys cannot tell drawers apart, and the dot
 * keeps through one scan without the program. */
#include "running.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct DockRunProcess proc(const char *name, unsigned long volume, unsigned long key, unsigned long port)
{
    struct DockRunProcess p;
    memset(&p, 0, sizeof p);
    snprintf(p.name, sizeof p.name, "%s", name);
    p.has_home = 1;
    p.volume = volume;
    p.key = key;
    p.port = port;
    return p;
}

int main(void)
{
    struct DockRunTarget t = { "Clock", 1, 0x100, 42, 0x200 };
    struct DockRunProcess seen[4];
    seen[0] = proc("Clock", 0x100, 42, 0x200);
    assert(dock_running_match(&t, seen, 1));
    seen[0] = proc("CLOCK", 0x100, 42, 0x200);  /* AmigaDOS names ignore case */
    assert(dock_running_match(&t, seen, 1));
    /* Same name in another drawer, another volume, another handler. */
    seen[0] = proc("Clock", 0x100, 43, 0x200);
    seen[1] = proc("Clock", 0x101, 42, 0x200);
    seen[2] = proc("Clock", 0x100, 42, 0x201);
    assert(!dock_running_match(&t, seen, 3));
    /* Same drawer, another name; an idle Shell (no name); no home drawer. */
    seen[0] = proc("MultiView", 0x100, 42, 0x200);
    seen[1] = proc("", 0x100, 42, 0x200);
    seen[2] = proc("Clock", 0x100, 42, 0x200);
    seen[2].has_home = 0;
    assert(!dock_running_match(&t, seen, 3));
    /* A target that cannot be compared never matches, even on equal fields. */
    t.usable = 0;
    seen[0] = proc("Clock", 0x100, 42, 0x200);
    assert(!dock_running_match(&t, seen, 1));
    assert(!dock_running_match(NULL, seen, 1));

    /* FAT gives every lock the key 0; a key equal to the parent's says the
     * same; a volume's root has no parent. */
    assert(!dock_running_keys_usable(0, 1, 5));
    assert(!dock_running_keys_usable(0, 0, 0));
    assert(!dock_running_keys_usable(7, 1, 7));
    assert(dock_running_keys_usable(7, 1, 3));
    assert(dock_running_keys_usable(7, 0, 0));

    struct DockRunState s = { 0, 0 };
    assert(dock_running_step(&s, 1) && s.shown);
    assert(!dock_running_step(&s, 1) && s.shown);
    assert(!dock_running_step(&s, 0) && s.shown);   /* one scan without: kept */
    assert(!dock_running_step(&s, 1) && s.shown);   /* back: still on, count reset */
    assert(!dock_running_step(&s, 0) && s.shown);
    assert(dock_running_step(&s, 0) && !s.shown);   /* two in a row: off */
    assert(!dock_running_step(&s, 0) && !s.shown);
    puts("PASS running: name and home drawer both, case, other drawer/volume/handler, unusable keys, keep through one missed scan");
    return 0;
}
