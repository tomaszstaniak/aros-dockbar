#include "wallprefs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Builds a prefs file the way Wanderer's prefs editor writes one
 * (workbench/prefs/wanderer/wpeditor.c, ExportFH): per view a WANR header
 * chunk (name, padding, little-endian data size at its end), then a WANR
 * data chunk (spec, padding to 4, little-endian tag pairs). */
static unsigned char file[4096];
static size_t used;

static void be(unsigned long v) { file[used++] = v >> 24; file[used++] = v >> 16; file[used++] = v >> 8; file[used++] = v; }
static void chunk(const char *id, const unsigned char *data, size_t n)
{
    memcpy(file + used, id, 4); used += 4; be(n);
    memcpy(file + used, data, n); used += n;
    if (n & 1) file[used++] = 0;
}
static void view(const char *name, const char *spec, int mode, int tile, int x, int y)
{
    unsigned char h[104], dta[512];
    size_t s = strlen(spec) + 1, off = (s + 3) / 4 * 4, n = off;
    memset(h, 0, sizeof h);
    memset(dta, 0, sizeof dta);
    memcpy(dta, spec, s);
    unsigned long tags[][2] = {{0x9f100001u, mode}, {0x9f100002u, tile}, {0x9f100003u, x}, {0x9f100004u, y},
                               {0x90462100u, 1}, {0x90462101u, 0}};  /* icon list tags follow */
    for (size_t i = 0; i < 6; i++) {
        for (int k = 0; k < 4; k++) dta[n + k] = tags[i][0] >> (8 * k);
        for (int k = 0; k < 4; k++) dta[n + 4 + k] = tags[i][1] >> (8 * k);
        n += 8;
    }
    snprintf((char *)h, 100, "%s", name);
    for (int k = 0; k < 4; k++) h[100 + k] = n >> (8 * k);
    chunk("WANR", h, sizeof h);
    chunk("WANR", dta, n);
}
static void build(const char *wb_spec, int mode, int tile, int x, int y)
{
    unsigned char global[104] = "wanderer:global";
    used = 0;
    memcpy(file, "FORM", 4); used = 8;
    memcpy(file + used, "PREF", 4); used += 4;
    unsigned char prhd[6] = {0};
    chunk("PRHD", prhd, 6);
    chunk("WANR", global, sizeof global);
    view("wanderer:viewsettings.Drawer", "5:SYS:Prefs/Presets/Wallpapers/d.jpg", 1, 1, 0, 0);
    view("wanderer:viewsettings.Workbench", wb_spec, mode, tile, x, y);
    unsigned long form = used - 8;
    file[4] = form >> 24; file[5] = form >> 16; file[6] = form >> 8; file[7] = form;
}

int main(void)
{
    struct DockWallPrefs w;
    build("5:SYS:Prefs/Presets/Wallpapers/2007-comp-entries/02-d980_aros-rose.jpg", 2, 1, 0, 0);
    assert(dock_wallprefs_parse(file, used, &w));
    assert(w.kind == DOCK_WALL_PICTURE && !strcmp(w.path, "SYS:Prefs/Presets/Wallpapers/2007-comp-entries/02-d980_aros-rose.jpg"));
    assert(w.mode == DOCK_WALL_SCALED && w.tile == 1 && !w.x && !w.y);  /* the Workbench view, not the Drawer one */
    build("5:SYS:t/tile.png", 1, 2, 16, 24);
    assert(dock_wallprefs_parse(file, used, &w) && w.mode == DOCK_WALL_TILED && w.tile == 2 && w.x == 16 && w.y == 24);
    build("2:9c9c9c9c,9c9c9c9c,9c9c9c9c", 2, 1, 0, 0);
    assert(dock_wallprefs_parse(file, used, &w) && w.kind == DOCK_WALL_COLOUR && w.rgb == 0x9c9c9c);

    /* Kinds the reader does not draw: MUI patterns, gradients, brushes. */
    const char *other[] = {"0:128", "7:v,ffffffff-00000000", "6:12", "1:4", "5:relative.png", "2:12345678,12", "5:\001bad:x"};
    for (size_t i = 0; i < sizeof other / sizeof *other; i++) {
        build(other[i], 2, 1, 0, 0);
        assert(!dock_wallprefs_parse(file, used, &w));
    }
    /* Values outside what Wanderer writes. */
    build("5:SYS:a.png", 3, 1, 0, 0);
    assert(!dock_wallprefs_parse(file, used, &w));
    build("5:SYS:a.png", 1, 1, 99999, 0);
    assert(!dock_wallprefs_parse(file, used, &w));

    /* Damage: every truncation, a wrong FORM size, a wrong announced size. */
    build("5:SYS:Prefs/a.jpg", 2, 1, 0, 0);
    size_t full = used;
    unsigned char good[4096];
    memcpy(good, file, full);
    for (size_t n = 0; n < full; n++) assert(!dock_wallprefs_parse(good, n, &w));
    good[7] ^= 4;
    assert(!dock_wallprefs_parse(good, full, &w));
    good[7] ^= 4;
    assert(dock_wallprefs_parse(good, full, &w));

    /* Random corruption must never crash or read out of bounds (run under
     * AddressSanitizer by scripts/test-host.sh); results may go either way
     * but anything accepted is within range. */
    srand(7);
    int accepted = 0;
    for (int round = 0; round < 20000; round++) {
        unsigned char bad[4096];
        memcpy(bad, good, full);
        int flips = 1 + rand() % 4;
        for (int k = 0; k < flips; k++) bad[rand() % full] ^= (unsigned char)(1 + rand() % 255);
        size_t n = full - (size_t)(rand() % 3 == 0 ? rand() % 16 : 0);
        if (dock_wallprefs_parse(bad, n, &w)) {
            accepted++;
            assert((w.mode == 1 || w.mode == 2) && (w.tile == 1 || w.tile == 2) && w.x <= 4096 && w.y <= 4096);
            assert(w.kind == DOCK_WALL_COLOUR || (w.kind == DOCK_WALL_PICTURE && strchr(w.path, ':')));
        }
    }
    printf("PASS wallprefs: picture/colour views, other kinds and ranges refused, truncation, 20000 corruptions (%d still valid)\n", accepted);
    return 0;
}
