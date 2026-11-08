#ifndef DOCK_MIGRATE_H
#define DOCK_MIGRATE_H

/* Settings of a package this one replaced (apkg `replaces`).

   apkg installs a package in a drawer named after its id and, when it
   replaces another package, removes that package's files but keeps the
   files the user changed or added in the old drawer. Dockbar 0.5.1 had two
   ids, dockbar and dockbar-info-only; since 0.5.2 there is only dockbar.

   dock_migrate_from_sibling() copies docks.config, dock.pins and the
   WidgetState drawer from the sibling drawer `old_name` into PROGDIR: once:
   only when PROGDIR: is a drawer named `new_name`, has none of these files
   yet, and the sibling has a docks.config. The old drawer is not changed.
   The result: 1 copied, 0 nothing to do, -1 failed (PROGDIR: unchanged
   apart from a log line). `note` receives one line per step. */
int dock_migrate_from_sibling(const char *new_name, const char *old_name,
                              void (*note)(const char *format, ...));

/* The same from any drawer `old` (a full path), with the same rules apart
   from the drawer names: used for the settings of a copy of Dockbar that
   was started from SYS:WBStartup (autostart.h). */
int dock_migrate_from_dir(const char *old, void (*note)(const char *format, ...));

/* 1 when PROGDIR: has docks.config, dock.config, dock.pins or WidgetState:
   then neither function copies anything. */
int dock_migrate_has_settings(void);

#endif
