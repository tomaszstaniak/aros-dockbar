/* Development probe, not shipped: does ExAll return every entry of a
 * drawer? Counts DIR with ExAll at two buffer sizes and with ExNext.
 *   exall_probe DIR LOGFILE */
#include <dos/dos.h>
#include <dos/exall.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>

static long count_exall(const char *dir, int bytes, int *calls)
{
    BPTR lock = Lock((CONST_STRPTR)dir, SHARED_LOCK);
    struct ExAllControl *eac = AllocDosObject(DOS_EXALLCONTROL, NULL);
    void *buffer = malloc((size_t)bytes);
    long n = 0;
    LONG more;
    *calls = 0;
    if (!lock || !eac || !buffer) return -1;
    eac->eac_LastKey = 0;
    do {
        more = ExAll(lock, buffer, bytes, ED_TYPE, eac);
        ++*calls;
        if (!more && IoErr() != ERROR_NO_MORE_ENTRIES) { n = -2; break; }
        n += eac->eac_Entries;
    } while (more);
    FreeDosObject(DOS_EXALLCONTROL, eac);
    free(buffer);
    UnLock(lock);
    return n;
}

static long count_exnext(const char *dir)
{
    BPTR lock = Lock((CONST_STRPTR)dir, SHARED_LOCK);
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    long n = 0;
    if (!lock || !fib || !Examine(lock, fib)) return -1;
    while (ExNext(lock, fib)) ++n;
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return n;
}

int main(int argc, char **argv)
{
    FILE *out;
    int c1, c2;
    if (argc < 3 || !(out = fopen(argv[2], "a"))) return 20;
    long a = count_exall(argv[1], 4096, &c1), b = count_exall(argv[1], 65536, &c2);
    fprintf(out, "%s exall4k=%ld (%d calls) exall64k=%ld (%d calls) exnext=%ld\n", argv[1], a, c1, b, c2, count_exnext(argv[1]));
    fclose(out);
    return 0;
}
