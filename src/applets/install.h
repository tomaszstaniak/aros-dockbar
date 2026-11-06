#ifndef DOCK_APPLET_INSTALL_H
#define DOCK_APPLET_INSTALL_H
#include "package.h"
#include <stddef.h>
/* Same values as DOCKBAR_WIDGETS_* in include/dockbar/widgets.h. */
#define DA_INSTALL_OK 0
#define DA_INSTALL_UPDATED 1
#define DA_INSTALL_REFUSED 11
#define DA_INSTALL_BUSY 12
#define DA_INSTALL_FAILED 13
/* Loads the source drawer as a package for this host; text gets its name
 * or the reason it is refused. */
int dock_applet_install_check(const char *source,
                              const struct DockAppletTarget *,
                              struct DockAppletPackage *, char *text,
                              size_t cap);
/* Copies a checked package into root: staged in .<id>.installing, the
 * installed package with that id (any folder name) set aside in .<id>.old
 * and deleted once the new one is in place; on failure the installed one
 * stays. WidgetState is not touched. Rescan afterwards. */
int dock_applet_install_copy(const struct DockAppletPackage *,
                             const char *root, const struct DockAppletTarget *,
                             const struct DockAppletCatalog *, char *text,
                             size_t cap);
#endif
