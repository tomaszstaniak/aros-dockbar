#ifndef DOCK_APPLET_PURGE_H
#define DOCK_APPLET_PURGE_H
#include <stddef.h>
/* Removes what one widget instance keeps in its own state drawer: the
 * files Dockbar itself writes there, by name, and then the drawer if it is
 * empty. Nothing else is touched, so an unknown file keeps the drawer.
 * 1 when the drawer is gone; 0 with error saying what stayed. */
int dock_applet_purge_dir(const char *dir, char *error, size_t cap);
#endif
