/* Development probe, not shipped: gives files the protection bits, date and
 * comment recorded by filesum, after a copy that could not keep them (via a
 * FAT disk, for example).
 *   fixattr LIST ROOT     each LIST line is relative to ROOT
 * Prints one line per entry it could not change, and a summary. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char line[2048], path[2200];
    int done = 0, failed = 0;
    FILE *in;
    if (argc < 3 || !(in = fopen(argv[1], "r"))) return 20;
    while (fgets(line, sizeof line, in)) {
        char *field[7];
        int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *p = line; n < 7; ) {
            field[n++] = p;
            char *bar = strchr(p, '|');
            if (!bar) break;
            *bar = 0;
            p = bar + 1;
        }
        if (n < 7) continue;
        snprintf(path, sizeof path, "%s%s%s", argv[2],
                 argv[2][strlen(argv[2]) - 1] == ':' ? "" : "/", field[0]);
        struct DateStamp ds;
        ds.ds_Days = strtol(field[4], NULL, 10);
        char *dot = strchr(field[4], '.');
        ds.ds_Minute = dot ? strtol(dot + 1, NULL, 10) : 0;
        dot = dot ? strchr(dot + 1, '.') : NULL;
        ds.ds_Tick = dot ? strtol(dot + 1, NULL, 10) : 0;
        int ok = SetProtection((CONST_STRPTR)path, (ULONG)strtoul(field[3], NULL, 16)) &&
                 SetComment((CONST_STRPTR)path, (CONST_STRPTR)field[5]) &&
                 SetFileDate((CONST_STRPTR)path, &ds);
        if (ok) done++;
        else { failed++; printf("FAILED %s (%ld)\n", path, (long)IoErr()); }
    }
    fclose(in);
    printf("fixattr: %d set, %d failed\n", done, failed);
    return failed ? 10 : 0;
}
