/* Development probe, not shipped: fills a drawer for panel timing tests.
 *   mkfiles DIR COUNT [ICON EVERY]
 * creates DIR (if needed) with COUNT small files f0000.txt..., and gives every
 * EVERY-th file its own icon, a copy of ICON (a .info file), so that a test
 * drawer has both own and identified (default) icons. Also MOD DIR N touches
 * the .info of file N (rewrites it with the same bytes), so that its icon
 * source changes without a new name. TOUCH PATH... creates small files with
 * exactly these names (quotes, asterisks, semicolons: argv is parsed with
 * the Shell's escapes, so it can hold names a redirection cannot).
 * CHURN DIR SECONDS MS keeps changing DIR: every MS milliseconds it creates
 * c0000.tmp, c0001.tmp... and deletes the one made 10 steps before, so the
 * drawer never rests; at the end the last 10 stay. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *icon_data;
static long icon_size;

static int write_file(const char *path, const void *data, long size)
{
    BPTR f = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!f) return 0;
    long n = Write(f, (APTR)data, size);
    Close(f);
    return n == size;
}

static int load_icon(const char *path)
{
    BPTR f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    if (!f) return 0;
    Seek(f, 0, OFFSET_END);
    icon_size = Seek(f, 0, OFFSET_BEGINNING);
    icon_data = malloc((size_t)icon_size);
    int ok = icon_data && Read(f, icon_data, icon_size) == icon_size;
    Close(f);
    return ok;
}

int main(int argc, char **argv)
{
    char path[512];
    if (argc == 4 && !strcmp(argv[1], "MOD")) {
        /* Rewrite an existing .info with its own bytes: new date, same content. */
        snprintf(path, sizeof path, "%s/f%04d.txt.info", argv[2], atoi(argv[3]));
        if (!load_icon(path) || !write_file(path, icon_data, icon_size)) { printf("MOD failed %s\n", path); return 10; }
        printf("MOD %s\n", path);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "CHURN")) {
        int seconds = atoi(argv[3]), ms = atoi(argv[4]), steps = 0;
        if (ms < 20) ms = 20;
        for (int i = 0; i * ms < seconds * 1000; i++, steps++) {
            snprintf(path, sizeof path, "%s/c%04d.tmp", argv[2], i);
            write_file(path, "c\n", 2);
            if (i >= 10) { snprintf(path, sizeof path, "%s/c%04d.tmp", argv[2], i - 10); DeleteFile((CONST_STRPTR)path); }
            Delay(ms / 20);
        }
        printf("churn %d steps in %s\n", steps, argv[2]);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "TOUCH")) {
        for (int i = 2; i < argc; i++)
            printf("%s [%s]\n", write_file(argv[i], "x\n", 2) ? "made" : "FAILED", argv[i]);
        return 0;
    }
    if (argc < 3) { printf("usage: mkfiles DIR COUNT [ICON EVERY] | MOD DIR N | TOUCH PATH...\n"); return 10; }
    int count = atoi(argv[2]), every = argc >= 5 ? atoi(argv[4]) : 0, icons = 0;
    if (every > 0 && !load_icon(argv[3])) { printf("cannot read %s\n", argv[3]); return 10; }
    BPTR lock = CreateDir((CONST_STRPTR)argv[1]);
    if (lock) UnLock(lock);
    for (int i = 0; i < count; i++) {
        char text[32];
        int n = snprintf(text, sizeof text, "file %d\n", i);
        snprintf(path, sizeof path, "%s/f%04d.txt", argv[1], i);
        if (!write_file(path, text, n)) { printf("failed at %s\n", path); return 10; }
        if (every > 0 && i % every == 0) {
            strcat(path, ".info");
            if (!write_file(path, icon_data, icon_size)) { printf("failed at %s\n", path); return 10; }
            icons++;
        }
    }
    printf("made %d files, %d icons in %s\n", count, icons, argv[1]);
    return 0;
}
