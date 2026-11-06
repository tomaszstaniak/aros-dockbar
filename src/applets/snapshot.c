#include "snapshot.h"
#include "json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int dock_applet_id_valid(const char *s) {
  if (!s || !*s || strlen(s) >= DA_ID || !strcmp(s, ".") || !strcmp(s, ".."))
    return 0;
  for (; *s; s++)
    if (!isalnum((unsigned char)*s) && *s != '.' && *s != '-' && *s != '_')
      return 0;
  return 1;
}
static int str(const struct DockJson *j, int obj, const char *key, char *out,
               size_t cap, int required) {
  int t = dock_json_get(j, obj, key);
  if (t < 0)
    return !required;
  return dock_json_string(j, t, out, cap);
}
int dock_applet_snapshot_parse(const char *text, size_t len,
                               struct DockAppletSnapshot *out) {
  int ok = 0, api, g;
  struct DockJson *j = malloc(sizeof *j);
  struct DockAppletSnapshot *s = calloc(1, sizeof *s);
  if (!j || !s)
    goto done;
  if (len > DA_RESPONSE_LIMIT || !dock_json_parse(j, text, len) ||
      !dock_json_int(j, dock_json_get(j, 0, "api"), &api) ||
      api < DA_API_MIN || api > DA_API_MAX)
    goto done;
  if (!str(j, 0, "package", s->package, sizeof s->package, 1) ||
      !str(j, 0, "instance", s->instance, sizeof s->instance, 1) ||
      !str(j, 0, "session", s->session, sizeof s->session, 1) ||
      !dock_applet_id_valid(s->package) || !dock_applet_id_valid(s->instance) ||
      !dock_applet_id_valid(s->session))
    goto done;
  if (!dock_json_int(j, dock_json_get(j, 0, "generation"), &g) || g < 1)
    goto done;
  s->generation = (unsigned long)g;
  int st = dock_json_get(j, 0, "status");
  if (dock_json_is(j, st, "ready"))
    s->status = DA_READY;
  else if (dock_json_is(j, st, "stale"))
    s->status = DA_STALE;
  else if (dock_json_is(j, st, "error"))
    s->status = DA_ERROR;
  else if (dock_json_is(j, st, "loading"))
    s->status = DA_LOADING;
  else
    goto done;
  if (!dock_json_number(j, dock_json_get(j, 0, "updated"), &s->updated) ||
      s->updated < 0 ||
      !dock_json_number(j, dock_json_get(j, 0, "sourceTime"),
                        &s->source_time) ||
      s->source_time < 0)
    goto done;
  if (!str(j, 0, "title", s->title, sizeof s->title, 1) ||
      !str(j, 0, "icon", s->icon, sizeof s->icon, 0) ||
      !str(j, 0, "fingerprint", s->fingerprint, sizeof s->fingerprint, 1) ||
      !str(j, 0, "message", s->message, sizeof s->message, 0) ||
      !str(j, 0, "attribution", s->attribution, sizeof s->attribution, 0) ||
      !str(j, 0, "url", s->url, sizeof s->url, 0))
    goto done;
  int fields = dock_json_get(j, 0, "fields");
  if (fields < 0 || j->token[fields].type != DJ_OBJECT ||
      j->token[fields].count > DA_FIELDS)
    goto done;
  for (int k = fields + 1; k < j->token[fields].next;) {
    struct DockAppletField *f = &s->fields[s->field_count++];
    int obj = j->token[k].next, v = dock_json_get(j, obj, "value");
    char unit[32] = "";
    if (!dock_json_string(j, k, f->key, sizeof f->key) || v < 0 ||
        !str(j, obj, "unit", unit, sizeof unit, 0))
      goto done;
    if (j->token[v].type == DJ_NULL)
      strcpy(f->text, "--");
    else if (j->token[v].type == DJ_STRING) {
      if (!dock_json_string(j, v, f->text, sizeof f->text))
        goto done;
    } else {
      if (!dock_json_number(j, v, &f->number) || fabs(f->number) > 1e9)
        goto done;
      f->numeric = 1;
      strcpy(f->unit, unit);
      snprintf(f->text, sizeof f->text, "%.0f%s%s", round(f->number),
               unit[0] ? " " : "", unit);
    }
    k = j->token[obj].next;
  }
  int rows = dock_json_get(j, 0, "rows");
  if (rows >= 0) {
    if (j->token[rows].type != DJ_ARRAY || j->token[rows].count > DA_ROWS)
      goto done;
    s->row_count = j->token[rows].count;
    for (int i = 0; i < s->row_count; i++) {
      int r = dock_json_at(j, rows, i);
      struct DockAppletRow *row = &s->rows[i];
      if (!str(j, r, "label", row->label, sizeof row->label, 1) ||
          !str(j, r, "value", row->value, sizeof row->value, 1) ||
          !str(j, r, "icon", row->icon, sizeof row->icon, 0) ||
          !str(j,r,"section",row->section,sizeof row->section,0) ||
          !str(j,r,"caption",row->caption,sizeof row->caption,0) ||
          !str(j,r,"detail",row->detail,sizeof row->detail,0))
        goto done;
      int v = dock_json_get(j, r, "plot");
      if (v >= 0 && j->token[v].type != DJ_NULL) {
        if (!dock_json_number(j, v, &row->plot) || fabs(row->plot) > 1e9)
          goto done;
        row->has_plot = 1;
      }
    }
  }
  *out = *s;
  ok = 1;
done:
  free(j);
  free(s);
  return ok;
}
const char *dock_applet_field(const struct DockAppletSnapshot *s,
                              const char *key) {
  for (int i = 0; s && i < s->field_count; i++)
    if (!strcmp(s->fields[i].key, key))
      return s->fields[i].text;
  return "--";
}

static int quoted(FILE *f, const char *s) {
  char b[2048];
  return dock_json_quote(s, b, sizeof b) && fputs(b, f) >= 0;
}
static int prop(FILE *f, const char *key, const char *value) {
  return quoted(f, key) && fputc(':', f) != EOF && quoted(f, value);
}
int dock_applet_snapshot_write(FILE *f, const struct DockAppletSnapshot *s) {
  if (fputs("{\"api\":1,", f) < 0 || !prop(f, "package", s->package) ||
      fputc(',', f) == EOF || !prop(f, "instance", s->instance) ||
      fputc(',', f) == EOF || !prop(f, "session", s->session))
    return 0;
  if (fprintf(f, ",\"generation\":%lu,\"updated\":%.0f,\"sourceTime\":%.0f,",
              s->generation, s->updated, s->source_time) < 0)
    return 0;
  const char *status = s->status == DA_READY   ? "ready"
                       : s->status == DA_STALE ? "stale"
                       : s->status == DA_ERROR ? "error"
                                               : "loading";
  if (!prop(f, "status", status) || fputc(',', f) == EOF ||
      !prop(f, "fingerprint", s->fingerprint) || fputc(',', f) == EOF ||
      !prop(f, "title", s->title) || fputc(',', f) == EOF ||
      !prop(f, "icon", s->icon) || fputc(',', f) == EOF ||
      !prop(f, "message", s->message) || fputc(',', f) == EOF ||
      !prop(f, "attribution", s->attribution) || fputc(',', f) == EOF ||
      !prop(f, "url", s->url) || fputs(",\"fields\":{", f) < 0)
    return 0;
  for (int i = 0; i < s->field_count; i++) {
    const struct DockAppletField *v = &s->fields[i];
    if ((i && fputc(',', f) == EOF) || !quoted(f, v->key) ||
        fputs(":{\"value\":", f) < 0)
      return 0;
    if (v->numeric) {
      if (fprintf(f, "%.8g,", v->number) < 0 || !prop(f, "unit", v->unit))
        return 0;
    } else if (!quoted(f, v->text))
      return 0;
    if (fputc('}', f) == EOF)
      return 0;
  }
  if (fputs("},\"rows\":[", f) < 0)
    return 0;
  for (int i = 0; i < s->row_count; i++) {
    const struct DockAppletRow *r = &s->rows[i];
    if ((i && fputc(',', f) == EOF) || fputc('{', f) == EOF ||
        !prop(f, "label", r->label) || fputc(',', f) == EOF ||
        !prop(f, "value", r->value) || fputc(',', f) == EOF ||
        !prop(f, "icon", r->icon) || fputc(',',f)==EOF ||
        !prop(f,"section",r->section) || fputc(',',f)==EOF ||
        !prop(f,"caption",r->caption) || fputc(',',f)==EOF ||
        !prop(f,"detail",r->detail))
      return 0;
    if (r->has_plot && fprintf(f, ",\"plot\":%.8g", r->plot) < 0)
      return 0;
    if (fputc('}', f) == EOF)
      return 0;
  }
  return fputs("]}\n", f) >= 0 && !ferror(f);
}
