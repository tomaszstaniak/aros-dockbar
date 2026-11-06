#ifndef DOCK_APPLET_PACKAGE_H
#define DOCK_APPLET_PACKAGE_H
#include "snapshot.h"
#define DA_PACKAGES 64
#define DA_ACTIONS 8
struct DockAppletTarget {
  const char *host, *variant;
  int api; /* highest widget API this host speaks; DA_API_MIN is the lowest */
};
struct DockAppletPackage {
  char id[DA_ID], name[80], version[32], description[256], license[80],
      root[DA_PATH], entry[DA_PATH], reason[160];
  char primary[40], secondary[40], tooltip[40], operation[40];
  /* API 2: refresh "none" means the host never starts the provider by
   * itself, only for one of the actions when the user chooses it. */
  char action_id[DA_ACTIONS][40], action_label[DA_ACTIONS][40];
  int available, api, manual, action_count;
};
struct DockAppletCatalog {
  struct DockAppletPackage packages[DA_PACKAGES];
  int count, overflow;
};
int dock_applet_relative(const char *path);
int dock_applet_path(const char *root, const char *relative, char *out,
                     size_t cap);
int dock_applet_scan(const char *, const struct DockAppletTarget *,
                     struct DockAppletCatalog *);
int dock_applet_package_refresh(struct DockAppletPackage *,
                                const struct DockAppletCatalog *, const char *,
                                int);
/* One package drawer outside the catalogue (an install source). */
int dock_applet_load_dir(const char *dir, const struct DockAppletTarget *,
                         struct DockAppletPackage *);
const struct DockAppletPackage *
dock_applet_find(const struct DockAppletCatalog *, const char *id);
#endif
