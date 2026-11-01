#include "prefs.h"
#include "frame.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dock_prefs_default(struct DockPrefs *prefs)
{
    prefs->icon_size = DOCK_ICON_DEFAULT;
    prefs->corner = DOCK_CORNER_DEFAULT;
    prefs->locked = 0;
}

int dock_prefs_valid(const struct DockPrefs *prefs)
{
    return prefs && prefs->icon_size >= DOCK_ICON_MIN && prefs->icon_size <= DOCK_ICON_MAX &&
           prefs->corner >= 0 && prefs->corner <= DOCK_CORNER_MAX &&
           (prefs->locked == 0 || prefs->locked == 1);
}

static int field(FILE *file, const char *key, int *value)
{
    char line[64], expect[32], *end;
    size_t length;
    if (!fgets(line, sizeof line, file)) return 0;
    length = strlen(line);
    if (!length || line[length - 1] != '\n') return 0;
    line[length - 1] = 0;
    snprintf(expect, sizeof expect, "%s\t", key);
    if (strncmp(line, expect, strlen(expect))) return 0;
    errno = 0;
    long number = strtol(line + strlen(expect), &end, 10);
    if (errno || *end || end == line + strlen(expect) || number < -1000 || number > 1000) return 0;
    *value = (int)number;
    return 1;
}

int dock_prefs_load(struct DockPrefs *prefs, const char *path)
{
    struct DockPrefs read;
    char line[16];
    int version = 0, valid;
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? 0 : -1;
    valid = field(file, "DOCKPREFS", &version) && version == 1 &&
            field(file, "ICONSIZE", &read.icon_size) &&
            field(file, "CORNER", &read.corner) &&
            field(file, "LOCKED", &read.locked) &&
            fgets(line, sizeof line, file) && !strcmp(line, "END\n") &&
            fgetc(file) == EOF && dock_prefs_valid(&read);
    if (fclose(file)) valid = 0;
    if (!valid) return -1;
    *prefs = read;
    return 1;
}

int dock_prefs_save(const struct DockPrefs *prefs, const char *path)
{
    char temporary[512];
    FILE *file;
    int ok;
    if (!dock_prefs_valid(prefs) || snprintf(temporary, sizeof temporary, "%s.new", path) >= (int)sizeof temporary)
        return 0;
    file = fopen(temporary, "wb");
    if (!file) return 0;
    ok = fprintf(file, "DOCKPREFS\t1\nICONSIZE\t%d\nCORNER\t%d\nLOCKED\t%d\nEND\n",
                 prefs->icon_size, prefs->corner, prefs->locked) > 0;
    if (fclose(file)) ok = 0;
    /* AmigaDOS Rename cannot replace a file. Losing preferences between the two
     * steps only restores defaults; the composition lives in dock.config. */
    if (ok && remove(path) && errno != ENOENT) ok = 0;
    if (ok && rename(temporary, path)) ok = 0;
    if (!ok) remove(temporary);
    return ok;
}
