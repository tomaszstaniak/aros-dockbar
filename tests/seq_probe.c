/* Development probe, not shipped: runs a timed sequence of commands with no
 * window or console of its own, and logs free memory (after a flush) at
 * each step. For screen-mode changes and memory checks while no Shell
 * window is open (a Shell window stops Intuition's screen reset).
 *   seq_probe            (from Wanderer or WBRun; reads ENV:seq)
 * ENV:seq: first line the log file, then lines "SECONDS COMMAND", where
 * SECONDS counts from the start and COMMAND "-" only measures. */
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
    /* Asking for far too much makes exec expunge unused libraries and
     * fonts first, as Avail FLUSH does. */
    for (int i = 0; i < 3; i++) { APTR p = AllocMem(0x7ffffff0, MEMF_ANY); if (p) FreeMem(p, 0x7ffffff0); }
    return AvailMem(MEMF_ANY);
}

int main(void)
{
    static char text[4096];
    LONG n = GetVar((CONST_STRPTR)"seq", (STRPTR)text, sizeof text, GVF_BINARY_VAR);
    if (n <= 0) return 20;
    char *line = strtok(text, "\n");
    FILE *out = line ? fopen(line, "w") : NULL;
    if (!out) return 20;
    struct DateStamp start;
    DateStamp(&start);
    ULONG first = 0;
    while ((line = strtok(NULL, "\n"))) {
        char *command = strchr(line, ' ');
        if (!command) continue;
        *command++ = 0;
        LONG at = atoi(line) * 50;
        for (;;) {
            struct DateStamp now;
            DateStamp(&now);
            LONG ticks = (now.ds_Days - start.ds_Days) * 24 * 60 * 3000 + (now.ds_Minute - start.ds_Minute) * 3000 + now.ds_Tick - start.ds_Tick;
            if (ticks >= at) break;
            Delay(at - ticks > 50 ? 50 : at - ticks);
        }
        ULONG mem = free_after_flush();
        if (!first) first = mem;
        fprintf(out, "%4lds free=%lu change=%ld before: %s\n", (long)(at / 50), (unsigned long)mem, (long)mem - (long)first, command);
        fflush(out);
        if (strcmp(command, "-")) {
            BPTR nil = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
            LONG rc = SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)nil, SYS_Output, (IPTR)NULL, NP_StackSize, 65536, TAG_END);
            if (rc == -1) Close(nil);
            fprintf(out, "      rc=%ld\n", (long)rc);
            fflush(out);
        }
    }
    fclose(out);
    return 0;
}
