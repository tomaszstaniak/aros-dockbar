#include "settings.h"
#include "json.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int str(const struct DockJson *j, int o, const char *k, char *s,
               size_t cap) {
  return dock_json_string(j, dock_json_get(j, o, k), s, cap);
}
int dock_applet_settings_schema(const char *path, struct DockAppletForm *out,
                                char *err, size_t cap) {
  size_t len;
  char *b = dock_applet_read(path, 65536, &len);
  struct DockJson *j = malloc(sizeof *j);
  struct DockAppletForm f;
  memset(&f, 0, sizeof f);
  int ok = 0;
  snprintf(err, cap, "Invalid settings schema");
  int schema;
  if (!b || !j || !dock_json_parse(j, b, len) ||
      !dock_json_int(j, dock_json_get(j, 0, "schema"), &schema) || schema != 1)
    goto done;
  int a = dock_json_get(j, 0, "fields");
  if (a < 0 || j->token[a].type != DJ_ARRAY ||
      dock_json_count(j, a) > DA_FORM_FIELDS)
    goto done;
  f.count = dock_json_count(j, a);
  for (int i = 0; i < f.count; i++) {
    int t = dock_json_at(j, a, i);
    struct DockAppletSetting *v = &f.fields[i];
    v->max_length = 159;
    v->min = -1e9;
    v->max = 1e9;
    if (!str(j, t, "key", v->key, sizeof v->key) ||
        !str(j, t, "label", v->label, sizeof v->label) ||
        !str(j, t, "type", v->type, sizeof v->type))
      goto done;
    if (strcmp(v->type, "text") && strcmp(v->type, "number") &&
        strcmp(v->type, "integer") && strcmp(v->type, "choice"))
      goto done;
    int length_token = dock_json_get(j, t, "maxLength");
    if (length_token >= 0 && (!dock_json_int(j, length_token, &v->max_length) ||
                              v->max_length < 1 || v->max_length > 159))
      goto done;
    int def = dock_json_get(j, t, "default");
    if (def >= 0) {
      if (j->token[def].type == DJ_STRING) {
        if (!dock_json_string(j, def, v->value, sizeof v->value))
          goto done;
      } else {
        double d;
        if (!dock_json_number(j, def, &d))
          goto done;
        snprintf(v->value, sizeof v->value, "%.8g", d);
      }
    }
    int mi = dock_json_get(j, t, "min"), ma = dock_json_get(j, t, "max");
    if (mi >= 0 && !dock_json_number(j, mi, &v->min))
      goto done;
    if (ma >= 0 && !dock_json_number(j, ma, &v->max))
      goto done;
    if (v->min > v->max)
      goto done;
    int ch = dock_json_get(j, t, "choices");
    v->choice_count = dock_json_count(j, ch);
    if (v->choice_count > DA_CHOICES ||
        (!strcmp(v->type, "choice") && !v->choice_count))
      goto done;
    for (int k = 0; k < v->choice_count; k++)
      if (!dock_json_string(j, dock_json_at(j, ch, k), v->choices[k],
                            sizeof v->choices[k]))
        goto done;
    for (int k = 0; k < i; k++)
      if (!strcmp(v->key, f.fields[k].key))
        goto done;
  }
  int search = dock_json_get(j, 0, "search");
  if (search >= 0) {
    if (!str(j, search, "operation", f.search, sizeof f.search) ||
        !str(j, search, "label", f.search_label, sizeof f.search_label))
      goto done;
    int map = dock_json_get(j, search, "map"),
        coords = dock_json_get(j, map, "coordinates");
    if (!str(j, map, "label", f.name_key, sizeof f.name_key) ||
        !dock_json_string(j, dock_json_at(j, coords, 0), f.latitude_key,
                          sizeof f.latitude_key) ||
        !dock_json_string(j, dock_json_at(j, coords, 1), f.longitude_key,
                          sizeof f.longitude_key))
      goto done;
  }
  *out = f;
  err[0] = 0;
  ok = 1;
done:
  free(b);
  free(j);
  return ok;
}
int dock_applet_settings_load(const char *path, struct DockAppletForm *form) {
  size_t len;
  char *b = dock_applet_read(path, 65536, &len);
  struct DockJson *j = malloc(sizeof *j);
  int ok = 0;
  struct DockAppletForm f = *form;
  int schema;
  if (!b || !j || !dock_json_parse(j, b, len) ||
      !dock_json_int(j, dock_json_get(j, 0, "schema"), &schema) || schema != 1)
    goto done;
  for (int i = 0; i < f.count; i++) {
    struct DockAppletSetting *v = &f.fields[i];
    int t = dock_json_get(j, 0, v->key);
    if (t < 0)
      continue;
    if (j->token[t].type == DJ_STRING) {
      if (!dock_json_string(j, t, v->value, sizeof v->value))
        goto done;
    } else {
      double n;
      if (!dock_json_number(j, t, &n))
        goto done;
      snprintf(v->value, sizeof v->value, "%.10g", n);
    }
  }
  *form = f;
  ok = 1;
done:
  free(b);
  free(j);
  return ok;
}
static int encode(const struct DockAppletForm *f, char *out, size_t cap,
                  char *err, size_t errcap);
/* On failure out is left empty: callers keep it as the instance's settings
 * and send it to the provider, and a half-written object (no closing brace)
 * made every request of an unconfigured instance invalid JSON, so a city
 * search never got an answer. */
int dock_applet_settings_encode(const struct DockAppletForm *f, char *out,
                                size_t cap, char *err, size_t errcap) {
  char *text = cap ? malloc(cap) : NULL;
  int ok = text && encode(f, text, cap, err, errcap);
  if (ok)
    memcpy(out, text, strlen(text) + 1);
  else if (cap)
    out[0] = 0;
  free(text);
  return ok;
}
static int encode(const struct DockAppletForm *f, char *out, size_t cap,
                  char *err, size_t errcap) {
  size_t used = 0;
  if (cap < 16)
    return 0;
  strcpy(out, "{\"schema\":1");
  used = strlen(out);
  for (int i = 0; i < f->count; i++) {
    const struct DockAppletSetting *v = &f->fields[i];
    char key[100], val[512];
    if (!dock_json_quote(v->key, key, sizeof key))
      return 0;
    if (!strcmp(v->type, "number") || !strcmp(v->type, "integer")) {
      char *end;
      double n = strtod(v->value, &end);
      if (!v->value[0] || *end || !isfinite(n) || n < v->min || n > v->max ||
          (!strcmp(v->type, "integer") && floor(n) != n)) {
        snprintf(err, errcap, "%s must be between %.8g and %.8g", v->label,
                 v->min, v->max);
        return 0;
      }
      snprintf(val, sizeof val, "%.10g", n);
    } else {
      if (strlen(v->value) > (size_t)v->max_length) {
        snprintf(err, errcap, "%s is too long (maximum %d bytes)", v->label,
                 v->max_length);
        return 0;
      }
      if (!v->value[0]) {
        snprintf(err, errcap, "Enter %s", v->label);
        return 0;
      }
      if (v->choice_count) {
        int found = 0;
        for (int k = 0; k < v->choice_count; k++)
          if (!strcmp(v->value, v->choices[k]))
            found = 1;
        if (!found) {
          snprintf(err, errcap, "Invalid %s", v->label);
          return 0;
        }
      }
      if (!dock_json_quote(v->value, val, sizeof val)) {
        snprintf(err, errcap, "Invalid text in %s", v->label);
        return 0;
      }
    }
    int n = snprintf(out + used, cap - used, ",%s:%s", key, val);
    if (n < 0 || (size_t)n >= cap - used)
      return 0;
    used += (size_t)n;
  }
  if (used + 2 > cap)
    return 0;
  strcpy(out + used, "}");
  err[0] = 0;
  return 1;
}

int dock_applet_settings_recover(const char *path, struct DockAppletForm *form,
                                 int *recovered) {
  *recovered = 0;
  struct DockAppletForm candidate = *form;
  if (dock_applet_settings_load(path, &candidate)) {
    char text[4096], error[160];
    if (!dock_applet_settings_encode(&candidate, text, sizeof text, error,
                                     sizeof error))
      return 0;
    *form = candidate;
    return 1;
  }
  FILE *primary = fopen(path, "rb");
  if (primary) {
    fclose(primary);
    return 0;
  }
  if (errno != ENOENT)
    return 0;
  char backup[1200];
  if (snprintf(backup, sizeof backup, "%s.old", path) >= (int)sizeof backup)
    return 0;
  candidate = *form;
  if (!dock_applet_settings_load(backup, &candidate))
    return 0;
  char text[4096], error[160];
  if (!dock_applet_settings_encode(&candidate, text, sizeof text, error,
                                   sizeof error))
    return 0;
  *form = candidate;
  *recovered = 1;
  return 1;
}
