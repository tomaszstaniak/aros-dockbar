#include "purge.h"
#include <stdio.h>
#include <string.h>

/* Every name the runtime writes in an instance drawer (worker_aros.c:
 * settings and the last good answer, each with save_text's .new and .old). */
static const char *const known[] = {
  "settings.json", "settings.json.new", "settings.json.old",
  "last-good.json", "last-good.json.new", "last-good.json.old",
};

static int present(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  fclose(f);
  return 1;
}

int dock_applet_purge_dir(const char *dir, char *error, size_t cap) {
  size_t n = strlen(dir);
  if (error && cap)
    error[0] = 0;
  if (!n)
    return 0;
  for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) {
    char path[1100];
    int w = snprintf(path, sizeof path, "%s%s%s", dir, dir[n - 1] == ':' || dir[n - 1] == '/' ? "" : "/", known[i]);
    if (w < 0 || (size_t)w >= sizeof path)
      return 0;
    /* A missing file is the usual case, and mainline leaves errno 0 for it:
     * judge by whether the file is still there. */
    if (remove(path) && present(path)) {
      if (error)
        snprintf(error, cap, "could not delete %s", known[i]);
      return 0;
    }
  }
  if (remove(dir)) {
    if (error)
      snprintf(error, cap, "drawer kept (not empty or in use)");
    return 0;
  }
  return 1;
}
