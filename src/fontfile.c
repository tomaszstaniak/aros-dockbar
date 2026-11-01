#include "fontfile.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static unsigned long be32(const unsigned char *p) { return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3]; }

static int ends_with(const char *s, size_t n, const char *suffix)
{
    size_t k = strlen(suffix);
    if (n < k) return 0;
    for (size_t i = 0; i < k; i++)
        if (tolower((unsigned char)s[n - k + i]) != suffix[i]) return 0;
    return 1;
}

int dock_otag_font_file(const unsigned char *d, size_t size, char *out, size_t capacity)
{
    if (!d || !out || capacity < 2 || size < 8 || size > 65536) return 0;
    for (size_t p = 0; p + 8 <= size && p < 256 * 8; p += 8) {
        unsigned long tag = be32(d + p), value = be32(d + p + 4);
        if (!tag) return 0;
        /* Indirect tags (bit 15) carry an offset to their data in the file. */
        if (!(tag & 0x80000000u) || !(tag & 0x8000u) || value >= size) continue;
        const char *s = (const char *)d + value;
        size_t n = 0, room = size - value;
        while (n < room && s[n]) n++;
        if (n == room || n < 6 || n >= capacity) continue;
        int printable = 1;
        for (size_t i = 0; i < n; i++)
            if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126) printable = 0;
        if (!printable || !memchr(s, ':', n)) continue;
        if (ends_with(s, n, ".ttf") || ends_with(s, n, ".otf") || ends_with(s, n, ".ttc")) {
            memcpy(out, s, n);
            out[n] = 0;
            return 1;
        }
    }
    return 0;
}

int dock_otag_path(const char *name, char *out, size_t capacity)
{
    size_t n = name ? strlen(name) : 0;
    if (n <= 5 || strcmp(name + n - 5, ".font") || strchr(name, ':') || strchr(name, '/')) return 0;
    int written = snprintf(out, capacity, "FONTS:%.*s.otag", (int)(n - 5), name);
    return written > 0 && (size_t)written < capacity;
}
