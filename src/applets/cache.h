#ifndef DOCK_APPLET_CACHE_H
#define DOCK_APPLET_CACHE_H
#include "snapshot.h"
/* Settings and snapshot are a single publication: a failed settings/cache
 * update can never pair another city's response with old settings. */
int dock_applet_cache_write(FILE *, const char *,
                            const struct DockAppletSnapshot *);
int dock_applet_cache_parse(const char *, size_t, const char *,
                            struct DockAppletSnapshot *);
#endif
