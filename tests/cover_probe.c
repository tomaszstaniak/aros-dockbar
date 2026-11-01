/* Development probe, not shipped: a window that covers a dock for a while,
 * opened and closed on a timer so the pointer can stay still meanwhile.
 *   cover_probe LEFT TOP WIDTH HEIGHT WAIT HOLD [LOG]
 * Waits WAIT ticks (1/50 s), opens an ordinary window there (activated, as a
 * started program's window is), keeps it HOLD ticks and closes it. LOG gets
 * one line per step with the time since start, in ms. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>

static long ms(const struct DateStamp *t0)
{
    struct DateStamp t;
    DateStamp(&t);
    return ((t.ds_Days - t0->ds_Days) * 1440L * 60 + (t.ds_Minute - t0->ds_Minute) * 60L) * 1000 +
           (t.ds_Tick - t0->ds_Tick) * 20L;
}

int main(int argc, char **argv)
{
    struct DateStamp t0;
    struct Window *w;
    FILE *log;
    if (argc < 7) return 20;
    log = argc > 7 ? fopen(argv[7], "w") : NULL;
    DateStamp(&t0);
    Delay(atoi(argv[5]));
    w = OpenWindowTags(NULL, WA_Left, atoi(argv[1]), WA_Top, atoi(argv[2]), WA_Width, atoi(argv[3]),
        WA_Height, atoi(argv[4]), WA_Title, (IPTR)"cover", WA_DragBar, TRUE, WA_Activate, TRUE,
        WA_SmartRefresh, TRUE, TAG_DONE);
    if (log) fprintf(log, "%ld open %s\n", ms(&t0), w ? "ok" : "failed");
    if (w) {
        Delay(atoi(argv[6]));
        CloseWindow(w);
        if (log) fprintf(log, "%ld closed\n", ms(&t0));
    }
    if (log) fclose(log);
    return w ? 0 : 10;
}
