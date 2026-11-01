#ifndef DOCK_PREFS_H
#define DOCK_PREFS_H
/* Appearance and behaviour, kept apart from dock.config so that existing
 * composition files load unchanged and a damaged prefs file costs only
 * the defaults, never the user's shortcuts. */
#define DOCK_ICON_MIN 32
#define DOCK_ICON_MAX 72
#define DOCK_ICON_DEFAULT 40
struct DockPrefs { int icon_size, corner, locked; };
void dock_prefs_default(struct DockPrefs *prefs);
int dock_prefs_valid(const struct DockPrefs *prefs);
/* 1 loaded, 0 missing, -1 invalid or unreadable; prefs untouched unless 1. */
int dock_prefs_load(struct DockPrefs *prefs, const char *path);
/* 1 saved. A failed write leaves any previous file in place. */
int dock_prefs_save(const struct DockPrefs *prefs, const char *path);
#endif
