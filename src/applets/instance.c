#include "instance.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
void dock_applet_instance_init(struct DockAppletInstance *s,
                               const char *package, const char *instance,
                               const char *session, unsigned int seconds) {
  memset(s, 0, sizeof *s);
  snprintf(s->package, sizeof s->package, "%s", package);
  snprintf(s->instance, sizeof s->instance, "%s", instance);
  snprintf(s->session, sizeof s->session, "%s", session);
  s->view_revision = 1;
  s->refresh_ms = (unsigned long)seconds * 1000;
  s->snapshot.status = DA_LOADING;
}
void dock_applet_fail(struct DockAppletInstance *s, const char *why,
                      uint64_t now) {
  s->view_revision++;
  s->pending = 0;
  s->snapshot.status = s->has_data ? DA_STALE : DA_ERROR;
  snprintf(s->snapshot.message, sizeof s->snapshot.message, "%s", why);
  s->failures++;
  s->next_due = now + (s->failures == 1   ? 60000ul
                       : s->failures == 2 ? 300000ul
                                          : 900000ul);
}
int dock_applet_tick(struct DockAppletInstance *s, uint64_t now,
                     struct DockAppletRequest *q) {
  if (s->pending || now < s->next_due || s->generation >= INT_MAX)
    return 0;
  s->view_revision++;
  s->generation++;
  s->pending = 1;
  s->requested = now;
  s->snapshot.message[0] = 0;
  if (s->has_data)
    s->snapshot.status = DA_STALE;
  else
    s->snapshot.status = DA_LOADING;
  snprintf(q->package, sizeof q->package, "%s", s->package);
  snprintf(q->instance, sizeof q->instance, "%s", s->instance);
  snprintf(q->session, sizeof q->session, "%s", s->session);
  q->generation = s->generation;
  return 1;
}
int dock_applet_accept(struct DockAppletInstance *s,
                       const struct DockAppletSnapshot *r, uint64_t now) {
  if (!s->pending || strcmp(s->package, r->package) ||
      strcmp(s->instance, r->instance) || strcmp(s->session, r->session) ||
      s->generation != r->generation)
    return 0;
  if (r->status != DA_READY) {
    dock_applet_fail(
        s, r->message[0] ? r->message : "Provider returned no data", now);
    return 1;
  }
  s->view_revision++;
  s->snapshot = *r;
  s->has_data = 1;
  s->pending = 0;
  s->failures = 0;
  s->next_due = now + s->refresh_ms;
  return 1;
}
void dock_applet_instance_reset(struct DockAppletInstance *s, uint64_t now) {
  s->view_revision++;
  s->generation++;
  s->pending = s->has_data = s->failures = 0;
  memset(&s->snapshot, 0, sizeof s->snapshot);
  s->snapshot.status = DA_LOADING;
  s->next_due = now;
}
int dock_applet_refresh(struct DockAppletInstance *s, uint64_t now) {
  if (s->pending || (s->refreshed && now - s->last_refresh < 30000))
    return 0;
  s->refreshed = 1;
  s->last_refresh = now;
  s->next_due = now;
  return 1;
}

uint64_t dock_applet_ticks_ms(uint64_t ticks, uint64_t origin,
                              uint32_t frequency) {
  if (!frequency || ticks < origin)
    return 0;
  uint64_t elapsed = ticks - origin;
  return elapsed / frequency * 1000 + (elapsed % frequency) * 1000 / frequency;
}
