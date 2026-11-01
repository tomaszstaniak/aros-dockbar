/* Development probe, not shipped: is the memory left by each command a
 * dock starts Dockbar's, or the system's? Starts COMMAND COUNT times the way
 * Dockbar starts a command without a Shell window (SystemTags, asynchronous,
 * NIL: in and out, a lock as current directory), waits WAIT ticks after
 * each, and logs free memory after a flush every 10 starts.
 *   cmd_probe MODE COUNT WAIT LOGFILE COMMAND
 * MODE dock: exactly Dockbar's tags; plain: no SYS_UserShell, no
 * NP_CurrentDir, no NP_StackSize; sync: Dockbar's tags, not asynchronous. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ULONG free_after_flush(void)
{
    for (int i = 0; i < 3; i++) { APTR p = AllocMem(0x7ffffff0, MEMF_ANY); if (p) FreeMem(p, 0x7ffffff0); }
    return AvailMem(MEMF_ANY);
}

int main(int argc, char **argv)
{
    if (argc < 6) return 20;
    const char *mode = argv[1], *command = argv[5];
    int count = atoi(argv[2]), wait = atoi(argv[3]);
    FILE *out = fopen(argv[4], "w");
    if (!out) return 20;
    ULONG first = free_after_flush();
    fprintf(out, "mode=%s count=%d start free=%lu\n", mode, count, (unsigned long)first);
    fflush(out);
    for (int i = 1; i <= count; i++) {
        BPTR in = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE), o = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
        BPTR dir = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);
        LONG rc;
        if (!strcmp(mode, "plain"))
            rc = SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)in, SYS_Output, (IPTR)o, SYS_Asynch, TRUE, TAG_END);
        else
            rc = SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)in, SYS_Output, (IPTR)o,
                            SYS_Asynch, strcmp(mode, "sync") ? TRUE : FALSE, SYS_UserShell, TRUE,
                            NP_CurrentDir, (IPTR)dir, NP_StackSize, 65536, NP_Name, (IPTR)"Dockbar command", TAG_END);
        /* Asynchronous: the new process owns the handles and the lock.
         * Synchronous, or a failed start: they are still ours. */
        if (rc == -1 || !strcmp(mode, "sync")) { Close(in); Close(o); }
        if (rc == -1 || !strcmp(mode, "sync") || !strcmp(mode, "plain")) UnLock(dir);
        Delay(wait);
        if (i % 10 == 0) {
            ULONG mem = free_after_flush();
            fprintf(out, "%3d free=%lu change=%ld rc=%ld\n", i, (unsigned long)mem, (long)mem - (long)first, (long)rc);
            fflush(out);
        }
    }
    fclose(out);
    return 0;
}
