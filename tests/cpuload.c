/* Development probe, not shipped: CPU left over for the lowest priority.
 * Counts loop rounds at priority -127 for SECONDS; whatever runs above it
 * (Dockbar, Wanderer) lowers the count. Compare runs on one boot only.
 *   cpuload LOGFILE SECONDS LABEL */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>

static long ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (d.ds_Days * 1440L + d.ds_Minute) * 3000L + d.ds_Tick;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 20;
    long seconds = atol(argv[2]);
    if (seconds < 1) return 20;
    BYTE was = SetTaskPri(FindTask(NULL), -127);
    volatile unsigned long count = 0;
    long start = ticks(), end = start + seconds * 50;
    while (ticks() < end)
        for (int i = 0; i < 4096; i++) count++;
    long took = ticks() - start;
    SetTaskPri(FindTask(NULL), was);
    FILE *out = fopen(argv[1], "a");
    if (!out) return 20;
    fprintf(out, "%s seconds=%ld rounds=%lu per_s=%lu\n", argv[3], took / 50,
            (unsigned long)count, (unsigned long)(count / (unsigned long)(took / 50 ? took / 50 : 1)));
    fclose(out);
    return 0;
}
