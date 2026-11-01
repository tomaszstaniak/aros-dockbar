#include "pak.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long u32(const unsigned char *p)
{
    return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

int dock_pak_split(const char *path, char *pak, size_t pak_cap, char *entry, size_t entry_cap)
{
    const char *hash = path ? strrchr(path, '#') : NULL;
    size_t n;
    if (!hash || !hash[1]) return 0;
    n = (size_t)(hash - path);
    if (n + 1 > pak_cap || strlen(hash + 1) + 1 > entry_cap) return 0;
    memcpy(pak, path, n);
    pak[n] = 0;
    strcpy(entry, hash + 1);
    return 1;
}

unsigned char *dock_pak_read(const char *pak, const char *name, size_t max, size_t *size)
{
    unsigned char head[12], field[4], *data = NULL;
    unsigned long count, offset = 0, length = 0;
    size_t want = strlen(name);
    int found = 0;
    FILE *f = fopen(pak, "rb");
    if (!f) return NULL;
    if (fread(head, 1, 12, f) != 12 || memcmp(head, "DKPAK1\0\0", 8)) goto done;
    count = u32(head + 8);
    if (count > 4096) goto done;
    for (unsigned long i = 0; i < count && !found; i++) {
        char entry[256];
        unsigned char pos[8];
        if (fread(field, 1, 4, f) != 4) goto done;
        unsigned long len = u32(field);
        if (len >= sizeof entry || fread(entry, 1, len, f) != len || fread(pos, 1, 8, f) != 8) goto done;
        entry[len] = 0;
        if (len == want && !memcmp(entry, name, len)) { offset = u32(pos); length = u32(pos + 4); found = 1; }
    }
    if (!found || length > max || fseek(f, (long)offset, SEEK_SET)) goto done;
    if (!(data = malloc(length ? length : 1)) || fread(data, 1, length, f) != length) { free(data); data = NULL; goto done; }
    {
        unsigned long x = (0x9E3779B9UL ^ offset) & 0xffffffffUL;
        if (!x) x = 1;
        for (unsigned long i = 0; i < length; i++) {
            x ^= (x << 13) & 0xffffffffUL;
            x ^= x >> 17;
            x ^= (x << 5) & 0xffffffffUL;
            data[i] ^= (unsigned char)(x & 0xff);
        }
    }
    *size = length;
done:
    fclose(f);
    return data;
}
