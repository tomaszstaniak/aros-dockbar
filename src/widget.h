#ifndef DOCK_WIDGET_H
#define DOCK_WIDGET_H
#include <stddef.h>
#include <time.h>

/* Built-in widget contract. A widget owns its state and produces up to two
 * text lines; the dock measures and draws them, so widgets never touch
 * windows. update() must be cheap and non-blocking and returns 1 only when
 * the visible text changed. Settings round-trip through a short string that
 * the dock stores in the item ("clock:seconds,date"). */

#define DOCK_WIDGET_TEXT 32

struct DockWidget;
struct DockWidgetClass {
    const char *prefix;  /* settings prefix, e.g. "clock:" */
    int (*start)(struct DockWidget *widget, const char *settings);
    int (*update)(struct DockWidget *widget, time_t now);
    void (*settings)(const struct DockWidget *widget, char *out, size_t capacity);
    void (*stop)(struct DockWidget *widget);
};

struct DockWidget {
    const struct DockWidgetClass *cls;
    char line[2][DOCK_WIDGET_TEXT];
    int lines;
    int options;  /* class-specific flags */
    time_t last;
};

extern const struct DockWidgetClass dock_clock_class;
enum { DOCK_CLOCK_SECONDS = 1, DOCK_CLOCK_DATE = 2 };

/* Starts the class whose prefix matches settings; 0 for unknown or invalid. */
int dock_widget_start(struct DockWidget *widget, const char *settings);
int dock_widget_update(struct DockWidget *widget, time_t now);
void dock_widget_stop(struct DockWidget *widget);
#endif
