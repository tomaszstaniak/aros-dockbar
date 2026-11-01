/* Development probe, not shipped: lists a tree with what a restore must
 * keep, so two systems can be compared entry for entry.
 *   filesum DIR OUTFILE [FLAT]     (FLAT: this level only)
 * One line per entry: path|type|size|protection(hex)|days.minute.tick|comment|crc32
 * (directories: size and crc 0). Walks with ExNext and locks relative to
 * the parent, so path length does not matter. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned long table[256];
static FILE *out;
static unsigned char buffer[65536];
static int flat;

static unsigned long crc_file(BPTR dir, const char *name)
{
    unsigned long crc = 0xffffffffUL;
    BPTR old = CurrentDir(dir), f = Open((CONST_STRPTR)name, MODE_OLDFILE);
    CurrentDir(old);
    if (!f) return 0xdeadbeefUL;
    LONG n;
    while ((n = Read(f, buffer, sizeof buffer)) > 0)
        for (LONG i = 0; i < n; i++) crc = table[(crc ^ buffer[i]) & 255] ^ (crc >> 8);
    Close(f);
    return crc ^ 0xffffffffUL;
}

static void walk(BPTR dir, const char *prefix)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib || !Examine(dir, fib)) { if (fib) FreeDosObject(DOS_FIB, fib); return; }
    while (ExNext(dir, fib)) {
        char path[1024];
        int is_dir = fib->fib_DirEntryType > 0 && fib->fib_DirEntryType != ST_SOFTLINK;
        snprintf(path, sizeof path, "%s%s", prefix, (char *)fib->fib_FileName);
        fprintf(out, "%s|%s|%ld|%lx|%ld.%ld.%ld|%s|%08lx\n", path, is_dir ? "D" : "F",
                is_dir ? 0L : (long)fib->fib_Size, (unsigned long)fib->fib_Protection,
                (long)fib->fib_Date.ds_Days, (long)fib->fib_Date.ds_Minute, (long)fib->fib_Date.ds_Tick,
                (char *)fib->fib_Comment, is_dir ? 0UL : crc_file(dir, (char *)fib->fib_FileName));
        if (is_dir && !flat) {
            BPTR old = CurrentDir(dir), sub = Lock(fib->fib_FileName, SHARED_LOCK);
            CurrentDir(old);
            if (sub) {
                char next[1100];
                snprintf(next, sizeof next, "%s/", path);
                walk(sub, next);
                UnLock(sub);
            }
        }
    }
    FreeDosObject(DOS_FIB, fib);
}

int main(int argc, char **argv)
{
    if (argc < 3) return 20;
    for (unsigned long i = 0; i < 256; i++) {
        unsigned long c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    flat = argc > 3;
    BPTR dir = Lock((CONST_STRPTR)argv[1], SHARED_LOCK);
    if (!dir || !(out = fopen(argv[2], "w"))) return 20;
    walk(dir, "");
    fclose(out);
    UnLock(dir);
    return 0;
}
