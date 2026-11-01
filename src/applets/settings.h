#ifndef DOCK_APPLET_SETTINGS_H
#define DOCK_APPLET_SETTINGS_H
#include <stddef.h>
#define DA_FORM_FIELDS 8
#define DA_CHOICES 8
struct DockAppletSetting {
  char key[40], label[80], value[160], type[16], choices[DA_CHOICES][40];
  int choice_count, max_length;
  double min, max;
};
struct DockAppletForm {
  struct DockAppletSetting fields[DA_FORM_FIELDS];
  int count;
  char search[40], search_label[80], name_key[40], latitude_key[40],
      longitude_key[40];
};
int dock_applet_settings_schema(const char *, struct DockAppletForm *, char *,
                                size_t);
int dock_applet_settings_load(const char *, struct DockAppletForm *);
int dock_applet_settings_recover(const char *, struct DockAppletForm *, int *);
int dock_applet_settings_encode(const struct DockAppletForm *, char *, size_t,
                                char *, size_t);
#endif
