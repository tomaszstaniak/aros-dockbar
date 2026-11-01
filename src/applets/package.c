#define _POSIX_C_SOURCE 200809L
#include "package.h"
#include "json.h"
#include "view.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int dock_applet_relative(const char *p) {
  if (!p || !*p || *p == '/' || strlen(p) > 512)
    return 0;
  for (const char *s = p; *s;) {
    const char *e = strchr(s, '/');
    size_t n = e ? (size_t)(e - s) : strlen(s);
    if (!n || (n == 1 && *s == '.') || (n == 2 && !memcmp(s, "..", 2)))
      return 0;
    for (size_t i = 0; i < n; i++)
      if ((unsigned char)s[i] < 32 || s[i] == ':' || s[i] == '\\')
        return 0;
    if (!e)
      break;
    s = e + 1;
    if (!*s)
      return 0;
  }
  return 1;
}
int dock_applet_path(const char *root, const char *rel, char *out, size_t cap) {
  if (!dock_applet_relative(rel))
    return 0;
  size_t n = strlen(root);
  int w = snprintf(out, cap, "%s%s%s", root, n && root[n - 1] == ':' ? "" : "/",
                   rel);
  if (w < 0 || (size_t)w >= cap)
    return 0;
  /* Check every component, including intermediate directories, before use. */
  char tmp[DA_PATH];
  if (strlen(out) >= sizeof tmp)
    return 0;
  strcpy(tmp, out);
  for (char *p = tmp + n; *p; p++) {
    if (*p != '/')
      continue;
    *p = 0;
    struct stat st;
    if (lstat(tmp, &st) || S_ISLNK(st.st_mode)) {
      *p = '/';
      return 0;
    }
    *p = '/';
  }
  struct stat st;
  return !lstat(tmp, &st) && !S_ISLNK(st.st_mode);
}
static int str(const struct DockJson *j, int o, const char *key, char *out,
               size_t cap) {
  return dock_json_string(j, dock_json_get(j, o, key), out, cap);
}
static void load(struct DockAppletPackage *p,
                 const struct DockAppletTarget *target) {
  struct DockJson *j = malloc(sizeof *j);
  char path[DA_PATH], relative[512];
  size_t n = 0;
  char *b = NULL;
  int v, min, max;
  strcpy(p->reason, "Invalid package metadata");
  if (!j)
    return;
  if (!dock_applet_path(p->root, "applet.json", path, sizeof path) ||
      (b = dock_applet_read(path, 65536, &n)) == NULL ||
      !dock_json_parse(j, b, n))
    goto done;
  if (!dock_json_int(j, dock_json_get(j, 0, "schema"), &v) || v != 1 ||
      !str(j, 0, "id", p->id, sizeof p->id) || !dock_applet_id_valid(p->id) ||
      !str(j, 0, "name", p->name, sizeof p->name) ||
      !str(j, 0, "version", p->version, sizeof p->version) ||
      !str(j, 0, "license", p->license, sizeof p->license) ||
      !str(j, 0, "description", p->description, sizeof p->description))
    goto done;
  strcpy(p->operation, "update");
  if (dock_json_get(j, 0, "refreshOperation") >= 0 &&
      !str(j, 0, "refreshOperation", p->operation, sizeof p->operation))
    goto done;
  int api = dock_json_get(j, 0, "api");
  if (!dock_json_int(j, dock_json_get(j, api, "min"), &min) ||
      !dock_json_int(j, dock_json_get(j, api, "max"), &max) || min > max)
    goto done;
  if (target->api < min || target->api > max) {
    strcpy(p->reason, "Unsupported widget API");
    goto done;
  }
  int hosts = dock_json_get(j, 0, "hosts"), found = 0;
  for (int i = 0; i < dock_json_count(j, hosts); i++)
    if (dock_json_is(j, dock_json_at(j, hosts, i), target->host))
      found = 1;
  if (!found) {
    strcpy(p->reason, "Unsupported host");
    goto done;
  }
  int entries = dock_json_get(j, 0, "entryPoints");
  if (!str(j, entries, target->variant, relative, sizeof relative)) {
    strcpy(p->reason, "No provider for this architecture/ABI");
    goto done;
  }
  if (!dock_applet_path(p->root, relative, p->entry, sizeof p->entry)) {
    strcpy(p->reason, "Provider missing or unsafe path");
    goto done;
  }
  struct stat st;
  if (stat(p->entry, &st) || !S_ISREG(st.st_mode)) {
    strcpy(p->reason, "Provider is not a regular file");
    goto done;
  }
  if (!dock_applet_path(p->root, "LICENSE", path, sizeof path))
    goto done;
  if (!str(j, 0, "settings", relative, sizeof relative) ||
      !dock_applet_path(p->root, relative, path, sizeof path))
    goto done;
  char views[512];
  if (!str(j, 0, "views", views, sizeof views))
    goto done;
  free(b);
  b = dock_applet_read(path, 65536, &n);
  if (!b || !dock_json_parse(j, b, n))
    goto done;
  if (!dock_applet_path(p->root, views, path, sizeof path))
    goto done;
  free(b);
  b = dock_applet_read(path, 65536, &n);
  if (!b || !dock_json_parse(j, b, n))
    goto done;
  int compact = dock_json_get(j, 0, "compact");
  if (!str(j, compact, "primary", p->primary, sizeof p->primary) ||
      !str(j, compact, "secondary", p->secondary, sizeof p->secondary))
    goto done;
  if (dock_json_get(j, compact, "tooltip") >= 0 &&
      !str(j, compact, "tooltip", p->tooltip, sizeof p->tooltip))
    goto done;
  int expanded = dock_json_get(j, 0, "expanded");
  struct DockAppletView card_view;
  if (!dock_json_is(j, dock_json_get(j, expanded, "component"), "rows") &&
      !dock_applet_view_parse(b,n,&card_view)) {
    strcpy(p->reason, "Unsupported required view component");
    goto done;
  }
  p->available = 1;
  p->reason[0] = 0;
done:
  free(b);
  free(j);
}
int dock_applet_scan(const char *root, const struct DockAppletTarget *t,
                     struct DockAppletCatalog *c) {
  memset(c, 0, sizeof *c);
  DIR *dir = opendir(root);
  if (!dir)
    return 0;
  struct dirent *de;
  while ((de = readdir(dir))) {
    if (de->d_name[0] == '.')
      continue;
    char p[DA_PATH];
    int w = snprintf(p, sizeof p, "%s%s%s", root,
                     root[0] && root[strlen(root) - 1] == ':' ? "" : "/",
                     de->d_name);
    if (w < 0 || (size_t)w >= sizeof p)
      continue;
    struct stat st;
    if (lstat(p, &st) || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
      continue;
    if (c->count == DA_PACKAGES) {
      c->overflow = 1;
      continue;
    }
    struct DockAppletPackage *pkg = &c->packages[c->count++];
    strcpy(pkg->root, p);
    snprintf(pkg->name, sizeof pkg->name, "%.79s", de->d_name);
    load(pkg, t);
  }
  closedir(dir);
  for (int i = 0; i < c->count; i++)
    for (int k = 0; k < i; k++)
      if (c->packages[i].id[0] &&
          !strcmp(c->packages[i].id, c->packages[k].id)) {
        c->packages[i].available = c->packages[k].available = 0;
        strcpy(c->packages[i].reason, "Duplicate package ID");
        strcpy(c->packages[k].reason, "Duplicate package ID");
      }
  return 1;
}
const struct DockAppletPackage *
dock_applet_find(const struct DockAppletCatalog *c, const char *id) {
  for (int i = 0; i < c->count; i++)
    if (!strcmp(c->packages[i].id, id))
      return &c->packages[i];
  return NULL;
}

int dock_applet_package_refresh(struct DockAppletPackage *current,
                                const struct DockAppletCatalog *catalog,
                                const char *id, int busy) {
  if (busy)
    return 0;
  struct DockAppletPackage candidate;
  memset(&candidate, 0, sizeof candidate);
  const struct DockAppletPackage *found = dock_applet_find(catalog, id);
  if (found)
    candidate = *found;
  else {
    snprintf(candidate.id, sizeof candidate.id, "%s", id);
    snprintf(candidate.name, sizeof candidate.name, "%s", id);
    strcpy(candidate.reason, "Package is not installed");
  }
  if (!memcmp(current, &candidate, sizeof candidate))
    return 0;
  *current = candidate;
  return 1;
}
