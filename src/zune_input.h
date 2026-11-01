#ifndef DOCK_ZUNE_INPUT_H
#define DOCK_ZUNE_INPUT_H
/* Polls a Zune application from Dockbar's own loop (which never Wait()s on
 * Zune's signals). MUIM_Application_NewInput with 0 makes Zune assume every
 * one of its signals arrived; called like that 25 times a second, the
 * ABIv11 Zune lost about 3 KB of memory per second while a window was open
 * (measured with tests/prefs_poll_probe). So NewInput is called only with
 * the signals that really arrived, plus once more after each answer,
 * because Zune hands back one queued return ID per call. *wait starts at 0;
 * the first call learns the signals from Zune. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>

static inline IPTR dock_zune_input(void *application, unsigned int *wait, int *again)
{
    ULONG got = *wait ? SetSignal(0, *wait) & *wait : 0;
    if (*wait && !got && !*again) return 0;
    ULONG signals = got;
    IPTR id = DoMethod((Object *)application, MUIM_Application_NewInput, (IPTR)&signals);
    *wait = signals;
    *again = id != 0;
    return id;
}
#endif
