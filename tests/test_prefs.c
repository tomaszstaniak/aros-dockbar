#include "prefs.h"
#include "frame.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}

int main(int argc, char **argv)
{
    struct DockPrefs p, q;
    const char *path = argc > 1 ? argv[1] : "test.prefs";
    remove(path);
    dock_prefs_default(&p);
    assert(p.icon_size == 40 && p.corner == DOCK_CORNER_DEFAULT && !p.locked);
    q = p;
    assert(dock_prefs_load(&q, path) == 0 && !memcmp(&p, &q, sizeof p));
    p.icon_size = 56; p.corner = 0; p.locked = 1;
    assert(dock_prefs_save(&p, path) == 1);
    dock_prefs_default(&q);
    assert(dock_prefs_load(&q, path) == 1 && !memcmp(&p, &q, sizeof p));
    p.icon_size = 32; p.corner = 8; p.locked = 0;
    assert(dock_prefs_save(&p, path) == 1);
    assert(dock_prefs_load(&q, path) == 1 && !memcmp(&p, &q, sizeof p));
    /* Out-of-range and malformed files leave the caller's values unchanged. */
    struct DockPrefs bad = p; bad.icon_size = 80;
    assert(!dock_prefs_save(&bad, path));
    assert(dock_prefs_load(&q, path) == 1 && q.icon_size == 32);
    const char *broken[] = {
        "DOCKPREFS\t2\nICONSIZE\t40\nCORNER\t5\nLOCKED\t0\nEND\n",
        "DOCKPREFS\t1\nICONSIZE\t40\nCORNER\t9\nLOCKED\t0\nEND\n",
        "DOCKPREFS\t1\nICONSIZE\t40\nCORNER\t5\nLOCKED\t0\n",
        "DOCKPREFS\t1\nICONSIZE\tx\nCORNER\t5\nLOCKED\t0\nEND\n",
        "DOCKPREFS\t1\nCORNER\t5\nICONSIZE\t40\nLOCKED\t0\nEND\n",
        "DOCKPREFS\t1\nICONSIZE\t40\nCORNER\t5\nLOCKED\t0\nEND\nextra",
    };
    for (size_t i = 0; i < sizeof broken / sizeof *broken; i++) {
        put(path, broken[i]);
        q = p;
        assert(dock_prefs_load(&q, path) == -1 && !memcmp(&p, &q, sizeof p));
    }
    remove(path);
    puts("PASS prefs: defaults, round trip, range checks, malformed files keep values");
    return 0;
}
