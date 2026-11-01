#include "worker_aros.h"
#include "purge.h"
#include "../command.h"
#include "cache.h"
#include "json.h"
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include "../elapsed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
static struct DockAppletCatalog catalog;
static struct DockAppletRuntime *instances;
static char packages_root[DA_PATH], state_root[DA_PATH], session_root[DA_PATH],
    session[DA_ID];
static unsigned long serial;
static struct MsgPort *clock_port;
static struct timerequest *clock_request;
static uint64_t clock_origin;
static uint32_t clock_frequency;
/* A request the provider did not answer in time is ended, not forgotten:
 * the provider process gets Ctrl-C and the slot stays taken until that
 * process is gone or its late answer arrives, so a retry never runs next to
 * it. The process is found by its name, unique per request. */
#define REQUEST_TIMEOUT_MS 60000u
struct Worker {
  struct DockAppletRuntime *owner;
  struct DockAppletRequest identity;
  char request[DA_PATH], name[112];
  uint64_t started;
  int searching, ending;
};
static struct Worker workers[4];
void (*dock_applets_note)(const char *, ...);
#define NOTE(...) do { if (dock_applets_note) dock_applets_note(__VA_ARGS__); } while (0)
static const char *variant(void) {
#ifdef DOCK_APPLET_VARIANT
  return DOCK_APPLET_VARIANT;
#else
  return "unsupported";
#endif
}
static int join(char *out, size_t cap, const char *root, const char *name) {
  size_t n = strlen(root);
  int w = snprintf(out, cap, "%s%s%s", root, n && root[n - 1] == ':' ? "" : "/",
                   name);
  return w >= 0 && (size_t)w < cap;
}
static int directory(const char *p) {
  BPTR lock = Lock((CONST_STRPTR)p, ACCESS_READ);
  if (lock) {
    UnLock(lock);
    return 1;
  }
  lock = CreateDir((CONST_STRPTR)p);
  if (!lock)
    return 0;
  UnLock(lock);
  return 1;
}
static int absolute(const char *p, char *out) {
  BPTR lock = Lock((CONST_STRPTR)p, ACCESS_READ);
  if (!lock)
    return 0;
  int ok = NameFromLock(lock, (STRPTR)out, DA_PATH);
  UnLock(lock);
  return ok;
}
void dock_applets_rescan(void) {
  struct DockAppletTarget target = {"dockbar", variant(), 1};
  dock_applet_scan(packages_root, &target, &catalog);
  for (struct DockAppletRuntime *r = instances; r; r = r->next)
    r->reconcile_pending = 1;
}
const struct DockAppletCatalog *dock_applets_catalog(void) { return &catalog; }
int dock_applets_init(const char *root, const char *state) {
  clock_port = CreateMsgPort();
  if (!clock_port)
    return 0;
  clock_request =
      (struct timerequest *)CreateIORequest(clock_port, sizeof *clock_request);
  if (!clock_request)
    return 0;
  if (OpenDevice((CONST_STRPTR) "timer.device", UNIT_MICROHZ,
                 (struct IORequest *)clock_request, 0)) {
    DeleteIORequest((struct IORequest *)clock_request);
    clock_request = NULL;
    return 0;
  }
  struct Device *TimerBase = clock_request->tr_node.io_Device;
  struct EClockVal ticks;
  clock_frequency = ReadEClock(&ticks);
  clock_origin = ((uint64_t)ticks.ev_hi << 32) | ticks.ev_lo;
  if (!clock_frequency)
    return 0;
  snprintf(packages_root, sizeof packages_root, "%s", root);
  if (!absolute(root, packages_root))
    snprintf(packages_root, sizeof packages_root, "%s", root);
  if (!directory(state) || !absolute(state, state_root))
    return 0;
  for (int n = 0; n < 100; n++) {
    snprintf(session, sizeof session, "s-%lx-%lx-%d", (unsigned long)time(NULL),
             (unsigned long)FindTask(NULL), n);
    char name[100];
    snprintf(name, sizeof name, ".%s", session);
    if (!join(session_root, sizeof session_root, state_root, name))
      return 0;
    BPTR l = CreateDir((CONST_STRPTR)session_root);
    if (l) {
      UnLock(l);
      dock_applets_rescan();
      return 1;
    }
  }
  return 0;
}
int dock_applets_new_id(char *out, size_t cap) {
  int n = snprintf(out, cap, "i-%lx-%lx", (unsigned long)time(NULL), ++serial);
  return n > 0 && (size_t)n < cap;
}
static void cache_load(struct DockAppletRuntime *r) {
  char path[DA_PATH];
  if (!join(path, sizeof path, r->directory, "last-good.json"))
    return;
  size_t n;
  char *b = dock_applet_read(path, DA_RESPONSE_LIMIT + 65536, &n);
  struct DockAppletSnapshot *s = malloc(sizeof *s);
  if (b && s && dock_applet_cache_parse(b, n, r->settings, s) &&
      !strcmp(s->package, r->state.package) &&
      !strcmp(s->instance, r->state.instance)) {
    r->state.snapshot = *s;
    r->state.snapshot.status = DA_STALE;
    r->state.has_data = 1;
  }
  free(s);
  free(b);
}
struct DockAppletRuntime *dock_applets_get(const char *id,
                                           const char *instance) {
  if (!state_root[0] || !dock_applet_id_valid(id) ||
      !dock_applet_id_valid(instance))
    return NULL;
  for (struct DockAppletRuntime *r = instances; r; r = r->next)
    if (!strcmp(r->state.package, id) && !strcmp(r->state.instance, instance)) {
      r->active = 1;
      return r;
    }
  struct DockAppletRuntime *r = calloc(1, sizeof *r);
  if (!r)
    return NULL;
  const struct DockAppletPackage *p = dock_applet_find(&catalog, id);
  if (p)
    r->package = *p;
  else {
    snprintf(r->package.id, sizeof r->package.id, "%s", id);
    strcpy(r->package.reason, "Package is not installed");
  }
  dock_applet_instance_init(&r->state, id, instance, session, 900);
  char parent[DA_PATH];
  if (!join(parent, sizeof parent, state_root, id) || !directory(parent) ||
      !join(r->directory, sizeof r->directory, parent, instance) ||
      !directory(r->directory) ||
      !join(r->settings_path, sizeof r->settings_path, r->directory,
            "settings.json")) {
    free(r);
    return NULL;
  }
  struct DockAppletForm form;
  char error[160];
  if (dock_applet_load_form(r, &form, error, sizeof error) &&
      dock_applet_settings_encode(&form, r->settings, sizeof r->settings, error,
                                  sizeof error)) {
    for (int fi = 0; fi < form.count; fi++)
      if (!strcmp(form.fields[fi].key, r->package.secondary))
        snprintf(r->caption, sizeof r->caption, "%s", form.fields[fi].value);
    r->configured = 1;
    for (int i = 0; i < form.count; i++)
      if (!strcmp(form.fields[i].key, "refreshMinutes"))
        r->state.refresh_ms =
            (unsigned long)strtoul(form.fields[i].value, NULL, 10) * 60000;
    cache_load(r);
  } else
    strcpy(r->state.snapshot.message, "Choose a location in Settings");
  r->active = r->changed = 1;
  r->next = instances;
  instances = r;
  return r;
}
void dock_applets_layout_begin(void) {
  for (struct DockAppletRuntime *r = instances; r; r = r->next)
    r->active = 0;
}
int dock_applet_load_form(struct DockAppletRuntime *r, struct DockAppletForm *f,
                          char *error, size_t cap) {
  char path[DA_PATH];
  if (!r->package.available) {
    snprintf(error, cap, "%s", r->package.reason);
    return 0;
  }
  if (!dock_applet_path(r->package.root, "settings.json", path, sizeof path) ||
      !dock_applet_settings_schema(path, f, error, cap))
    return 0;
  if (!dock_applet_settings_recover(r->settings_path, f, &r->recovered) &&
      !strncmp(r->state.instance, "copy-", 5)) {
    const char *original = strchr(r->state.instance + 5, '-');
    char parent[DA_PATH], source[DA_PATH], file[DA_PATH];
    if (original && dock_applet_id_valid(original + 1) &&
        join(parent, sizeof parent, state_root, r->state.package) &&
        join(source, sizeof source, parent, original + 1) &&
        join(file, sizeof file, source, "settings.json")) {
      if (dock_applet_settings_load(file, f)) {
        char data[4096];
        if (dock_applet_settings_encode(f, data, sizeof data, error, cap)) {
          FILE *copy = fopen(r->settings_path, "wb");
          if (copy) {
            fputs(data, copy);
            fclose(copy);
          }
        }
      }
    }
  }
  return 1;
}
/* Backup replacement matches Dockbar's existing DOS persistence convention.
 * Do not touch a good primary until the temporary has been closed. */
static int save_text(const char *path, const char *text) {
  char tmp[DA_PATH], old[DA_PATH];
  if (snprintf(tmp, sizeof tmp, "%s.new", path) >= (int)sizeof tmp ||
      snprintf(old, sizeof old, "%s.old", path) >= (int)sizeof old)
    return 0;
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return 0;
  int ok = fputs(text, f) >= 0;
  if (fclose(f))
    ok = 0;
  if (!ok) {
    remove(tmp);
    return 0;
  }
  BPTR l = Lock((CONST_STRPTR)path, ACCESS_READ);
  if (l) {
    UnLock(l);
    remove(old);
    if (rename(path, old)) {
      remove(tmp);
      return 0;
    }
  }
  if (rename(tmp, path)) {
    rename(old, path);
    remove(tmp);
    return 0;
  }
  return 1;
}
int dock_applet_save_settings(struct DockAppletRuntime *r,
                              const struct DockAppletForm *f, char *error,
                              size_t cap) {
  char text[4096];
  if (!dock_applet_settings_encode(f, text, sizeof text, error, cap))
    return 0;
  if (!save_text(r->settings_path, text)) {
    snprintf(error, cap, "Could not save settings; previous settings retained");
    return 0;
  }
  if (!strcmp(r->settings, text))
    return 1;
  strcpy(r->settings, text);
  for (int fi = 0; fi < f->count; fi++)
    if (!strcmp(f->fields[fi].key, r->package.secondary))
      snprintf(r->caption, sizeof r->caption, "%s", f->fields[fi].value);
  r->configured = 1;
  r->recovered = 0;
  dock_applet_instance_reset(&r->state, 0);
  r->state.refresh_ms = 900000;
  for (int i = 0; i < f->count; i++)
    if (!strcmp(f->fields[i].key, "refreshMinutes"))
      r->state.refresh_ms =
          (unsigned long)strtoul(f->fields[i].value, NULL, 10) * 60000;
  r->changed = 1;
  return 1;
}
static int launch(const char *program, const char *request, const char *name) {
  char command[DA_PATH * 2 + 16], line[DA_PATH * 4 + 32];
  /* Quote the executable with the same tested DOS escaping as its argument. */
  if (!dock_command_quoted("", program, command, sizeof command) ||
      !dock_command_quoted(command, request, line, sizeof line))
    return 0;
  BPTR in = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE),
       out = Open((CONST_STRPTR) "NIL:", MODE_NEWFILE);
  if (!in || !out) {
    if (in)
      Close(in);
    if (out)
      Close(out);
    return 0;
  }
  struct Process *me = (struct Process *)FindTask(NULL);
  BPTR home = me->pr_HomeDir;
  me->pr_HomeDir = BNULL;
  LONG rc =
      SystemTags((CONST_STRPTR)line, SYS_Input, (IPTR)in, SYS_Output, (IPTR)out,
                 SYS_Asynch, TRUE, SYS_UserShell, TRUE, NP_StackSize, 262144,
                 NP_Name, (IPTR)name, TAG_END);
  me->pr_HomeDir = home;
  if (rc == -1) {
    Close(in);
    Close(out);
    return 0;
  }
  return 1;
}
/* The provider runs as a command in a Shell process that SystemTags names
 * itself (NP_Name is replaced on ABIv11), so it is found by its arguments:
 * the request file's path is unique per request. A provider that starts
 * another program with the same argument is found (and signalled) too. */
static int provider_signal(const char *request, ULONG signals) {
  int found = 0;
  Forbid();
  ULONG max = MaxCli();
  for (ULONG i = 1; i <= max; i++) {
    struct Process *p = FindCliProc(i);
    if (p && p->pr_Arguments && strstr((const char *)p->pr_Arguments, request)) {
      found++;
      if (signals)
        Signal(&p->pr_Task, signals);
    }
  }
  Permit();
  return found;
}
static int provider_running(const char *request) {
  return provider_signal(request, 0);
}
static void provider_break(const char *request) {
  provider_signal(request, SIGBREAKF_CTRL_C);
}
static int request(struct Worker *w, struct DockAppletRuntime *r,
                   const struct DockAppletRequest *q, const char *operation,
                   const char *query, uint64_t now) {
  char file[80];
  snprintf(file, sizeof file, "r-%lu.json", ++serial);
  if (!join(w->request, sizeof w->request, session_root, file))
    return 0;
  char a[200], b[200], c[200], d[300], op[100], root[DA_PATH], e[DA_PATH * 2];
  /* The provider finds its own files (Weather: certs/) from this, not from
   * the drawer of the process the Shell runs it in. Resolved per request,
   * so a package that resolves differently later is still named exactly. */
  if (!absolute(r->package.root, root)) {
    /* Passed on as configured; the provider then also tries its own drawer. */
    snprintf(root, sizeof root, "%s", r->package.root);
    NOTE("WIDGET %s: full path of %s not found; packageRoot passed as is", w->name, root);
  }
  if (!dock_json_quote(root, e, sizeof e) ||
      !dock_json_quote(q->package, a, sizeof a) ||
      !dock_json_quote(q->instance, b, sizeof b) ||
      !dock_json_quote(q->session, c, sizeof c) ||
      !dock_json_quote(query ? query : "", d, sizeof d) ||
      !dock_json_quote(operation, op, sizeof op))
    return 0;
  FILE *f = fopen(w->request, "wb");
  if (!f)
    return 0;
  int ok =
      fprintf(
          f,
          "{\"api\":1,\"package\":%s,\"packageRoot\":%s,\"instance\":%s,"
          "\"session\":%s,\"generation\":%lu,\"operation\":%s,\"query\":%s,"
          "\"settings\":%s}\n",
          a, e, b, c, q->generation, op, d,
          r->settings[0] ? r->settings : "{}") > 0;
  if (fclose(f))
    ok = 0;
  snprintf(w->name, sizeof w->name, "Widget provider %s", file);
  if (!ok || !launch(r->package.entry, w->request, w->name)) {
    remove(w->request);
    return 0;
  }
  w->owner = r;
  w->identity = *q;
  w->started = now;
  w->searching = query != NULL;
  w->ending = 0;
  NOTE("WIDGET request %s started (%s)", w->name, operation);
  return 1;
}
int dock_applet_search(struct DockAppletRuntime *r, const char *query,
                       uint64_t now) {
  if (r->searching)
    return 0;
  for (int i = 0; i < 4; i++)
    if (workers[i].owner == r)
      return 0;
  for (int i = 0; i < 4; i++)
    if (!workers[i].owner) {
      struct DockAppletRequest q;
      memset(&q, 0, sizeof q);
      strcpy(q.package, r->state.package);
      strcpy(q.instance, r->state.instance);
      strcpy(q.session, session);
      q.generation = ++serial;
      struct DockAppletForm form;
      char error[160];
      if (!dock_applet_load_form(r, &form, error, sizeof error))
        return 0;
      if (!request(&workers[i], r, &q, form.search, query, now))
        return 0;
      r->searching = 1;
      free(r->search_result);
      r->search_result = NULL;
      return 1;
    }
  return 0;
}
static void reconcile(struct DockAppletRuntime *r, uint64_t now) {
  if (!r->reconcile_pending)
    return;
  for (int i = 0; i < 4; i++)
    if (workers[i].owner == r)
      return;
  r->reconcile_pending = 0;
  if (!dock_applet_package_refresh(&r->package, &catalog, r->state.package, 0))
    return;
  dock_applet_instance_reset(&r->state, now);
  r->configured = 0;
  r->settings[0] = r->caption[0] = 0;
  r->changed = 1;
  struct DockAppletForm f;
  char error[160];
  if (r->package.available &&
      dock_applet_load_form(r, &f, error, sizeof error) &&
      dock_applet_settings_encode(&f, r->settings, sizeof r->settings, error,
                                  sizeof error)) {
    r->configured = 1;
    for (int i = 0; i < f.count; i++) {
      if (!strcmp(f.fields[i].key, r->package.secondary))
        snprintf(r->caption, sizeof r->caption, "%s", f.fields[i].value);
      if (!strcmp(f.fields[i].key, "refreshMinutes"))
        r->state.refresh_ms =
            (unsigned long)strtoul(f.fields[i].value, NULL, 10) * 60000;
    }
    cache_load(r);
  } else
    snprintf(r->state.snapshot.message, sizeof r->state.snapshot.message, "%s",
             r->package.available ? "Configure this widget in Settings"
                                  : r->package.reason);
}
static void retire(struct Worker *w) {
  char path[DA_PATH];
  if (snprintf(path, sizeof path, "%s.done", w->request) < (int)sizeof path)
    remove(path);
  remove(w->request);
  memset(w, 0, sizeof *w);
}
int dock_applets_ending(void) {
  int n = 0;
  for (int i = 0; i < 4; i++)
    n += workers[i].owner && workers[i].ending;
  return n;
}
/* Instances no dock uses any more (layout_begin left them inactive, and no
 * get made them active again), with no request out: removed for good. */
int dock_applets_collect(int (*in_use)(const struct DockAppletRuntime *)) {
  int freed = 0;
  for (struct DockAppletRuntime **p = &instances; *p;) {
    struct DockAppletRuntime *r = *p;
    int owned = 0;
    for (int i = 0; i < 4; i++)
      owned |= workers[i].owner == r;
    if (r->active || owned || (in_use && in_use(r))) {
      p = &r->next;
      continue;
    }
    *p = r->next;
    free(r->search_result);
    free(r);
    freed++;
  }
  return freed;
}
/* Instances the user removed for good, waiting until nothing can write their
 * drawer again. Memory only: what is still queued at Quit stays on disk. */
static struct { char package[DA_ID], instance[DA_ID]; } purges[16];
static int purge_count;
int dock_applets_forget(const char *package, const char *instance) {
  if (!dock_applet_id_valid(package) || !dock_applet_id_valid(instance))
    return 0;
  for (int i = 0; i < purge_count; i++)
    if (!strcmp(purges[i].package, package) && !strcmp(purges[i].instance, instance))
      return 1;
  if (purge_count == (int)(sizeof purges / sizeof purges[0])) {
    NOTE("WIDGET %s/%s: data kept, too many removals waiting", package, instance);
    return 0;
  }
  snprintf(purges[purge_count].package, DA_ID, "%s", package);
  snprintf(purges[purge_count].instance, DA_ID, "%s", instance);
  purge_count++;
  return 1;
}
int dock_applets_purge(int (*referenced)(const char *, const char *)) {
  int done = 0;
  for (int i = 0; i < purge_count;) {
    const char *package = purges[i].package, *instance = purges[i].instance;
    int alive = 0;
    /* Its runtime goes only once no provider works for it (collect), so a
     * provider still running cannot write the cache back afterwards. */
    for (struct DockAppletRuntime *r = instances; r; r = r->next)
      alive |= !strcmp(r->state.package, package) && !strcmp(r->state.instance, instance);
    if (alive) {
      i++;
      continue;
    }
    char parent[DA_PATH], dir[DA_PATH], error[80];
    if (referenced && referenced(package, instance))
      NOTE("WIDGET %s/%s: data kept, still used by the docks", package, instance);
    else if (join(parent, sizeof parent, state_root, package) &&
             join(dir, sizeof dir, parent, instance)) {
      if (dock_applet_purge_dir(dir, error, sizeof error))
        NOTE("WIDGET %s/%s: data deleted", package, instance);
      else
        NOTE("WIDGET %s/%s: cleanup incomplete: %s", package, instance, error);
      done++;
    }
    purges[i] = purges[--purge_count];
  }
  return done;
}
int dock_applets_count(void) {
  int n = 0;
  for (struct DockAppletRuntime *r = instances; r; r = r->next)
    n++;
  return n;
}
void dock_applets_poll(uint64_t now) {
  for (int i = 0; i < 4; i++) {
    struct Worker *w = &workers[i];
    if (!w->owner)
      continue;
    char path[DA_PATH];
    snprintf(path, sizeof path, "%s.done", w->request);
    size_t n;
    char *b = dock_applet_read(path, DA_RESPONSE_LIMIT, &n);
    if (b && w->ending) {
      /* A late answer to an ended request is thrown away, but the slot stays
       * taken until the provider has really gone: it may still be working,
       * and a retry must not run next to it. */
      NOTE("WIDGET late answer to %s after %lu ms discarded", w->name, (unsigned long)(now - w->started));
      free(b);
      b = NULL;
      remove(path);
    }
    if (b) {
      struct DockAppletSnapshot *s = malloc(sizeof *s);
      struct DockAppletRuntime *r = w->owner;
      int valid = s && dock_applet_snapshot_parse(b, n, s) &&
                  !strcmp(s->session, w->identity.session) &&
                  !strcmp(s->instance, w->identity.instance) &&
                  !strcmp(s->package, w->identity.package) &&
                  s->generation == w->identity.generation;
      if (w->searching) {
        r->searching = 0;
        if (valid) {
          free(r->search_result);
          r->search_result = s;
          s = NULL;
        }
      } else if (valid && dock_applet_accept(&r->state, s, now)) {
        NOTE("WIDGET answer to %s after %lu ms: %s, next in %lu s", w->name, (unsigned long)(now - w->started),
             s->status == DA_READY ? "data" : "failed", (unsigned long)((r->state.next_due - now) / 1000u));
        if (s->status == DA_READY) {
          char cache[DA_PATH];
          if (join(cache, sizeof cache, r->directory, "last-good.json")) {
            FILE *f = tmpfile();
            if (f) {
              if (dock_applet_cache_write(f, r->settings, s) &&
                  !fseek(f, 0, SEEK_END)) {
                long len = ftell(f);
                if (len > 0 && len < (long)(DA_RESPONSE_LIMIT + 65536) &&
                    !fseek(f, 0, SEEK_SET)) {
                  char *v = malloc((size_t)len + 1);
                  if (v && fread(v, 1, (size_t)len, f) == (size_t)len) {
                    v[len] = 0;
                    save_text(cache, v);
                  }
                  free(v);
                }
              }
              fclose(f);
            }
          }
        }
      } else if (r->state.pending &&
                 r->state.generation == w->identity.generation)
        dock_applet_fail(&r->state, "Invalid provider response", now);
      r->changed = 1;
      free(s);
      free(b);
      remove(path);
      remove(w->request);
      memset(w, 0, sizeof *w);
    } else if (w->ending) {
      /* The provider has gone (answer or not): the files go, the slot is free. */
      if (!provider_running(w->request)) {
        NOTE("WIDGET %s provider gone after %lu ms; slot free", w->name, (unsigned long)(now - w->started));
        retire(w);
      }
    } else if (now - w->started > REQUEST_TIMEOUT_MS) {
      struct DockAppletRuntime *r = w->owner;
      if (w->searching) {
        /* Tell the settings window instead of leaving it on "Searching...". */
        struct DockAppletSnapshot *s = calloc(1, sizeof *s);
        if (s) {
          s->status = DA_ERROR;
          snprintf(s->message, sizeof s->message, "No answer from the provider; try again");
          free(r->search_result);
          r->search_result = s;
        }
        r->searching = 0;
      } else if (r->state.pending && r->state.generation == w->identity.generation)
        dock_applet_fail(&r->state, "No answer from the provider; will try again", now);
      r->changed = 1;
      w->ending = 1;
      NOTE("WIDGET %s timed out after %lu ms; Ctrl-C sent to %d process(es)", w->name, (unsigned long)(now - w->started), provider_running(w->request));
      provider_break(w->request);
    }
  }
  for (struct DockAppletRuntime *r = instances; r; r = r->next) {
    reconcile(r, now);
    if (!r->active || !r->configured || !r->package.available || r->searching)
      continue;
    int busy = 0, slot = -1;
    for (int i = 0; i < 4; i++) {
      if (workers[i].owner == r)
        busy = 1;
      if (!workers[i].owner)
        slot = i;
    }
    if (busy || slot < 0)
      continue;
    struct DockAppletRequest q;
    if (dock_applet_tick(&r->state, now, &q)) {
      if (!request(&workers[slot], r, &q, r->package.operation, NULL, now))
        dock_applet_fail(&r->state, "Could not launch provider", now);
      r->changed = 1;
    }
  }
}
int dock_applet_asset_path(const struct DockAppletRuntime *r,const char *key,
                            char *out,size_t cap,struct DockSpriteSpec *spec){
 char path[DA_PATH],resource[512],relative[600];size_t n;int ok=0;
 if(!dock_applet_path(r->package.root,"assets/conditions.json",path,sizeof path))return 0;
 char *b=dock_applet_read(path,65536,&n);struct DockJson *j=malloc(sizeof *j);
 if(b&&j&&dock_json_parse(j,b,n)){
  int c=dock_json_get(j,0,"conditions"),e=dock_json_get(j,c,key);
  if(e<0)e=dock_json_get(j,c,"unknown");
  int at=spec?dock_json_get(j,e,"sprite"):e;
  if((!spec||(
     dock_json_int(j,dock_json_get(j,at,"frameWidth"),&spec->width)&&
     dock_json_int(j,dock_json_get(j,at,"frameHeight"),&spec->height)&&
     dock_json_int(j,dock_json_get(j,at,"columns"),&spec->columns)&&
     dock_json_int(j,dock_json_get(j,at,"frames"),&spec->count)&&
     dock_json_int(j,dock_json_get(j,at,"fps"),&spec->fps)))&&
     dock_json_string(j,dock_json_get(j,at,"resource"),resource,sizeof resource)){
   /* "file.pak#entry": the pack is the file checked on disk; the entry
    * part is appended for the sprite loader. */
   char *entry=strchr(resource,'#');
   if(entry)*entry++=0;
   snprintf(relative,sizeof relative,"assets/%s",resource);
   ok=dock_applet_path(r->package.root,relative,out,cap);
   if(ok && entry){
    size_t used=strlen(out);
    ok=*entry && !strpbrk(entry,"/:#\\") && used+1+strlen(entry)<cap;
    if(ok){out[used]='#';strcpy(out+used+1,entry);}
   }
  }
 }
 free(b);free(j);return ok;
}
int dock_applet_icon_path(const struct DockAppletRuntime *r,char *out,size_t cap){
 return dock_applet_asset_path(r,r->state.snapshot.icon,out,cap,NULL);
}
int dock_applet_sprite_path(const struct DockAppletRuntime *r,char *out,size_t cap,struct DockSpriteSpec *spec){
 return dock_applet_asset_path(r,r->state.snapshot.icon,out,cap,spec);
}
const char *dock_applet_primary(const struct DockAppletRuntime *r) {
  if (!r)
    return "Unavailable";
  if (!r->package.available)
    return "Unavailable";
  if (!r->configured)
    return "Settings...";
  if (!r->state.has_data)
    return r->state.snapshot.status == DA_ERROR ? "Offline" : "Loading...";
  return dock_applet_field(&r->state.snapshot, r->package.primary);
}
const char *dock_applet_compact(const struct DockAppletRuntime *r) {
  if (r && r->package.available && r->configured && !r->state.has_data &&
      r->state.snapshot.status != DA_ERROR)
    return "...";
  return r && r->state.has_data ? dock_applet_primary(r) : "--";
}
const char *dock_applet_secondary(const struct DockAppletRuntime *r) {
  return r && r->state.has_data
             ? dock_applet_field(&r->state.snapshot, r->package.secondary)
         : r ? (r->caption[0] ? r->caption : r->package.name)
             : "Widget";
}
/* Elapsed milliseconds and microseconds: not ReadEClock(), which on PC goes
 * back when the system time is set (see elapsed_aros.c). */
uint64_t dock_applets_now(void) { return dock_elapsed_ms(); }
uint64_t dock_applets_now_us(void) { return dock_elapsed_us(); }
void dock_applets_close(void) {
  if (clock_request) {
    CloseDevice((struct IORequest *)clock_request);
    DeleteIORequest((struct IORequest *)clock_request);
    clock_request = NULL;
  }
  if (clock_port) {
    DeleteMsgPort(clock_port);
    clock_port = NULL;
  }

  while (instances) {
    struct DockAppletRuntime *n = instances->next;
    free(instances->search_result);
    free(instances);
    instances = n;
  } /* Running workers own only files and process-local memory. Their spool is
deliberately retained: a timeout is not proof that an OS task has died. */
  memset(workers, 0, sizeof workers);
}
