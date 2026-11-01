/* Prints what DOS says about two paths: lock, full name and SameLock. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include "native_icons.h"

int main(int argc, char **argv)
{
    char name[512];
    if (argc != 3) { puts("same_probe PATH1 PATH2"); return 20; }
    BPTR a = Lock((CONST_STRPTR)argv[1], ACCESS_READ), b = Lock((CONST_STRPTR)argv[2], ACCESS_READ);
    printf("lock1=%p lock2=%p\n", (void *)a, (void *)b);
    if (a && NameFromLock(a, (STRPTR)name, sizeof name)) printf("name1=%s\n", name);
    if (b && NameFromLock(b, (STRPTR)name, sizeof name)) printf("name2=%s\n", name);
    if (a && b) printf("SameLock=%ld SameDevice=%d\n", (long)SameLock(a, b), (int)SameDevice(a, b));
    printf("dock_same_object=%d\n", dock_same_object(argv[1], argv[2]));
    if (a) UnLock(a);
    if (b) UnLock(b);
    return 0;
}
