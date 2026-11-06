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
  /* A host that also speaks API 2 keeps loading API 1 packages and asks
   * them in API 1. */
  t.api = 2;
  assert(dock_applet_scan(argv[1], &t, c) && c->packages[0].available &&
         c->packages[0].api == 1 && !c->packages[0].manual);
  t.api = 1;
  {
    static const char head[] =
        "{\"schema\":1,\"id\":\"org.test.sample\",\"name\":\"Sample\","
        "\"version\":\"0.1.0\",\"description\":\"Example\",\"license\":"
        "\"MIT\",\"hosts\":[\"dockbar\"],\"settings\":\"settings.json\","
        "\"views\":\"views.json\",\"entryPoints\":{\"test\":\"provider\"},";
    static const struct {
      const char *tail;
      int api, available, manual, actions;
    } cases[] = {
        /* Too new for this host, then accepted by an API 2 host. */
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"area\",\"label\":\"Area\"},{\"id\":\"screen\","
         "\"label\":\"Screen\"}]}",
         1, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"area\",\"label\":\"Area\"},{\"id\":\"screen\","
         "\"label\":\"Screen\"}]}",
         2, 1, 1, 2},
        /* Speaks 1 to 3: asked in 2 here. */
        {"\"api\":{\"min\":1,\"max\":3}}", 2, 1, 0, 0},
        {"\"api\":{\"min\":3,\"max\":3}}", 2, 0, 0, 0},
        /* Manual refresh needs actions; actions need API 2. */
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\"}", 2, 0, 0, 0},
        {"\"api\":{\"min\":1,\"max\":1},\"actions\":[{\"id\":\"a\","
         "\"label\":\"A\"}]}",
         2, 0, 0, 0},
        {"\"api\":{\"min\":1,\"max\":1},\"refresh\":\"none\"}", 2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"sometimes\"}", 2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"schedule\"}", 2, 1, 0, 0},
        /* Bad action lists. */
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":[]}", 2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"a/b\",\"label\":\"A\"}]}",
         2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"a\",\"label\":\"\"}]}",
         2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"a\",\"label\":\"A\"},{\"id\":\"a\",\"label\":\"B\"}]}",
         2, 0, 0, 0},
        {"\"api\":{\"min\":2,\"max\":2},\"refresh\":\"none\",\"actions\":"
         "[{\"id\":\"a1\",\"label\":\"A\"},{\"id\":\"a2\",\"label\":\"A\"},"
         "{\"id\":\"a3\",\"label\":\"A\"},{\"id\":\"a4\",\"label\":\"A\"},"
         "{\"id\":\"a5\",\"label\":\"A\"},{\"id\":\"a6\",\"label\":\"A\"},"
         "{\"id\":\"a7\",\"label\":\"A\"},{\"id\":\"a8\",\"label\":\"A\"},"
         "{\"id\":\"a9\",\"label\":\"A\"}]}",
         2, 0, 0, 0},
    };
    char saved[4096], next[4096];
    snprintf(q, sizeof q, "%s/applet.json", p);
    FILE *f = fopen(q, "r");
    assert(f);
    size_t got = fread(saved, 1, sizeof saved - 1, f);
    saved[got] = 0;
    fclose(f);
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
      snprintf(next, sizeof next, "%s%s", head, cases[i].tail);
      put(p, "applet.json", next);
      t.api = cases[i].api;
      assert(dock_applet_scan(argv[1], &t, c) && c->count == 1);
      const struct DockAppletPackage *k = &c->packages[0];
      if (k->available != cases[i].available ||
          (k->available && (k->manual != cases[i].manual ||
                            k->action_count != cases[i].actions))) {
        fprintf(stderr, "case %u: available=%d manual=%d actions=%d (%s)\n",
                (unsigned)i, k->available, k->manual, k->action_count, k->reason);
        return 1;
      }
      if (i == 0)
        assert(!strcmp(k->reason, "Unsupported widget API"));
      if (i == 1)
        assert(k->api == 2 && !strcmp(k->action_id[1], "screen") &&
               !strcmp(k->action_label[0], "Area"));
    }
    put(p, "applet.json", saved);
    t.api = 1;
  }
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
