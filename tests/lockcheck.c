/* Development probe, not shipped: is anyone still holding a lock on PATH?
 * An exclusive lock is granted only when no other lock exists, so a drawer
 * that "Delete" calls "in use" fails here too, without deleting anything.
 *   lockcheck PATH LABEL      appends "LABEL free|in use" to RESULTS:lockcheck.log */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *out;
    if (argc < 3 || !(out = fopen("RESULTS:lockcheck.log", "a"))) return 20;
    BPTR lock = Lock((CONST_STRPTR)argv[1], EXCLUSIVE_LOCK);
    LONG error = lock ? 0 : IoErr();
    if (lock) UnLock(lock);
    fprintf(out, "%s %s (%ld)\n", argv[2], lock ? "free" : "in use", (long)error);
    printf("%s %s (%ld)\n", argv[2], lock ? "free" : "in use", (long)error);
    fclose(out);
    return lock ? 0 : 5;
}
