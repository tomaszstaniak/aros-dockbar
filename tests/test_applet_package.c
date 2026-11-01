#define _POSIX_C_SOURCE 200809L
#include "applets/package.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void put(const char *dir, const char *name, const char *text) {
  char p[2048];
  snprintf(p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen(p, "w");
  assert(f);
  fputs(text, f);
  fclose(f);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  char p[1024], q[1024];
  snprintf(p, sizeof p, "%s/renamed", argv[1]);
  assert(!mkdir(p, 0700));
  put(p, "applet.json",
      "{\"schema\":1,\"id\":\"org.test.sample\",\"name\":\"Sample\","
      "\"version\":\"0.1.0\",\"description\":\"Example\",\"author\":\"AROS\","
      "\"license\":\"MIT\",\"api\":{\"min\":1,\"max\":1},\"hosts\":["
      "\"dockbar\"],\"capabilities\":[],\"settings\":\"settings.json\","
      "\"views\":\"views.json\",\"entryPoints\":{\"test\":\"provider\"}}");
  put(p, "settings.json", "{\"schema\":1,\"fields\":[]}");
  put(p, "views.json",
      "{\"schema\":1,\"compact\":{\"primary\":\"value\",\"secondary\":\"name\"}"
      ",\"expanded\":{\"component\":\"rows\"}}");
  put(p, "LICENSE", "MIT");
  put(p, "provider", "must never execute");
  struct DockAppletCatalog *c = calloc(1, sizeof *c);
  struct DockAppletTarget t = {"dockbar", "test", 1};
  assert(dock_applet_scan(argv[1], &t, c) && c->count == 1 &&
         c->packages[0].available);
  assert(!strcmp(c->packages[0].id, "org.test.sample"));
  struct DockAppletPackage cached = {0};
  strcpy(cached.id, "org.test.sample");
  assert(!dock_applet_package_refresh(&cached, c, "org.test.sample", 1) &&
         !cached.available);
  assert(dock_applet_package_refresh(&cached, c, "org.test.sample", 0) &&
         cached.available);
  put(p,"views.json","{\"schema\":1,\"compact\":{\"primary\":\"value\",\"secondary\":\"name\"},\"expanded\":{\"component\":\"cards\",\"title\":\"name\",\"value\":\"value\"}}");
  assert(dock_applet_scan(argv[1],&t,c)&&c->packages[0].available);
  t.variant = "other";
  assert(dock_applet_scan(argv[1], &t, c) && !c->packages[0].available);
  t.variant = "test";
  t.api = 2;
  assert(dock_applet_scan(argv[1], &t, c) && !c->packages[0].available);
  t.api = 1;
  snprintf(q, sizeof q, "%s/provider", p);
  assert(!unlink(q));
  assert(dock_applet_scan(argv[1], &t, c) && !c->packages[0].available);
  assert(!symlink("/bin/sh", q));
  assert(dock_applet_scan(argv[1], &t, c) && !c->packages[0].available);
  assert(!dock_applet_relative("../x") && !dock_applet_relative("SYS:x") &&
         !dock_applet_relative("/x") && !dock_applet_relative("a/../b") &&
         dock_applet_relative("bin/test/provider"));
  assert(!unlink(q));
  put(p, "provider", "not executed");
  char duplicate[1024];
  snprintf(duplicate, sizeof duplicate, "%s/duplicate", argv[1]);
  assert(!mkdir(duplicate, 0700));
  char manifest[1200];
  snprintf(manifest, sizeof manifest, "%s/applet.json", p);
  FILE *in = fopen(manifest, "r");
  assert(in);
  char body[4096];
  size_t bytes = fread(body, 1, sizeof body - 1, in);
  body[bytes] = 0;
  fclose(in);
  put(duplicate, "applet.json", body);
  assert(dock_applet_scan(argv[1], &t, c) && c->count == 2);
  assert(!c->packages[0].available && !c->packages[1].available);
  assert(strstr(c->packages[0].reason, "Duplicate"));
  for (int k = 0; k < 64; k++) {
    char extra[1024];
    snprintf(extra, sizeof extra, "%s/extra-%d", argv[1], k);
    assert(!mkdir(extra, 0700));
  }
  assert(dock_applet_scan(argv[1], &t, c) && c->count == 64 && c->overflow);
  puts("applet package: passed");
  free(c);
  return 0;
}
