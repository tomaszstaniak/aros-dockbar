#include "cache.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
int dock_applet_cache_write(FILE *file, const char *settings,
                            const struct DockAppletSnapshot *s) {
  struct DockJson *j = malloc(sizeof *j);
  if (!j)
    return 0;
  int valid = dock_json_parse(j, settings, strlen(settings)) &&
              j->token[0].type == DJ_OBJECT;
  free(j);
  if (!valid)
    return 0;
  return fprintf(file, "{\"settings\":%s,\"snapshot\":", settings) > 0 &&
         dock_applet_snapshot_write(file, s) && fputs("}\n", file) >= 0 &&
         !ferror(file);
}
int dock_applet_cache_parse(const char *text, size_t len, const char *settings,
                            struct DockAppletSnapshot *out) {
  struct DockJson *j = malloc(sizeof *j);
  int ok = 0;
  if (!j)
    return 0;
  if (len > DA_RESPONSE_LIMIT + 65536 || !dock_json_parse(j, text, len))
    goto done;
  int cfg = dock_json_get(j, 0, "settings"),
      snapshot = dock_json_get(j, 0, "snapshot");
  if (cfg < 0 || snapshot < 0 || j->token[cfg].type != DJ_OBJECT ||
      j->token[snapshot].type != DJ_OBJECT)
    goto done;
  size_t n = (size_t)(j->token[cfg].end - j->token[cfg].start);
  if (n != strlen(settings) || memcmp(text + j->token[cfg].start, settings, n))
    goto done;
  ok = dock_applet_snapshot_parse(
      text + j->token[snapshot].start,
      (size_t)(j->token[snapshot].end - j->token[snapshot].start), out);
done:
  free(j);
  return ok;
}
