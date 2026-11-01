#include "elapsed.h"
#include <stdlib.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/intuition.h>

/* GetUpTime() counts tb_Elapsed, which only the tick interrupt advances:
 * TR_SETSYSTIME rewrites the calendar time and, on PC, the EClock counter
 * (arch/all-pc/timer/ticks.c EClockSet), but not the uptime, in mainline
 * and ABIv11 alike. ReadEClock() is therefore no elapsed clock on PC: it
 * went back with a timesync and Dockbar's widgets stopped until a restart.
 * The request is never sent, so it needs no reply port (and no signal). */
static struct timerequest uptime_request;
static int uptime_state; /* 0 not tried, 1 open, -1 not available */
static struct DockElapsed elapsed;

static void uptime_close(void)
{
    if (uptime_state == 1) CloseDevice((struct IORequest *)&uptime_request);
    uptime_state = -1;
}

uint64_t dock_elapsed_us(void)
{
    if (!uptime_state) {
        uptime_request.tr_node.io_Message.mn_Length = sizeof uptime_request;
        if (!OpenDevice((CONST_STRPTR)"timer.device", UNIT_MICROHZ, (struct IORequest *)&uptime_request, 0)) {
            uptime_state = 1;
            atexit(uptime_close);
        } else uptime_state = -1;
    }
    if (uptime_state == 1) {
        struct Device *TimerBase = uptime_request.tr_node.io_Device;
        struct timeval tv;
        GetUpTime(&tv);
        return dock_elapsed_feed(&elapsed, (uint64_t)tv.tv_secs * 1000000u + tv.tv_micro, 0);
    }
    /* No timer.device: the calendar clock, with any step of it (back, or
     * more than ten seconds ahead in one look) counted as no time. */
    ULONG seconds, micros;
    CurrentTime(&seconds, &micros);
    return dock_elapsed_feed(&elapsed, (uint64_t)seconds * 1000000u + micros, 10000000u);
}

uint64_t dock_elapsed_ms(void)
{
    return dock_elapsed_us() / 1000u;
}
