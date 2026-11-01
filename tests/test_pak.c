#include "pak.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* test_pak PAK: the pack made by test-host.sh from two known files. */
int main(int argc, char **argv)
{
    char pak[64], entry[32];
    size_t size = 0;
    unsigned char *d;
    assert(argc == 2);
    d = dock_pak_read(argv[1], "a.txt", 1000, &size);
    assert(d && size == 6 && !memcmp(d, "hello\n", 6));
    free(d);
    d = dock_pak_read(argv[1], "b.bin", 1000, &size);
    assert(d && size == 256);
    for (int i = 0; i < 256; i++) assert(d[i] == (unsigned char)i);
    free(d);
    assert(!dock_pak_read(argv[1], "b.bin", 100, &size));      /* over the limit */
    assert(!dock_pak_read(argv[1], "missing", 1000, &size));
    assert(!dock_pak_read("/no/such.pak", "a.txt", 1000, &size));
    assert(dock_pak_split("assets/sprites.pak#rain.png", pak, sizeof pak, entry, sizeof entry) &&
           !strcmp(pak, "assets/sprites.pak") && !strcmp(entry, "rain.png"));
    assert(!dock_pak_split("assets/rain.png", pak, sizeof pak, entry, sizeof entry));
    assert(!dock_pak_split("x.pak#", pak, sizeof pak, entry, sizeof entry));
    puts("PASS pak: entries read and unscrambled, size limit, missing entry and pack, path split");
    return 0;
}
