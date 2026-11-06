#ifndef DOCK_APPLET_PANEL_AROS_H
#define DOCK_APPLET_PANEL_AROS_H
#include <stddef.h>
#include <stdint.h>
struct Screen;
struct DockColors;
struct DockAppletRuntime;
struct Window;
int dock_applet_ui_picker(struct Screen *);
int dock_applet_ui_open(struct Screen *, struct DockAppletRuntime *,
                        int settings);
int dock_applet_ui_actions(struct Screen *, struct DockAppletRuntime *);
/* After an install: "Add <name> to the dock?". Poll returns 1 with the id
 * when the user chose Add; dock_applet_ui_offered_dock then gives the dock
 * index chosen (once; -1 otherwise). */
int dock_applet_ui_offer(struct Screen *, const char *id, const char *name,
                         const char *const *docks, int count);
int dock_applet_ui_offered_dock(void);
int dock_applet_ui_open_at(struct Screen *, struct DockAppletRuntime *, int, int, int, int, int, const struct DockColors *);
/* 1: selected package id in output; 2: Remove current instance. */
int dock_applet_ui_poll(uint64_t, char *, size_t);
struct DockAppletRuntime *dock_applet_ui_instance(void);
int dock_applet_ui_shows(const struct DockAppletRuntime *);
void dock_applet_ui_forget(const struct DockAppletRuntime *);
struct Window *dock_applet_ui_window(void);
void dock_applet_ui_close(void);
/* Set by the host: 0 when this widget may not be changed now (its dock is
 * locked). Then settings open read-only as details, and Save and Remove are
 * refused with refusal() as the message. Looking at the card and Refresh
 * stay available. NULL: always editable. */
extern int (*dock_applet_ui_editable)(struct DockAppletRuntime *, const char **refusal);
#endif
