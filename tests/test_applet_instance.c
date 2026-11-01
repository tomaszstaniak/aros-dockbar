#include "applets/instance.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  struct DockAppletInstance *s = calloc(1, sizeof *s);
  struct DockAppletSnapshot *r = calloc(1, sizeof *r);
  struct DockAppletRequest q;
  dock_applet_instance_init(s, "org.test", "one", "host1", 900);
  assert(dock_applet_tick(s, 0, &q));
  assert(!dock_applet_tick(s, 100, &q));
  strcpy(r->package, "org.test");
  strcpy(r->instance, "one");
  strcpy(r->session, "other");
  r->generation = q.generation;
  r->status = DA_READY;
  assert(!dock_applet_accept(s, r, 100));
  strcpy(r->session, "host1");
  assert(dock_applet_accept(s, r, 100));
  assert(s->has_data);
  unsigned long ready_revision = s->view_revision;
  assert(!dock_applet_tick(s, 1000, &q));
  assert(dock_applet_tick(s, 900100, &q));
  r->generation = q.generation;
  r->status = DA_ERROR;
  strcpy(r->message, "TLS failed");
  assert(dock_applet_accept(s, r, 900200));
  assert(s->snapshot.status == DA_STALE && s->has_data);
  assert(s->view_revision > ready_revision);
  assert(!dock_applet_tick(s, 901000, &q));
  assert(dock_applet_tick(s, 960200, &q));
  dock_applet_instance_reset(s, 960300);
  r->status = DA_READY;
  assert(!dock_applet_accept(s, r, 960400));
  assert(!s->has_data);
  assert(dock_applet_ticks_ms(5000000, 1000000, 1000) == 4000000);
  assert(dock_applet_ticks_ms(4294968000ULL, 0, 1000) == 4294968000ULL);
  dock_applet_instance_init(s, "org.test", "two", "host1", 900);
  assert(dock_applet_tick(s, 4294967200ULL, &q));
  strcpy(r->instance, "two");
  r->generation = q.generation;
  assert(dock_applet_accept(s, r, 4294967201ULL));
  assert(!dock_applet_tick(s, 4294968200ULL, &q));
  assert(dock_applet_tick(s, 4295867201ULL, &q));
  puts("applet instance: passed");
  free(s);
  free(r);
  return 0;
}
