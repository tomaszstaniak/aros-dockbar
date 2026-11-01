#ifndef DOCK_APPLET_PACKAGE_H
#define DOCK_APPLET_PACKAGE_H
#include "snapshot.h"
#define DA_PACKAGES 64
struct DockAppletTarget {
  const char *host, *variant;
  int api;
};
struct DockAppletPackage {
  char id[DA_ID], name[80], version[32], description[256], license[80],
      root[DA_PATH], entry[DA_PATH], reason[160];
  char primary[40], secondary[40], tooltip[40], operation[40];
  int available;
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
const struct DockAppletPackage *
dock_applet_find(const struct DockAppletCatalog *, const char *id);
#endif
