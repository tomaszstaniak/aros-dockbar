/* Development probe, not shipped: does free memory drift while the
 * Preferences window is open, and does it depend on how it is polled?
 *   prefs_poll_probe MODE SECONDS LOGFILE [CONFIG]
 * MODE poll: Dockbar's loop (Delay(2), MUIM_Application_NewInput with 0);
 *      real: the same Delay loop, but NewInput only with the signals
 *            that actually arrived (SetSignal), as MUI expects;
 *      none: the same Delay loop with no window (control).
 * Logs free memory after a flush every 5 s. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prefs_window.h"

static struct DockSet set;

static ULONG free_after_flush(void)
{
    for (int i = 0; i < 3; i++) { APTR p = AllocMem(0x7ffffff0, MEMF_ANY); if (p) FreeMem(p, 0x7ffffff0); }
    return AvailMem(MEMF_ANY);
}

int main(int argc, char **argv)
{
    static struct DockPrefsWindow w;
    if (argc < 4) return 20;
    const char *mode = argv[1];
    int seconds = atoi(argv[2]);
    FILE *out = fopen(argv[3], "w");
    if (!out) return 20;
    if (argc < 5 || dock_set_load(&set, argv[4]) != 1) { dock_set_add(&set, NULL); }
    struct Screen *screen = LockPubScreen(NULL);
    int window = strcmp(mode, "none") != 0;
    if (window && !dock_prefs_window_open(&w, screen, &set, 0, -1, -1)) {
        fprintf(out, "open failed at %d\n", dock_prefs_window_failed);
        fclose(out);
        if (screen) UnlockPubScreen(NULL, screen);
        return 20;
    }
    ULONG first = free_after_flush();
    fprintf(out, "mode=%s window=%d start free=%lu\n", mode, window, (unsigned long)first);
    fflush(out);
    struct DateStamp start, now;
    DateStamp(&start);
    LONG next = 250, ticks = 0;
    ULONG signals = 0;
    while (ticks < seconds * 50) {
        if (!window || !strcmp(mode, "poll")) {
            if (window && dock_prefs_window_poll(&w) == DOCK_PREFS_CANCEL) break;
            Delay(2);
        } else {
            /* NewInput hands back the signals it waits for. */
            ULONG got = signals ? SetSignal(0, signals) & signals : 0;
            if (got || !signals) {
                ULONG s = got;
                IPTR id = DoMethod(w.application, MUIM_Application_NewInput, (IPTR)&s);
                if (id == (IPTR)MUIV_Application_ReturnID_Quit) break;
                signals = s;
            }
            Delay(2);
        }
        DateStamp(&now);
        ticks = (now.ds_Days - start.ds_Days) * 24 * 60 * 3000 + (now.ds_Minute - start.ds_Minute) * 3000 + now.ds_Tick - start.ds_Tick;
        if (ticks >= next) {
            ULONG mem = free_after_flush();
            fprintf(out, "%4lds free=%lu change=%ld\n", (long)(next / 50), (unsigned long)mem, (long)mem - (long)first);
            fflush(out);
            next += 250;
        }
    }
    if (window) dock_prefs_window_close(&w);
    fprintf(out, "closed free=%lu change=%ld\n", (unsigned long)free_after_flush(), (long)free_after_flush() - (long)first);
    fclose(out);
    if (screen) UnlockPubScreen(NULL, screen);
    return 0;
}
