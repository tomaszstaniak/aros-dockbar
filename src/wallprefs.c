#include "wallprefs.h"
#include <string.h>

#define TAG_RENDERMODE 0x9f100001u
#define TAG_TILEMODE   0x9f100002u
#define TAG_XOFFSET    0x9f100003u
#define TAG_YOFFSET    0x9f100004u

static unsigned long be32(const unsigned char *p) { return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3]; }
static unsigned long le32(const unsigned char *p) { return (unsigned long)p[3] << 24 | (unsigned long)p[2] << 16 | (unsigned long)p[1] << 8 | p[0]; }

static int hexword(const char *s, unsigned long *v)
{
    *v = 0;
    for (int i = 0; i < 8; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0;
        *v = *v << 4 | (unsigned long)d;
    }
    return 1;
}

/* MUI image spec of the background: "5:<file>" picture, "2:rrrrrrrr,gggggggg,bbbbbbbb". */
static int spec(const char *s, size_t n, struct DockWallPrefs *out)
{
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126) return 0;
    if (n > 2 && s[0] == '5' && s[1] == ':') {
        if (n - 2 >= sizeof out->path || !strchr(s + 2, ':')) return 0;  /* an absolute AmigaDOS path */
        out->kind = DOCK_WALL_PICTURE;
        memcpy(out->path, s + 2, n - 2);
        out->path[n - 2] = 0;
        return 1;
    }
    if (n == 2 + 26 && s[0] == '2' && s[1] == ':' && s[10] == ',' && s[19] == ',') {
        unsigned long r, g, b;
        if (!hexword(s + 2, &r) || !hexword(s + 11, &g) || !hexword(s + 20, &b)) return 0;
        out->kind = DOCK_WALL_COLOUR;
        out->rgb = (r >> 24) << 16 | (g >> 24) << 8 | (b >> 24);
        return 1;
    }
    return 0;
}

int dock_wallprefs_parse(const unsigned char *d, size_t size, struct DockWallPrefs *out)
{
    static const char view[] = "wanderer:viewsettings.Workbench";
    size_t p = 12;
    int header = 0;
    if (!d || !out || size < 12 || size > 65536 || memcmp(d, "FORM", 4) || memcmp(d + 8, "PREF", 4)) return 0;
    if (be32(d + 4) != size - 8 && be32(d + 4) + 8 != size - 1) return 0;  /* one pad byte allowed */
    memset(out, 0, sizeof *out);
    while (p + 8 <= size) {
        unsigned long len = be32(d + p + 4);
        const unsigned char *c = d + p + 8;
        if (len > size - p - 8) return 0;
        if (!memcmp(d + p, "WANR", 4)) {
            if (header) {
                /* The data chunk after the view's header: its length must be
                 * the one the header announced. */
                if (len != (unsigned long)header) return 0;
                const char *text = (const char *)c;
                size_t n = 0;
                while (n < len && text[n]) n++;
                if (n == len || !spec(text, n, out)) return 0;
                size_t tags = (n + 1 + 3) / 4 * 4;
                if (tags > len || (len - tags) % 8) return 0;
                out->mode = DOCK_WALL_TILED;
                out->tile = 1;
                for (size_t k = tags; k + 8 <= len; k += 8) {
                    unsigned long t = le32(c + k), v = le32(c + k + 4);
                    if (t == TAG_RENDERMODE) { if (v != 1 && v != 2) return 0; out->mode = (int)v; }
                    else if (t == TAG_TILEMODE) { if (v != 1 && v != 2) return 0; out->tile = (int)v; }
                    else if (t == TAG_XOFFSET) { if (v > 4096) return 0; out->x = (int)v; }
                    else if (t == TAG_YOFFSET) { if (v > 4096) return 0; out->y = (int)v; }
                }
                return 1;
            }
            if (len >= sizeof view + 4 && !memcmp(c, view, sizeof view)) {
                unsigned long announced = le32(c + len - 4);
                if (!announced || announced > 4096) return 0;
                header = (int)announced;
            }
        } else if (header) return 0;  /* something between header and data */
        p += 8 + len + (len & 1);
    }
    return 0;
}
