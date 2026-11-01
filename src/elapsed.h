#ifndef DOCK_ELAPSED_H
#define DOCK_ELAPSED_H
#include <stdint.h>

/* Two clocks, never mixed. Calendar time (time(), CurrentTime) is what the
 * dock shows: the clock widget, forecast hours, "updated N min ago". It can
 * step either way at any moment (Prefs/Time, timesync, SetClock). Elapsed
 * time is what everything that waits or animates counts with: timeouts,
 * widget refreshes, fades, hover, drags, double-clicks. It only goes
 * forwards; a step of the calendar clock must not stall or hurry it. */

/* Accumulates elapsed time from a raw counter. A raw value below the last
 * one adds nothing (the counter was reset), so time goes on from there
 * instead of waiting for the counter to catch up; a forward step larger
 * than max_step (0: no limit) adds nothing either. */
struct DockElapsed {
    uint64_t last, total;
    int started;
};
uint64_t dock_elapsed_feed(struct DockElapsed *e, uint64_t raw, uint64_t max_step);

/* Microseconds and milliseconds of elapsed time since the first call. */
uint64_t dock_elapsed_us(void);
uint64_t dock_elapsed_ms(void);
#endif
