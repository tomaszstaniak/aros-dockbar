#include "panel_aros.h"
#include "cards_aros.h"
#include "../zune_input.h"
#include "worker_aros.h"
#include <exec/libraries.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#define __MUIMASTER_NOLIBBASE__
#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define obj(cls, ...)                                                          \
  MUI_NewObjectA((ClassID)(cls), (struct TagItem *)(IPTR[]){                   \
                                     AROS_PP_VARIADIC_CAST2IPTR(__VA_ARGS__)})
static struct {
  struct Library *base;
  Object *application, *window, *body, *notice, *controls[DA_FORM_FIELDS],
      *query, *results, *selection;
  struct DockAppletRuntime *runtime;
  struct Screen *screen;
  struct DockAppletForm form;
  char message[200], entries[DA_PACKAGES][256];
  const char *entry_ptrs[DA_PACKAGES + 1],
      *choices[DA_FORM_FIELDS][DA_CHOICES + 1];
  char row_text[DA_ROWS][360], title[200];
  struct BitMap *bitmap;
  long pen[3];
  unsigned int wait;
  int again, mode, search_seen;
  unsigned long shown_generation;
} ui;
static struct DockAppletRuntime *last_instance;
static Object *attach(struct Library *MUIMasterBase, Object *parent,
                      Object *child) {
  if (!child)
    return NULL;
  if (!DoMethod(parent, OM_ADDMEMBER, (IPTR)child)) {
    MUI_DisposeObject(child);
    return NULL;
  }
  return child;
}
static Object *text(struct Library *MUIMasterBase, const char *s) {
  return obj(MUIC_Text, MUIA_Text_Contents, (IPTR)s, TAG_DONE);
}
static Object *button(struct Library *MUIMasterBase, Object *parent,
                      const char *s, int id) {
  Object *b =
      attach(MUIMasterBase, parent,
             obj(MUIC_Text, MUIA_Text_Contents, (IPTR)s, MUIA_Text_PreParse,
                 (IPTR) "\33c", MUIA_Frame, MUIV_Frame_Button, MUIA_Background,
                 MUII_ButtonBack, MUIA_InputMode, MUIV_InputMode_RelVerify,
                 MUIA_CycleChain, TRUE, TAG_DONE));
  if (b)
    DoMethod(b, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)ui.application, 2,
             MUIM_Application_ReturnID, id);
  return b;
}
int (*dock_applet_ui_editable)(struct DockAppletRuntime *, const char **refusal);
static int editable(struct DockAppletRuntime *r, const char **why) {
  *why = NULL;
  return !dock_applet_ui_editable || dock_applet_ui_editable(r, why);
}
static void message(const char *s) {
  snprintf(ui.message, sizeof ui.message, "%s", s);
  if (ui.notice)
    SetAttrs(ui.notice, MUIA_Text_Contents, (IPTR)ui.message, TAG_DONE);
}
static void clean(char *s) {
  for (; *s; s++)
    if ((unsigned char)*s < 32 || *s == 127)
      *s = ' ';
}
void dock_applet_ui_close(void) {
  dock_applet_cards_close();
  struct Library *MUIMasterBase = ui.base;
  if (MUIMasterBase) {
    if (ui.window)
      SetAttrs(ui.window, MUIA_Window_Open, FALSE, TAG_DONE);
    if (ui.application)
      MUI_DisposeObject(ui.application);
    else if (ui.window)
      MUI_DisposeObject(ui.window);
    if (ui.bitmap)
      FreeBitMap(ui.bitmap);
    for (int i = 0; i < 3; i++)
      if (ui.pen[i] >= 0 && ui.screen)
        ReleasePen(ui.screen->ViewPort.ColorMap, ui.pen[i]);
    CloseLibrary(MUIMasterBase);
  }
  memset(&ui, 0, sizeof ui);
}
static int begin(struct Screen *screen, const char *title, int mode) {
  dock_applet_ui_close();
  ui.screen = screen;
  ui.mode = mode;
  for (int i = 0; i < 3; i++)
    ui.pen[i] = -1;
  ui.base = OpenLibrary((CONST_STRPTR) "muimaster.library", 0);
  struct Library *MUIMasterBase = ui.base;
  if (!MUIMasterBase)
    return 0;
  ui.application = obj(MUIC_Application, MUIA_Application_Title,
                       (IPTR) "Dockbar widgets", TAG_DONE);
  ui.body = obj(MUIC_Group, TAG_DONE);
  if (!ui.application || !ui.body)
    goto fail;
  ui.window = obj(MUIC_Window, MUIA_Window_Title, (IPTR)title,
                  MUIA_Window_Screen, (IPTR)screen, MUIA_Window_RootObject,
                  (IPTR)ui.body, MUIA_Window_Width, 560, MUIA_Window_Height,
                  mode == 2 ? 470 : 330, TAG_DONE);
  if (!ui.window)
    goto fail;
  if (!DoMethod(ui.application, OM_ADDMEMBER, (IPTR)ui.window))
    goto fail;
  DoMethod(ui.window, MUIM_Notify, MUIA_Window_InputEvent, (IPTR) "esc",
           (IPTR)ui.application, 2, MUIM_Application_ReturnID, 99);
  DoMethod(ui.window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
           (IPTR)ui.application, 2, MUIM_Application_ReturnID, 99);
  return 1;
fail:
  dock_applet_ui_close();
  return 0;
}
static int show(void) {
  struct Library *MUIMasterBase = ui.base;
  /* A widget on a locked dock: its details say why there is no Save or
   * Remove, also after the window was rebuilt for new data. */
  const char *why = NULL;
  if (ui.mode == 2 && ui.runtime && !ui.message[0] && !editable(ui.runtime, &why))
    snprintf(ui.message, sizeof ui.message, "%s", why ? why : "This widget cannot be changed now");
  ui.notice = attach(MUIMasterBase, ui.body, text(MUIMasterBase, ui.message));
  SetAttrs(ui.window, MUIA_Window_Open, TRUE, TAG_DONE);
  IPTR open = 0;
  GetAttr(MUIA_Window_Open, ui.window, &open);
  if (!open) {
    dock_applet_ui_close();
    return 0;
  }
  return 1;
}
struct Window *dock_applet_ui_window(void) {
  if(dock_applet_cards_window())return dock_applet_cards_window();
  struct Library *MUIMasterBase = ui.base;
  IPTR window = 0;
  if (MUIMasterBase && ui.window)
    GetAttr(MUIA_Window_Window, ui.window, &window);
  return (struct Window *)window;
}
struct DockAppletRuntime *dock_applet_ui_instance(void) {
  return ui.runtime ? ui.runtime : last_instance;
}
/* The instance is shown by an open settings window or card. last_instance
 * alone does not count: it outlives the window for the Remove action. */
int dock_applet_ui_shows(const struct DockAppletRuntime *r) {
  return dock_applet_ui_window() &&
         (ui.runtime == r || dock_applet_cards_instance() == r);
}
/* r is about to be freed: no pointer to it may remain. */
void dock_applet_ui_forget(const struct DockAppletRuntime *r) {
  if (last_instance == r)
    last_instance = NULL;
  if (ui.runtime == r)
    ui.runtime = NULL;
  dock_applet_cards_forget(r);
}
int dock_applet_ui_picker(struct Screen *screen) {
  if (!begin(screen, "Add widget", 0))
    return 0;
  struct Library *MUIMasterBase = ui.base;
  const struct DockAppletCatalog *c = dock_applets_catalog();
  for (int i = 0; i < c->count; i++) {
    const struct DockAppletPackage *p = &c->packages[i];
    snprintf(ui.entries[i], sizeof ui.entries[i], "%.75s%s%.150s", p->name,
             p->available ? "" : " - ", p->reason);
    clean(ui.entries[i]);
    ui.entry_ptrs[i] = ui.entries[i];
  }
  ui.entry_ptrs[c->count] = NULL;
  if (c->count)
    ui.selection =
        attach(MUIMasterBase, ui.body,
               obj(MUIC_Cycle, MUIA_Cycle_Entries, (IPTR)ui.entry_ptrs,
                   MUIA_CycleChain, TRUE, TAG_DONE));
  else
    attach(MUIMasterBase, ui.body,
           text(MUIMasterBase, "No packages found in Widgets. Copy a package "
                               "folder there, then Rescan."));
  if (ui.runtime && ui.runtime->recovered)
    snprintf(
        ui.message, sizeof ui.message,
        "Recovered settings backup. Save restores the primary settings file.");
  Object *buttons = attach(MUIMasterBase, ui.body,
                           obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
  button(MUIMasterBase, buttons, "Add", 1);
  button(MUIMasterBase, buttons, "Rescan", 5);
  button(MUIMasterBase, buttons, "Cancel", 99);
  if (c->overflow)
    snprintf(ui.message, sizeof ui.message,
             "Package limit reached (64). Extra packages were not loaded.");
  return show();
}
static int settings_form(void) {
  struct Library *MUIMasterBase = ui.base;
  if (!dock_applet_load_form(ui.runtime, &ui.form, ui.message,
                             sizeof ui.message))
    return 0;
  if (ui.form.search[0]) {
    Object *line = attach(MUIMasterBase, ui.body,
                          obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    ui.query = attach(MUIMasterBase, line,
                      obj(MUIC_String, MUIA_String_MaxLen, 120, MUIA_CycleChain,
                          TRUE, TAG_DONE));
    button(MUIMasterBase, line, ui.form.search_label, 10);
    ui.entry_ptrs[0] = "Search for a city, or enter coordinates below";
    ui.entry_ptrs[1] = NULL;
    ui.results = attach(
        MUIMasterBase, ui.body,
        obj(MUIC_Cycle, MUIA_Cycle_Entries, (IPTR)ui.entry_ptrs, TAG_DONE));
    button(MUIMasterBase, ui.body, "Use selected city", 11);
  }
  if (ui.runtime->recovered)
    snprintf(
        ui.message, sizeof ui.message,
        "Recovered settings backup. Save restores the primary settings file.");
  for (int i = 0; i < ui.form.count; i++) {
    struct DockAppletSetting *v = &ui.form.fields[i];
    Object *line = attach(MUIMasterBase, ui.body,
                          obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    attach(MUIMasterBase, line, text(MUIMasterBase, v->label));
    if (v->choice_count) {
      int active = 0;
      for (int k = 0; k < v->choice_count; k++) {
        ui.choices[i][k] = v->choices[k];
        if (!strcmp(v->value, v->choices[k]))
          active = k;
      }
      ui.choices[i][v->choice_count] = NULL;
      ui.controls[i] = attach(MUIMasterBase, line,
                              obj(MUIC_Cycle, MUIA_Cycle_Entries,
                                  (IPTR)ui.choices[i], MUIA_Cycle_Active,
                                  active, MUIA_CycleChain, TRUE, TAG_DONE));
    } else
      ui.controls[i] =
          attach(MUIMasterBase, line,
                 obj(MUIC_String, MUIA_String_Contents, (IPTR)v->value,
                     MUIA_String_MaxLen, v->max_length + 1, MUIA_CycleChain,
                     TRUE, TAG_DONE));
    if (!ui.controls[i])
      return 0;
  }
  Object *buttons = attach(MUIMasterBase, ui.body,
                           obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
  button(MUIMasterBase, buttons, "Save", 2);
  button(MUIMasterBase, buttons, "Cancel", 99);
  button(MUIMasterBase, buttons, "Remove widget", 4);
  return 1;
}
static Object *chart(struct Library *MUIMasterBase,
                     const struct DockAppletSnapshot *s) {
  int count = 0;
  double lo = 1e9, hi = -1e9;
  for (int i = 0; i < s->row_count; i++)
    if (s->rows[i].has_plot) {
      double v = s->rows[i].plot;
      if (v < lo)
        lo = v;
      if (v > hi)
        hi = v;
      count++;
    }
  if (count < 2)
    return NULL;
  if (hi - lo < 1)
    hi = lo + 1;
  int w = 440, h = 70;
  ui.bitmap =
      AllocBitMap(w, h, GetBitMapAttr(ui.screen->RastPort.BitMap, BMA_DEPTH),
                  BMF_CLEAR, ui.screen->RastPort.BitMap);
  if (!ui.bitmap)
    return NULL;
  struct RastPort rp;
  InitRastPort(&rp);
  rp.BitMap = ui.bitmap;
  ui.pen[0] = ObtainBestPen(ui.screen->ViewPort.ColorMap, 0x17000000,
                            0x23000000, 0x31000000, TAG_DONE);
  ui.pen[1] = ObtainBestPen(ui.screen->ViewPort.ColorMap, 0x54000000,
                            0xb9000000, 0xff000000, TAG_DONE);
  if (ui.pen[0] < 0 || ui.pen[1] < 0)
    return NULL;
  SetAPen(&rp, (ULONG)ui.pen[0]);
  RectFill(&rp, 0, 0, w - 1, h - 1);
  SetAPen(&rp, (ULONG)ui.pen[1]);
  int k = 0;
  for (int i = 0; i < s->row_count; i++)
    if (s->rows[i].has_plot) {
      int x = 4 + k * (w - 8) / (count - 1),
          y = h - 5 - (int)((s->rows[i].plot - lo) * (h - 10) / (hi - lo));
      if (k)
        Draw(&rp, x, y);
      else
        Move(&rp, x, y);
      k++;
    }
  return obj(MUIC_Bitmap, MUIA_Bitmap_Bitmap, (IPTR)ui.bitmap,
             MUIA_Bitmap_Width, w, MUIA_Bitmap_Height, h, TAG_DONE);
}
int dock_applet_ui_open(struct Screen *screen, struct DockAppletRuntime *r,
                        int settings) {
  const char *why = NULL;
  if (!r)
    return 0;
  if (settings && !editable(r, &why)) {
    /* Already showing its details: stay, and say why. */
    if (ui.runtime == r && ui.mode == 2) {
      message(why ? why : "This widget cannot be changed now");
      return 1;
    }
    return dock_applet_ui_open(screen, r, 0);
  }
  if (!settings && ui.runtime == r && ui.mode == 2) {
    dock_applet_ui_close();
    return 1;
  }
  if (!begin(screen, settings ? "Widget settings" : "Widget details",
             settings ? 1 : 2))
    return 0;
  ui.runtime = r;
  last_instance = r;
  struct Library *MUIMasterBase = ui.base;
  if (settings) {
    if (!settings_form()) {
      Object *buttons =
          attach(MUIMasterBase, ui.body,
                 obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
      button(MUIMasterBase, buttons, "Remove widget", 4);
      button(MUIMasterBase, buttons, "Close", 99);
      return show();
    }
  } else {
    const struct DockAppletSnapshot *s = &r->state.snapshot;
    ui.shown_generation = r->state.view_revision;
    snprintf(ui.title, sizeof ui.title, "%.110s   %.50s%s",
             s->title[0] ? s->title : r->package.name, dock_applet_primary(r),
             s->status == DA_STALE ? " (stale)" : "");
    clean(ui.title);
    attach(MUIMasterBase, ui.body, text(MUIMasterBase, ui.title));
    Object *graph = chart(MUIMasterBase, s);
    if (graph)
      attach(MUIMasterBase, ui.body, graph);
    Object *rows = obj(MUIC_Virtgroup, TAG_DONE);
    if (!rows) {
      dock_applet_ui_close();
      return 0;
    }
    for (int i = 0; i < s->row_count; i++) {
      snprintf(ui.row_text[i], sizeof ui.row_text[i], "%.150s    %.150s",
               s->rows[i].label, s->rows[i].value);
      clean(ui.row_text[i]);
      attach(MUIMasterBase, rows, text(MUIMasterBase, ui.row_text[i]));
    }
    Object *scroll =
        obj(MUIC_Scrollgroup, MUIA_Scrollgroup_Contents, (IPTR)rows, TAG_DONE);
    if (scroll)
      attach(MUIMasterBase, ui.body, scroll);
    else
      MUI_DisposeObject(rows);
    char age[200];
    double elapsed = difftime(time(NULL), (time_t)s->updated);
    snprintf(age, sizeof age, "Updated %.0f minutes ago%s",
             elapsed > 0 ? elapsed / 60 : 0,
             s->status == DA_STALE ? " - cached data" : "");
    if (r->state.has_data)
      attach(MUIMasterBase, ui.body, text(MUIMasterBase, age));
    if (s->attribution[0])
      attach(MUIMasterBase, ui.body, text(MUIMasterBase, s->attribution));
    if (s->url[0])
      attach(MUIMasterBase, ui.body, text(MUIMasterBase, s->url));
    Object *buttons = attach(MUIMasterBase, ui.body,
                             obj(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    button(MUIMasterBase, buttons, "Refresh", 3);
    button(MUIMasterBase, buttons, "Settings", 6);
    button(MUIMasterBase, buttons, "Close", 99);
    snprintf(ui.message, sizeof ui.message, "%s", s->message);
  }
  return show();
}
int dock_applet_ui_open_at(struct Screen *screen,struct DockAppletRuntime *r,
 int x,int y,int w,int h,int vertical,const struct DockColors *colors){
 if(!r)return 0;
 if(dock_applet_cards_window()&&dock_applet_cards_instance()==r){dock_applet_ui_close();return 1;}
 dock_applet_ui_close();
 if(r->configured&&dock_applet_cards_open(screen,r,x,y,w,h,vertical,colors)){
  ui.screen=screen;ui.runtime=r;last_instance=r;return 1;
 }
 return dock_applet_ui_open(screen,r,!r->configured);
}
int dock_applet_ui_poll(uint64_t now, char *selected, size_t cap) {
  if(dock_applet_cards_window()){
    int action=dock_applet_cards_poll(now);
    if(action==6)dock_applet_ui_open(ui.screen,ui.runtime,1);
    return 0;
  }
  if (!ui.application)
    return 0;
  if (ui.mode == 2 && ui.runtime &&
      ui.shown_generation != ui.runtime->state.view_revision) {
    struct DockAppletRuntime *r = ui.runtime;
    struct Screen *screen = ui.screen;
    dock_applet_ui_close();
    dock_applet_ui_open(screen, r, 0);
    return 0;
  }
  if (ui.mode == 1 && ui.runtime && ui.runtime->search_result &&
      !ui.search_seen) {
    struct DockAppletSnapshot *s = ui.runtime->search_result;
    ui.search_seen = 1;
    for (int i = 0; i < s->row_count; i++) {
      snprintf(ui.entries[i], sizeof ui.entries[i], "%s", s->rows[i].label);
      clean(ui.entries[i]);
      ui.entry_ptrs[i] = ui.entries[i];
    }
    if (!s->row_count) {
      ui.entry_ptrs[0] = "No cities found";
      ui.entry_ptrs[1] = NULL;
    } else
      ui.entry_ptrs[s->row_count] = NULL;
    if (ui.results)
      SetAttrs(ui.results, MUIA_Cycle_Entries, (IPTR)ui.entry_ptrs,
               MUIA_Cycle_Active, 0, TAG_DONE);
    message(s->status == DA_ERROR ? s->message
            : s->row_count        ? "Choose a city and click Use selected city"
                                  : "No cities found");
  }
  IPTR action = dock_zune_input(ui.application, &ui.wait, &ui.again);
  if (action == MUIV_Application_ReturnID_Quit || action == 99) {
    dock_applet_ui_close();
    return 0;
  }
  if (action == 5) {
    struct Screen *screen = ui.screen;
    dock_applets_rescan();
    dock_applet_ui_picker(screen);
  }
  if (action == 1 && ui.mode == 0) {
    IPTR at = 0;
    if (ui.selection)
      GetAttr(MUIA_Cycle_Active, ui.selection, &at);
    const struct DockAppletCatalog *c = dock_applets_catalog();
    if (at < (IPTR)c->count && c->packages[at].available) {
      snprintf(selected, cap, "%s", c->packages[at].id);
      dock_applet_ui_close();
      return 1;
    }
    message(c->count ? c->packages[at].reason : "No widgets installed");
  }
  if (action == 2 && ui.runtime) {
    for (int i = 0; i < ui.form.count; i++) {
      struct DockAppletSetting *v = &ui.form.fields[i];
      IPTR value = 0;
      if (v->choice_count) {
        GetAttr(MUIA_Cycle_Active, ui.controls[i], &value);
        if (value < (IPTR)v->choice_count)
          snprintf(v->value, sizeof v->value, "%s", v->choices[value]);
      } else {
        GetAttr(MUIA_String_Contents, ui.controls[i], &value);
        snprintf(v->value, sizeof v->value, "%s",
                 value ? (const char *)value : "");
      }
    }
    char error[200];
    const char *why = NULL;
    if (!editable(ui.runtime, &why))
      message(why ? why : "This widget cannot be changed now");
    else if (dock_applet_save_settings(ui.runtime, &ui.form, error, sizeof error))
      dock_applet_ui_close();
    else
      message(error);
  }
  if (action == 3 && ui.runtime)
    message(dock_applet_refresh(&ui.runtime->state, now)
                ? "Refreshing..."
                : "Already refreshing; manual refresh is limited to once per "
                  "30 seconds");
  if (action == 4 && ui.runtime) {
    const char *why = NULL;
    if (!editable(ui.runtime, &why)) {
      message(why ? why : "This widget cannot be changed now");
      return 0;
    }
    last_instance = ui.runtime;
    dock_applet_ui_close();
    return 2;
  }
  if (action == 6 && ui.runtime) {
    struct DockAppletRuntime *r = ui.runtime;
    struct Screen *screen = ui.screen;
    dock_applet_ui_open(screen, r, 1);
  }
  if (action == 10 && ui.runtime) {
    IPTR query = 0;
    GetAttr(MUIA_String_Contents, ui.query, &query);
    if (query && *(char *)query &&
        dock_applet_search(ui.runtime, (const char *)query, now)) {
      ui.search_seen = 0;
      message("Searching...");
    } else
      message("Enter a city, or wait for the current request to finish");
  }
  if (action == 11 && ui.runtime && ui.runtime->search_result) {
    IPTR at = 0;
    GetAttr(MUIA_Cycle_Active, ui.results, &at);
    const struct DockAppletSnapshot *s = ui.runtime->search_result;
    if (at < (IPTR)s->row_count) {
      char lat[64], lon[64];
      if (sscanf(s->rows[at].value, "%63[^,],%63[^|]", lat, lon) == 2)
        for (int i = 0; i < ui.form.count; i++) {
          const char *value = NULL;
          struct DockAppletSetting *v = &ui.form.fields[i];
          if (!strcmp(v->key, ui.form.name_key))
            value = s->rows[at].label;
          else if (!strcmp(v->key, ui.form.latitude_key))
            value = lat;
          else if (!strcmp(v->key, ui.form.longitude_key))
            value = lon;
          if (value) {
            snprintf(v->value, sizeof v->value, "%.100s", value);
            SetAttrs(ui.controls[i], MUIA_String_Contents, (IPTR)v->value,
                     TAG_DONE);
          }
        }
    }
  }
  return 0;
}
