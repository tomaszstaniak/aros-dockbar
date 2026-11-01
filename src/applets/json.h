#ifndef DOCK_APPLET_JSON_H
#define DOCK_APPLET_JSON_H
#include <stddef.h>
/* Input is borrowed until the next parse. Tokens never contain pointers into
 * other processes. Bounds are shared by package and network decoders. */
#define DOCK_JSON_TOKENS 16384
#define DOCK_JSON_LIMIT (512u * 1024u)
enum DockJsonType {
  DJ_OBJECT,
  DJ_ARRAY,
  DJ_STRING,
  DJ_NUMBER,
  DJ_BOOL,
  DJ_NULL
};
struct DockJsonToken {
  int type, start, end, next, count;
};
struct DockJson {
  const char *text;
  size_t length;
  int count;
  struct DockJsonToken token[DOCK_JSON_TOKENS];
};
int dock_json_parse(struct DockJson *json, const char *text, size_t length);
int dock_json_get(const struct DockJson *json, int object, const char *key);
int dock_json_at(const struct DockJson *json, int array, int index);
int dock_json_count(const struct DockJson *json, int array);
int dock_json_string(const struct DockJson *json, int token, char *out,
                     size_t capacity);
int dock_json_number(const struct DockJson *json, int token, double *value);
int dock_json_int(const struct DockJson *json, int token, int *value);
int dock_json_is(const struct DockJson *json, int token, const char *value);
/* Allocated bounded file. Caller frees. Rejects embedded NULs. */
char *dock_applet_read(const char *path, size_t limit, size_t *length);
/* JSON string escaping, including quotes; 0 on insufficient capacity. */
int dock_json_quote(const char *text, char *out, size_t capacity);
#endif
