#ifndef DOCK_APPLET_INSTANCE_H
#define DOCK_APPLET_INSTANCE_H
#include "snapshot.h"
#include <stdint.h>
struct DockAppletRequest {
  char package[DA_ID], instance[DA_ID], session[DA_ID];
  unsigned long generation;
};
struct DockAppletInstance {
  struct DockAppletSnapshot snapshot;
  char package[DA_ID], instance[DA_ID], session[DA_ID];
  unsigned long view_revision, generation;
  uint64_t next_due, requested, last_refresh, refresh_ms;
  int pending, has_data, failures, refreshed;
};
void dock_applet_instance_init(struct DockAppletInstance *, const char *,
                               const char *, const char *,
                               unsigned int refresh_seconds);
int dock_applet_tick(struct DockAppletInstance *, uint64_t,
                     struct DockAppletRequest *);
int dock_applet_accept(struct DockAppletInstance *,
                       const struct DockAppletSnapshot *, uint64_t);
void dock_applet_fail(struct DockAppletInstance *, const char *, uint64_t);
void dock_applet_instance_reset(struct DockAppletInstance *, uint64_t);
int dock_applet_refresh(struct DockAppletInstance *, uint64_t);
uint64_t dock_applet_ticks_ms(uint64_t, uint64_t, uint32_t);
#endif
