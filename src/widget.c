#include "widget.h"
#include <stdio.h>
#include <string.h>

static int clock_start(struct DockWidget *w, const char *settings)
{
    const char *p = settings + strlen("clock:");
    w->options = 0;
    while (*p) {
        size_t n = strcspn(p, ",");
        if (n == 7 && !strncmp(p, "seconds", n)) w->options |= DOCK_CLOCK_SECONDS;
        else if (n == 4 && !strncmp(p, "date", n)) w->options |= DOCK_CLOCK_DATE;
        else if (n) return 0;
        p += n + (p[n] == ',');
    }
    w->last = (time_t)-1;
    return 1;
}

static int clock_update(struct DockWidget *w, time_t now)
{
    char line[2][DOCK_WIDGET_TEXT];
    struct tm *tm;
    int lines;
    /* Without seconds the text only changes per minute. */
    time_t step = w->options & DOCK_CLOCK_SECONDS ? now : now - now % 60;
    if (step == w->last) return 0;
    w->last = step;
    tm = localtime(&now);
    if (!tm) return 0;
    strftime(line[0], sizeof line[0], w->options & DOCK_CLOCK_SECONDS ? "%H:%M:%S" : "%H:%M", tm);
    lines = 1;
    if (w->options & DOCK_CLOCK_DATE) { strftime(line[1], sizeof line[1], "%d %b", tm); lines = 2; }
    else line[1][0] = 0;
    if (lines == w->lines && !strcmp(line[0], w->line[0]) && !strcmp(line[1], w->line[1])) return 0;
    memcpy(w->line, line, sizeof line);
    w->lines = lines;
    return 1;
}

static void clock_settings(const struct DockWidget *w, char *out, size_t capacity)
{
    snprintf(out, capacity, "clock:%s%s%s", w->options & DOCK_CLOCK_SECONDS ? "seconds" : "",
             (w->options & DOCK_CLOCK_SECONDS) && (w->options & DOCK_CLOCK_DATE) ? "," : "",
             w->options & DOCK_CLOCK_DATE ? "date" : "");
}

static void clock_stop(struct DockWidget *w) { (void)w; }

const struct DockWidgetClass dock_clock_class = { "clock:", clock_start, clock_update, clock_settings, clock_stop };

static const struct DockWidgetClass *const classes[] = { &dock_clock_class };

int dock_widget_start(struct DockWidget *widget, const char *settings)
{
    memset(widget, 0, sizeof(*widget));
    /* 0.1 stored clocks with an empty path: the time and date it showed. */
    if (!settings || !*settings) settings = "clock:date";
    for (size_t i = 0; i < sizeof classes / sizeof *classes; i++)
        if (!strncmp(settings, classes[i]->prefix, strlen(classes[i]->prefix))) {
            widget->cls = classes[i];
            if (classes[i]->start(widget, settings)) return 1;
            break;
        }
    memset(widget, 0, sizeof(*widget));
    return 0;
}

int dock_widget_update(struct DockWidget *widget, time_t now)
{
    return widget && widget->cls ? widget->cls->update(widget, now) : 0;
}

void dock_widget_stop(struct DockWidget *widget)
{
    if (widget && widget->cls) widget->cls->stop(widget);
    if (widget) memset(widget, 0, sizeof(*widget));
}
