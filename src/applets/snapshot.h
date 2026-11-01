#ifndef DOCK_APPLET_SNAPSHOT_H
#define DOCK_APPLET_SNAPSHOT_H
#include <stddef.h>
#include <stdio.h>
#define DA_ID 80
#define DA_PATH 1024
#define DA_FIELDS 24
#define DA_ROWS 64
#define DA_RESPONSE_LIMIT (128u * 1024u)
enum DockAppletStatus { DA_LOADING, DA_READY, DA_STALE, DA_ERROR };
struct DockAppletField {
  char key[40], text[160], unit[32];
  int numeric;
  double number;
};
struct DockAppletRow {
  char label[160], value[160], icon[48];
  char section[40], caption[80], detail[80];
  double plot;
  int has_plot;
};
struct DockAppletSnapshot {
  char package[DA_ID], instance[DA_ID], session[DA_ID], fingerprint[160];
  unsigned long generation;
  double updated, source_time;
  enum DockAppletStatus status;
  char title[160], icon[48], message[160], attribution[160], url[256];
  struct DockAppletField fields[DA_FIELDS];
  int field_count;
  struct DockAppletRow rows[DA_ROWS];
  int row_count;
};
int dock_applet_id_valid(const char *id);
int dock_applet_snapshot_parse(const char *, size_t,
                               struct DockAppletSnapshot *);
int dock_applet_snapshot_write(FILE *, const struct DockAppletSnapshot *);
const char *dock_applet_field(const struct DockAppletSnapshot *,
                              const char *key);
#endif
