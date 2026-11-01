#include "applets/purge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void put(const char *dir, const char *name) {
  char p[512];
  snprintf(p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen(p, "wb");
  assert(f);
  fputs("x", f);
  fclose(f);
}
static int there(const char *dir, const char *name) {
  char p[512];
  struct stat st;
  snprintf(p, sizeof p, "%s%s%s", dir, name[0] ? "/" : "", name);
  return stat(p, &st) == 0;
}

int main(int argc, char **argv) {
  char a[400], b[400], c[400], error[80];
  assert(argc == 2);
  snprintf(a, sizeof a, "%s/a", argv[1]);
  snprintf(b, sizeof b, "%s/b", argv[1]);
  snprintf(c, sizeof c, "%s/c", argv[1]);
  /* Known files only: all gone, the drawer too. */
  assert(!mkdir(a, 0700));
  put(a, "settings.json");
  put(a, "last-good.json");
  put(a, "last-good.json.old");
  assert(dock_applet_purge_dir(a, error, sizeof error) == 1);
  assert(!there(a, ""));
  /* An unknown file stays, and so does the drawer; known files go. */
  assert(!mkdir(b, 0700));
  put(b, "settings.json");
  put(b, "notes.txt");
  assert(dock_applet_purge_dir(b, error, sizeof error) == 0);
  assert(strstr(error, "drawer kept"));
  assert(there(b, "notes.txt") && !there(b, "settings.json"));
  /* An empty drawer (instance never configured) goes. */
  assert(!mkdir(c, 0700));
  assert(dock_applet_purge_dir(c, error, sizeof error) == 1);
  /* A drawer already gone is reported, not invented. */
  assert(dock_applet_purge_dir(c, error, sizeof error) == 0);
  puts("applet purge: passed");
  return 0;
}
