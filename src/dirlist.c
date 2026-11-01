#include "dirlist.h"
#include <string.h>

static int fold(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int dock_dir_listed(const char *name)
{
    size_t length = name ? strlen(name) : 0;
    if (!length || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    if (length >= 5) {
        const char *s = name + length - 5;
        if (s[0] == '.' && fold((unsigned char)s[1]) == 'i' && fold((unsigned char)s[2]) == 'n' &&
            fold((unsigned char)s[3]) == 'f' && fold((unsigned char)s[4]) == 'o')
            return 0;
    }
    return 1;
}

int dock_dir_compare(int a_is_dir, const char *a, int b_is_dir, const char *b)
{
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    if (!a_is_dir != !b_is_dir) return a_is_dir ? -1 : 1;
    for (; *p && fold(*p) == fold(*q); p++, q++) {}
    if (fold(*p) != fold(*q)) return fold(*p) < fold(*q) ? -1 : 1;
    return strcmp(a, b);
}

int dock_dir_parent(const char *path, char *out, unsigned long capacity)
{
    const char *colon = strchr(path, ':'), *slash = strrchr(path, '/');
    size_t length;
    if (!colon || !colon[1]) return 0;
    length = slash && slash > colon ? (size_t)(slash - path) : (size_t)(colon - path + 1);
    if (length + 1 > capacity) return 0;
    memcpy(out, path, length);
    out[length] = 0;
    return 1;
}

int dock_dir_join(const char *path, const char *name, char *out, unsigned long capacity)
{
    size_t a = strlen(path), b = strlen(name);
    int slash = a && path[a - 1] != ':' && path[a - 1] != '/';
    if (a + slash + b + 1 > capacity) return 0;
    memcpy(out, path, a);
    if (slash) out[a++] = '/';
    memcpy(out + a, name, b + 1);
    return 1;
}

int dock_dir_info_base(const char *name, char *out, unsigned long capacity)
{
    size_t length = name ? strlen(name) : 0;
    if (length <= 5 || dock_dir_listed(name) || length - 5 + 1 > capacity) return 0;
    memcpy(out, name, length - 5);
    out[length - 5] = 0;
    return 1;
}

int dock_dir_samename(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    for (; *p && fold(*p) == fold(*q); p++, q++) {}
    return !*p && !*q;
}
