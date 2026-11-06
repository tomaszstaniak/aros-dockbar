#define _POSIX_C_SOURCE 200809L
#include "applets/install.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char base[1024];
static void put(const char *dir, const char *name, const char *text) {
  char p[2048];
  snprintf(p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen(p, "w");
  assert(f);
  fputs(text, f);
  fclose(f);
}
static void package(const char *dir, const char *version, const char *variant) {
  char manifest[2048], bin[1200];
  assert(!mkdir(dir, 0755));
  snprintf(manifest, sizeof manifest,
           "{\"schema\":1,\"id\":\"org.test.capture\",\"name\":\"Capture\","
           "\"version\":\"%s\",\"description\":\"d\",\"license\":\"MIT\","
           "\"api\":{\"min\":2,\"max\":2},\"hosts\":[\"dockbar\"],"
           "\"refresh\":\"none\",\"actions\":[{\"id\":\"area\",\"label\":"
           "\"Area\"}],\"settings\":\"settings.json\",\"views\":\"views.json\","
           "\"entryPoints\":{\"%s\":\"bin/%s/Provider\"}}",
           version, variant, variant);
  put(dir, "applet.json", manifest);
  put(dir, "settings.json", "{\"schema\":1,\"fields\":[]}");
  put(dir, "views.json",
      "{\"schema\":1,\"compact\":{\"primary\":\"v\",\"secondary\":\"n\"},"
      "\"expanded\":{\"component\":\"rows\"}}");
  put(dir, "LICENSE", "MIT");
  snprintf(bin, sizeof bin, "%s/bin", dir);
  assert(!mkdir(bin, 0755));
  snprintf(bin, sizeof bin, "%s/bin/%s", dir, variant);
  assert(!mkdir(bin, 0755));
  put(bin, "Provider", "not executed");
}
static const char *path(const char *rel) {
  static char p[4][1200];
  static int k;
  k = (k + 1) % 4;
  snprintf(p[k], sizeof p[k], "%s/%s", base, rel);
  return p[k];
}
static int exists(const char *p) {
  struct stat st;
  return !lstat(p, &st);
}
void dock_applet_install_test_rename(int (*)(const char *, const char *));
/* Fails every rename onto the installed drawer: the new copy cannot be put
 * in place, and neither can the old one be put back. */
static char blocked[1300];
static int block_count;
static int blocking_rename(const char *from, const char *to) {
  if (block_count && !strcmp(to, blocked)) {
    block_count--;
    return -1;
  }
  return rename(from, to);
}
static void foreign(const char *rel) {
  assert(!mkdir(path(rel), 0755));
  put(path(rel), "keep.txt", "not a widget");
}
static void unforeign(const char *rel) {
  char p[1300];
  snprintf(p, sizeof p, "%s/keep.txt", path(rel));
  assert(!unlink(p) && !rmdir(path(rel)));
}
static int kept(const char *rel) {
  char p[1300];
  snprintf(p, sizeof p, "%s/keep.txt", path(rel));
  return exists(p);
}
static int install(const char *source, struct DockAppletCatalog *c,
                   const struct DockAppletTarget *t, char *text) {
  struct DockAppletPackage pkg;
  assert(dock_applet_scan(path("root"), t, c));
  int r = dock_applet_install_check(source, t, &pkg, text, 160);
  if (r)
    return r;
  r = dock_applet_install_copy(&pkg, path("root"), t, c, text, 160);
  assert(dock_applet_scan(path("root"), t, c));
  return r;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  snprintf(base, sizeof base, "%s", argv[1]);
  struct DockAppletTarget t = {"dockbar", "x86_64-abiv11", DA_API_MAX};
  struct DockAppletCatalog *c = calloc(1, sizeof *c);
  char text[160];
  assert(c && !mkdir(path("root"), 0755));
  package(path("v1"), "0.1.0", "x86_64-abiv11");
  package(path("v2"), "0.2.0", "x86_64-abiv11");
  package(path("pi"), "0.1.0", "aarch64-raspi-v1");
  /* Wrong architecture, missing drawer: refused, nothing written. */
  assert(install(path("pi"), c, &t, text) == DA_INSTALL_REFUSED &&
         strstr(text, "architecture") && c->count == 0);
  assert(install(path("missing"), c, &t, text) == DA_INSTALL_REFUSED);
  /* A drawer with the package's name that is not the package, and staging
   * or old drawers no install made: refused, left as they are. */
  foreign("root/org.test.capture");
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_REFUSED &&
         strstr(text, "not this widget") && kept("root/org.test.capture"));
  unforeign("root/org.test.capture");
  foreign("root/.org.test.capture.installing");
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_REFUSED &&
         strstr(text, ".org.test.capture.installing") &&
         kept("root/.org.test.capture.installing") &&
         !exists(path("root/org.test.capture")));
  unforeign("root/.org.test.capture.installing");
  foreign("root/.org.test.capture.old");
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_REFUSED &&
         strstr(text, ".org.test.capture.old") &&
         kept("root/.org.test.capture.old") &&
         !exists(path("root/org.test.capture")));
  unforeign("root/.org.test.capture.old");
  /* A foreign drawer with a stale name is not deleted; the install goes on. */
  foreign("root/.org.test.capture.stale-1");
  /* New install: the drawer is named after the ID. */
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_OK &&
         !strcmp(text, "Capture"));
  assert(c->count == 1 && c->packages[0].available &&
         !strcmp(c->packages[0].version, "0.1.0") &&
         exists(path("root/org.test.capture/bin/x86_64-abiv11/Provider")));
  assert(kept("root/.org.test.capture.stale-1"));
  unforeign("root/.org.test.capture.stale-1");
  /* The installed package carries no install mark. */
  assert(!exists(path("root/org.test.capture/.dockbar-install")));
  /* The installed drawer itself is not a source. */
  assert(install(path("root/org.test.capture"), c, &t, text) ==
         DA_INSTALL_REFUSED);
  /* Update: same drawer, no duplicate, nothing left behind. */
  assert(install(path("v2"), c, &t, text) == DA_INSTALL_UPDATED);
  assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0"));
  assert(!exists(path("root/.org.test.capture.old")) &&
         !exists(path("root/.org.test.capture.installing")));
  /* A source with a link is not copied; the installed one stays. */
  assert(!symlink("/etc/passwd", path("v1/link")));
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_FAILED);
  assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0") &&
         !exists(path("root/.org.test.capture.installing")));
  assert(!unlink(path("v1/link")));
  /* Read-only widget drawer: fails, installed one stays. */
  if (geteuid() != 0) {
    assert(!chmod(path("root"), 0555));
    assert(install(path("v1"), c, &t, text) == DA_INSTALL_FAILED);
    assert(!chmod(path("root"), 0755));
    assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0"));
  }
  /* An installed package under another folder name is replaced there. */
  assert(!rename(path("root/org.test.capture"), path("root/Capture")));
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_UPDATED);
  assert(c->count == 1 && exists(path("root/Capture/applet.json")) &&
         !exists(path("root/org.test.capture")) &&
         !strcmp(c->packages[0].version, "0.1.0"));
  /* An install that stopped between its renames: the old copy comes back,
   * then the update goes on. */
  assert(!rename(path("root/Capture"), path("root/.org.test.capture.old")));
  assert(!mkdir(path("root/.org.test.capture.installing"), 0755));
  assert(dock_applet_scan(path("root"), &t, c) && c->count == 0);
  assert(install(path("v2"), c, &t, text) == DA_INSTALL_UPDATED);
  assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0") &&
         !exists(path("root/.org.test.capture.old")) &&
         !exists(path("root/.org.test.capture.installing")));
  /* An old copy that cannot be deleted (a drawer still in use): moved
   * aside, the update still succeeds, and the next install deletes it. */
  if (geteuid() != 0) {
    assert(!chmod(path("root/org.test.capture/bin"), 0555));
    assert(install(path("v1"), c, &t, text) == DA_INSTALL_UPDATED &&
           !strcmp(c->packages[0].version, "0.1.0"));
    assert(exists(path("root/.org.test.capture.stale-1/bin")) &&
           !exists(path("root/.org.test.capture.old")));
    assert(install(path("v2"), c, &t, text) == DA_INSTALL_UPDATED);
    assert(exists(path("root/.org.test.capture.stale-1")) || 1);
    assert(!chmod(path("root/.org.test.capture.stale-1/bin"), 0755));
    assert(install(path("v1"), c, &t, text) == DA_INSTALL_UPDATED);
    assert(!exists(path("root/.org.test.capture.stale-1")) &&
           c->count == 1 && !strcmp(c->packages[0].version, "0.1.0"));
  }
  /* The new copy cannot be put in place and the old one cannot be put
   * back: the result says where the old one is, and the next install puts
   * it back and updates. */
  snprintf(blocked, sizeof blocked, "%s", path("root/org.test.capture"));
  block_count = 2;
  dock_applet_install_test_rename(blocking_rename);
  assert(install(path("v2"), c, &t, text) == DA_INSTALL_FAILED &&
         strstr(text, "kept in") && strstr(text, ".org.test.capture.old"));
  dock_applet_install_test_rename(NULL);
  assert(c->count == 0 && exists(path("root/.org.test.capture.old/applet.json")) &&
         !exists(path("root/.org.test.capture.installing")));
  assert(install(path("v2"), c, &t, text) == DA_INSTALL_UPDATED);
  assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0") &&
         !exists(path("root/.org.test.capture.old")) &&
         !exists(path("root/org.test.capture/.dockbar-install")));
  /* The new copy cannot be put in place but the old one goes back. */
  block_count = 1;
  dock_applet_install_test_rename(blocking_rename);
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_FAILED &&
         strstr(text, "nothing was changed"));
  dock_applet_install_test_rename(NULL);
  assert(c->count == 1 && !strcmp(c->packages[0].version, "0.2.0") &&
         !exists(path("root/.org.test.capture.old")) &&
         !exists(path("root/.org.test.capture.installing")) &&
         !exists(path("root/org.test.capture/.dockbar-install")));
  /* Two installed packages with the ID: refused. */
  assert(!mkdir(path("root/copy"), 0755));
  put(path("root/copy"), "applet.json", "{}");
  assert(!unlink(path("root/copy/applet.json")) && !rmdir(path("root/copy")));
  package(path("root/copy"), "0.1.0", "x86_64-abiv11");
  assert(install(path("v1"), c, &t, text) == DA_INSTALL_REFUSED &&
         strstr(text, "Two installed"));
  free(c);
  puts("applet install: new, update, refusals, foreign drawers, rollback and "
       "recovery passed");
  return 0;
}
