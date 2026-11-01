#include "widget.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    struct DockWidget a, b;
    char out[64];
    setenv("TZ", "UTC", 1);
    tzset();
    time_t t = 1791892800;  /* 2026-10-13 12:00:00 UTC */
    assert(dock_widget_start(&a, "clock:seconds"));
    assert(dock_widget_start(&b, ""));  /* 0.1 clock item */
    assert(dock_widget_update(&a, t) && !strcmp(a.line[0], "12:00:00") && a.lines == 1);
    assert(dock_widget_update(&b, t) && !strcmp(b.line[0], "12:00") && !strcmp(b.line[1], "13 Oct"));
    /* Within the same minute only the seconds clock asks for a redraw. */
    assert(dock_widget_update(&a, t + 1) && !dock_widget_update(&b, t + 1));
    assert(!dock_widget_update(&a, t + 1));
    assert(dock_widget_update(&b, t + 60) && !strcmp(b.line[0], "12:01"));
    a.cls->settings(&a, out, sizeof out);
    assert(!strcmp(out, "clock:seconds"));
    b.options |= DOCK_CLOCK_SECONDS;
    b.cls->settings(&b, out, sizeof out);
    assert(!strcmp(out, "clock:seconds,date"));
    assert(!dock_widget_start(&a, "clock:bogus") && !a.cls);
    assert(!dock_widget_start(&a, "weather:") && !a.cls);
    dock_widget_stop(&b);
    assert(!b.cls && !dock_widget_update(&b, t));
    puts("PASS widget: clock settings, per-instance redraw only on change, unknown widgets refused");
    return 0;
}
