#include "fontfile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char path[700], out[256];
    unsigned char data[8192];
    assert(dock_otag_path("Vera Sans.font", out, sizeof out) && !strcmp(out, "FONTS:Vera Sans.otag"));
    assert(!dock_otag_path("topaz", out, sizeof out) && !dock_otag_path("SYS:x.font", out, sizeof out) &&
           !dock_otag_path(".font", out, sizeof out) && !dock_otag_path("a.font", out, 8));
    /* The system's own .otag files (ABIv11 2026.09). */
    const char *names[] = { "Vera Sans", "Open Sans Regular" }, *files[] = { "Fonts:TrueType/VeraSans.ttf", "Fonts:TrueType/OpenSans-Regular.ttf" };
    size_t full = 0;
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof path, "%s/%s.otag", argc > 1 ? argv[1] : ".", names[i]);
        FILE *f = fopen(path, "rb");
        if (!f) { puts("SKIP fontfile: .otag files not found"); return 0; }
        full = fread(data, 1, sizeof data, f);
        fclose(f);
        assert(dock_otag_font_file(data, full, out, sizeof out) && !strcmp(out, files[i]));
    }
    /* Damage: truncation anywhere, random flips; never a crash, and anything
     * found is a printable path with a device and a font suffix. */
    for (size_t n = 0; n < full; n++) dock_otag_font_file(data, n, out, sizeof out);
    assert(!dock_otag_font_file(data, full, out, 8));  /* does not fit */
    srand(11);
    int found = 0;
    for (int round = 0; round < 20000; round++) {
        unsigned char bad[8192];
        memcpy(bad, data, full);
        for (int k = 1 + rand() % 4; k > 0; k--) bad[rand() % full] ^= (unsigned char)(1 + rand() % 255);
        if (dock_otag_font_file(bad, full, out, sizeof out)) {
            found++;
            assert(strchr(out, ':') && strlen(out) >= 6);
        }
    }
    printf("PASS fontfile: .otag path, system .otag files, truncation, 20000 corruptions (%d still found)\n", found);
    return 0;
}
