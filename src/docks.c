#include "docks.h"
#include "config_io.h"
#include "frame.h"
#include "layout.h"
#include "theme.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Large scratch copies live off the small AROS default stack. */
static struct DockSet scratch_set;
static struct DockModel scratch_from, scratch_to;

void dock_look_default(struct DockLook *look)
{
    look->icon = DOCK_ICON_DEFAULT;
    look->corner = DOCK_CORNER_DEFAULT;
    look->spacing = 0;
    look->labels = DOCK_LABELS_HOVER;
    look->orientation = DOCK_HORIZONTAL;
    look->transparency = look->glass = look->theme = look->background = look->grip = look->hover = look->running = DOCK_INHERIT;
    look->present = 0;
    look->buttons = 0;
    look->beside = 0;
}

void dock_appearance_default(struct DockAppearance *a)
{
    a->theme = DOCK_THEME_AUTO;
    a->background = DOCK_BG_BAR;
    a->transparency = 0;
    a->glass = 0;
    a->grip = 1;
    a->text = 0;
    a->hover = DOCK_HOVER_LIFT;
    a->running = 0;
}

void dock_appearance_legacy(struct DockAppearance *a)
{
    dock_appearance_default(a);
    a->theme = DOCK_THEME_DARK;
    a->hover = DOCK_HOVER_NONE;  /* existing configurations keep still icons */
}

void dock_appearance_of(const struct DockSet *set, int index, struct DockAppearance *out)
{
    const struct DockLook *l = &set->docks[index].look;
    *out = set->defaults;
    if (l->theme != DOCK_INHERIT) out->theme = l->theme;
    if (l->background != DOCK_INHERIT) out->background = l->background;
    if (l->transparency != DOCK_INHERIT) out->transparency = l->transparency;
    if (l->glass != DOCK_INHERIT) out->glass = l->glass;
    if (l->grip != DOCK_INHERIT) out->grip = l->grip;
    if (l->hover != DOCK_INHERIT) out->hover = l->hover;
    if (l->running != DOCK_INHERIT) out->running = l->running;
}

static int in(int v, int lo, int hi, int inherit)
{
    return (inherit && v == DOCK_INHERIT) || (v >= lo && v <= hi);
}

static int appearance_valid(const struct DockAppearance *a)
{
    return in(a->theme, DOCK_THEME_DARK, DOCK_THEME_AUTO, 0) && in(a->background, DOCK_BG_BAR, DOCK_BG_NONE, 0) &&
           in(a->transparency, 0, 100, 0) && in(a->glass, 0, 1, 0) && in(a->grip, 0, 1, 0) && in(a->text, 0, 1, 0) &&
           in(a->hover, DOCK_HOVER_NONE, DOCK_HOVER_MAGNIFY, 0) && in(a->running, 0, 1, 0);
}

void dock_place_default(struct DockPlace *place)
{
    memset(place, 0, sizeof(*place));
    place->mode = DOCK_PLACE_BOTTOM;
    place->align = DOCK_ALIGN_CENTER;
    place->offset = 8;
}

static int look_valid(const struct DockLook *l)
{
    return l->icon >= DOCK_ICON_MIN && l->icon <= DOCK_ICON_MAX &&
           l->corner >= 0 && l->corner <= DOCK_CORNER_MAX &&
           l->spacing >= 0 && l->spacing <= 16 &&
           l->labels >= DOCK_LABELS_NONE && l->labels <= DOCK_LABELS_ALWAYS &&
           (l->orientation == DOCK_HORIZONTAL || l->orientation == DOCK_VERTICAL) &&
           in(l->transparency, 0, 100, 1) && in(l->glass, 0, 1, 1) &&
           in(l->theme, DOCK_THEME_DARK, DOCK_THEME_AUTO, 1) && in(l->background, DOCK_BG_BAR, DOCK_BG_NONE, 1) &&
           in(l->grip, 0, 1, 1) && in(l->hover, DOCK_HOVER_NONE, DOCK_HOVER_MAGNIFY, 1) && l->present >= 0 && l->present <= 2 && (l->buttons == 0 || l->buttons == 1) &&
           (l->beside == 0 || l->beside == 1) && in(l->running, 0, 1, 1);
}

static int place_valid(const struct DockPlace *p)
{
    return p->mode >= DOCK_PLACE_FREE && p->mode <= DOCK_PLACE_RIGHT &&
           p->align >= DOCK_ALIGN_START && p->align <= DOCK_ALIGN_END &&
           p->offset >= 0 && p->offset <= 400 && (p->valid == 0 || p->valid == 1) &&
           (p->locked == 0 || p->locked == 1);
}

static int name_valid(const char *name)
{
    return name[0] && dock_cfg_text_valid(name, DOCK_NAME) && !strchr(name, '\t');
}

int dock_set_valid(const struct DockSet *set) {
  if (!set || set->count < 1 || set->count > DOCK_MAX_DOCKS ||
      set->next_id < 1 || !appearance_valid(&set->defaults))
    return 0;
  for (int i = 0; i < set->count; i++) {
    const struct DockInstance *d = &set->docks[i];
    if (d->id < 1 || d->id >= set->next_id || !name_valid(d->name) ||
        !look_valid(&d->look) || !place_valid(&d->place) ||
        !dock_cfg_model_valid(&d->model, 8))
      return 0;
    for (int item = 0; item < d->model.count; item++)
      if (d->model.items[item].kind == DOCK_APPLET) {
        const struct DockItem *a = &d->model.items[item];
        for (int pi = 0; pi <= i; pi++)
          for (int pj = 0; pj < set->docks[pi].model.count; pj++) {
            if (pi == i && pj >= item)
              break;
            const struct DockItem *b = &set->docks[pi].model.items[pj];
            if (b->kind == DOCK_APPLET && !strcmp(a->path, b->path) &&
                !strcmp(a->dir, b->dir))
              return 0;
          }
      }
    for (int j = 0; j < i; j++)
      if (set->docks[j].id == d->id)
        return 0;
  }
  return 1;
}

int dock_set_find(const struct DockSet *set, int id) {
  for (int i = 0; set && i < set->count; i++)
    if (set->docks[i].id == id)
      return i;
  return -1;
}

/* ---- file format, version 2 ------------------------------------------- */

static int read_model(FILE *file, struct DockModel *model, int count,
                      int version) {
  char line[CONFIG_LINE], *fields[4];
  int n;
  model->count = count;
  for (int i = 0; i < count; i++)
    if (!dock_cfg_read_item(file, model->items + i, version))
      return 0;
  while ((n = dock_cfg_record(file, line, fields, 4)) != 0) {
    int group;
    if (n == 1 && !strcmp(fields[0], "ENDDOCK"))
      return 1;
    if (n != 3 || strcmp(fields[0], "GROUP") ||
        !dock_cfg_number(fields[1], 1, DOCK_MAX_GROUPS, &group) ||
        model->groups[group - 1].used)
      return 0;
    struct DockGroup *g = &model->groups[group - 1];
    if (!dock_cfg_number(fields[2], 0, DOCK_MAX_ITEMS, &g->count))
      return 0;
    g->used = 1;
    for (int i = 0; i < g->count; i++)
      if (!dock_cfg_read_item(file, g->items + i, version))
        return 0;
  }
  return 0;
}

static int read_set(FILE *file, struct DockSet *set)
{
    char line[CONFIG_LINE], *f[16];
    int n, version;
    memset(set, 0, sizeof(*set));
    if (dock_cfg_record(file, line, f, 16) != 3 || strcmp(f[0], "DOCKBAR") ||
        !dock_cfg_number(f[1], 2, 10, &version) || !dock_cfg_number(f[2], 1, INT_MAX, &set->next_id))
        return 0;
    if (version == 2) dock_appearance_legacy(&set->defaults);
    else {
        struct DockAppearance *a = &set->defaults;
        /* Files before version 9 had no hover effect: their icons stay still. */
        a->hover = DOCK_HOVER_NONE;
        if (dock_cfg_record(file, line, f, 16) != (version >= 10 ? 9 : version >= 9 ? 8 : version >= 4 ? 7 : 6) || strcmp(f[0], "DEFAULTS") ||
            !dock_cfg_number(f[1], 0, 1000, &a->theme) || !dock_cfg_number(f[2], 0, 1000, &a->background) ||
            !dock_cfg_number(f[3], 0, 1000, &a->transparency) || !dock_cfg_number(f[4], 0, 1000, &a->glass) ||
            !dock_cfg_number(f[5], 0, 1000, &a->grip) || (version >= 4 && !dock_cfg_number(f[6], 0, 1, &a->text)) ||
            (version >= 9 && !dock_cfg_number(f[7], 0, 1000, &a->hover)) ||
            (version >= 10 && !dock_cfg_number(f[8], 0, 1000, &a->running)))
            return 0;
    }
    while ((n = dock_cfg_record(file, line, f, 16)) != 0) {
        if (n == 1 && !strcmp(f[0], "END"))
            return fgetc(file) == EOF && !ferror(file) && dock_set_valid(set);
        if (set->count == DOCK_MAX_DOCKS || n != 3 || strcmp(f[0], "DOCK")) return 0;
        struct DockInstance *d = &set->docks[set->count++];
        struct DockLook *l = &d->look;
        struct DockPlace *p = &d->place;
        int count;
        if (!dock_cfg_number(f[1], 1, INT_MAX, &d->id) || strlen(f[2]) >= DOCK_NAME) return 0;
        strcpy(d->name, f[2]);
        int fields = version == 2 ? 8 : version == 3 ? 11 : version == 4 ? 13 : version <= 8 ? 14 : version == 9 ? 15 : 16;
        if (dock_cfg_record(file, line, f, 16) != fields || strcmp(f[0], "LOOK") ||
            !dock_cfg_number(f[1], 0, 1000, &l->icon) || !dock_cfg_number(f[2], 0, 1000, &l->corner) ||
            !dock_cfg_number(f[3], 0, 1000, &l->spacing) || !dock_cfg_number(f[4], 0, 1000, &l->labels) ||
            !dock_cfg_number(f[5], 0, 1000, &l->orientation) || !dock_cfg_number(f[6], -1, 1000, &l->transparency) ||
            !dock_cfg_number(f[7], -1, 1000, &l->glass))
            return 0;
        if (version == 2) {
            /* 0.2 stored transparency and glass per dock: keep them as this
             * dock's own values; colours, bar and grip come from the defaults,
             * which for 0.2 files are 0.2's look. */
            if (l->transparency < 0 || l->glass < 0) return 0;
            l->theme = l->background = l->grip = DOCK_INHERIT;
        } else if (!dock_cfg_number(f[8], -1, 1000, &l->theme) || !dock_cfg_number(f[9], -1, 1000, &l->background) ||
                   !dock_cfg_number(f[10], -1, 1000, &l->grip))
            return 0;
        l->present = l->buttons = 0;
        l->beside = version == 4;  /* 0.4-test1 drew icon and text side by side */
        if (version >= 4 && (!dock_cfg_number(f[11], 0, 2, &l->present) || !dock_cfg_number(f[12], 0, 1, &l->buttons)))
            return 0;
        if (version >= 5 && !dock_cfg_number(f[13], 0, 1, &l->beside)) return 0;
        l->hover = DOCK_INHERIT;
        if (version >= 9 && !dock_cfg_number(f[14], -1, 1000, &l->hover)) return 0;
        l->running = DOCK_INHERIT;
        if (version >= 10 && !dock_cfg_number(f[15], -1, 1000, &l->running)) return 0;
        if (dock_cfg_record(file, line, f, 16) != 8 || strcmp(f[0], "PLACE") ||
            !dock_cfg_number(f[1], 0, 1000, &p->mode) || !dock_cfg_number(f[2], 0, 1000, &p->align) ||
            !dock_cfg_number(f[3], 0, 1000, &p->offset) || !dock_cfg_number(f[4], 0, 1, &p->valid) ||
            !dock_cfg_number(f[5], INT_MIN, INT_MAX, &p->x) || !dock_cfg_number(f[6], INT_MIN, INT_MAX, &p->y) ||
            !dock_cfg_number(f[7], 0, 1, &p->locked))
            return 0;
        if (dock_cfg_record(file, line, f, 16) != 2 || strcmp(f[0], "ROOT") ||
            !dock_cfg_number(f[1], 0, DOCK_MAX_ITEMS, &count) || !read_model(file, &d->model, count, version))
            return 0;
    }
    return 0;
}

static int write_set(FILE *file, const struct DockSet *set)
{
    const struct DockAppearance *a = &set->defaults;
    if (fprintf(file, "DOCKBAR\t10\t%d\nDEFAULTS\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", set->next_id,
                a->theme, a->background, a->transparency, a->glass, a->grip, a->text, a->hover, a->running) < 0)
        return 0;
    for (int i = 0; i < set->count; i++) {
        const struct DockInstance *d = &set->docks[i];
        const struct DockLook *l = &d->look;
        const struct DockPlace *p = &d->place;
        /* One record per call: ABIv11's varargs stubs pass at most 22
         * integer arguments in all, so fprintf takes 20 after file and
         * format (tests/varargs_probe.c; working notes). */
        if (fprintf(file, "DOCK\t%d\t%s\n", d->id, d->name) < 0 ||
            fprintf(file, "LOOK\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                    l->icon, l->corner, l->spacing, l->labels, l->orientation,
                    l->transparency, l->glass, l->theme, l->background, l->grip, l->present, l->buttons, l->beside,
                    l->hover, l->running) < 0 ||
            fprintf(file, "PLACE\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                    p->mode, p->align, p->offset, p->valid, p->x, p->y, p->locked) < 0 ||
            fprintf(file, "ROOT\t%d\n", d->model.count) < 0)
            return 0;
        for (int j = 0; j < d->model.count; j++)
            if (!dock_cfg_write_item_v4(file, d->model.items + j)) return 0;
        for (int g = 0; g < DOCK_MAX_GROUPS; g++) {
            const struct DockGroup *group = &d->model.groups[g];
            if (!group->used) continue;
            if (fprintf(file, "GROUP\t%d\t%d\n", g + 1, group->count) < 0) return 0;
            for (int j = 0; j < group->count; j++)
                if (!dock_cfg_write_item_v4(file, group->items + j)) return 0;
        }
        if (fputs("ENDDOCK\n", file) < 0) return 0;
    }
    return fputs("END\n", file) >= 0;
}

int dock_set_load(struct DockSet *set, const char *path)
{
    char previous[DOCK_PATH + 8];
    int recovered = 0, valid;
    if (!set || !path || !*path || !dock_cfg_text_valid(path, DOCK_PATH)) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno != ENOENT) return -1;
        snprintf(previous, sizeof previous, "%s.old", path);
        file = fopen(previous, "rb");
        if (!file) return errno == ENOENT ? 0 : -1;
        recovered = 1;
    }
    valid = read_set(file, &scratch_set);
    if (fclose(file)) valid = 0;
    if (valid) *set = scratch_set;
    return valid && !recovered ? 1 : -1;
}

/* An existing primary that does not parse (damaged, future version, a
 * directory) must never be replaced by a save. */
static int primary_state(const char *path)
{
    FILE *file = fopen(path, "rb");
    int valid;
    if (!file) return errno == ENOENT ? 0 : -1;
    valid = read_set(file, &scratch_set);
    if (fclose(file)) valid = 0;
    return valid ? 1 : -1;
}

int dock_set_save(const struct DockSet *set, const char *path)
{
    char temporary[DOCK_PATH + 8], previous[DOCK_PATH + 8];
    int exists, valid;
    if (!path || !*path || !dock_cfg_text_valid(path, DOCK_PATH) || !dock_set_valid(set)) return 0;
    snprintf(temporary, sizeof temporary, "%s.new", path);
    snprintf(previous, sizeof previous, "%s.old", path);
    exists = primary_state(path);
    if (exists < 0) return 0;
    if (!exists) {
        FILE *backup = fopen(previous, "rb");
        if (backup) { fclose(backup); return 0; }
        if (errno != ENOENT) return 0;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) return 0;
    valid = write_set(file, set);
    if (fclose(file)) valid = 0;
    if (!valid) { remove(temporary); return 0; }
    /* AmigaDOS Rename cannot replace a file; .old bridges the gap. */
    if (exists) {
        /* Mainline's C library fails remove() of a missing file without
         * setting errno (seen on Macaros), so ask whether it is still there. */
        if (remove(previous)) {
            FILE *still = fopen(previous, "rb");
            if (still) { fclose(still); remove(temporary); return 0; }
        }
        if (rename(path, previous)) { remove(temporary); return 0; }
    }
    if (rename(temporary, path)) {
        if (exists) rename(previous, path);
        remove(temporary);
        return 0;
    }
    if (exists) remove(previous);
    return 1;
}

/* ---- creation and migration ------------------------------------------- */

static void default_name(const struct DockSet *set, char *name)
{
    for (int n = set->count + 1; ; n++) {
        int taken = 0;
        snprintf(name, DOCK_NAME, n == 1 ? "Dock" : "Dock %d", n);
        for (int i = 0; i < set->count; i++)
            if (!strcmp(set->docks[i].name, name)) taken = 1;
        if (!taken) return;
    }
}

void dock_set_from_v1(struct DockSet *set, const struct DockModel *model,
                      const struct DockPosition *position, const struct DockPrefs *prefs)
{
    struct DockInstance *d;
    memset(set, 0, sizeof(*set));
    set->count = 1;
    set->next_id = 2;
    dock_appearance_legacy(&set->defaults);
    d = &set->docks[0];
    d->id = 1;
    strcpy(d->name, "Dock");
    dock_look_default(&d->look);
    dock_place_default(&d->place);
    if (prefs) {
        d->look.icon = prefs->icon_size;
        d->look.corner = prefs->corner;
        d->place.locked = prefs->locked;
    }
    if (position && position->valid) {
        /* 0.1 docks were placed by hand; keep them exactly where they were. */
        d->place.mode = DOCK_PLACE_FREE;
        d->place.valid = 1;
        d->place.x = position->x;
        d->place.y = position->y;
    }
    d->model = *model;
}

int dock_set_add(struct DockSet *set, const char *name)
{
    struct DockInstance *d;
    if (!set || set->count >= DOCK_MAX_DOCKS) return -1;
    d = &set->docks[set->count];
    memset(d, 0, sizeof(*d));
    d->id = set->next_id++;
    if (name && name_valid(name) && strlen(name) < DOCK_NAME) strcpy(d->name, name);
    else default_name(set, d->name);
    dock_look_default(&d->look);
    dock_place_default(&d->place);
    return set->count++;
}

int dock_set_duplicate(struct DockSet *set, int index)
{
    struct DockInstance *d;
    if (!set || index < 0 || index >= set->count || set->count >= DOCK_MAX_DOCKS) return -1;
    for(int i=0;i<set->docks[index].model.count;i++){
        const struct DockItem *it=&set->docks[index].model.items[i];
        if(it->kind==DOCK_APPLET && strlen(it->dir)>50)return -1;
    }
    d = &set->docks[set->count];
    *d = set->docks[index];
    d->id = set->next_id++;
    for(int i=0;i<d->model.count;i++)if(d->model.items[i].kind==DOCK_APPLET){
        char source[sizeof d->model.items[i].dir];strcpy(source,d->model.items[i].dir);
        snprintf(d->model.items[i].dir,sizeof d->model.items[i].dir,"copy-%d-%.50s",d->id,source);
    }
    char name[DOCK_NAME];
    snprintf(name, sizeof name, "%.*s copy", DOCK_NAME - 6, set->docks[index].name);
    strcpy(d->name, name);
    /* A copy on top of its original would look like one dock. */
    d->place.mode = DOCK_PLACE_FREE;
    d->place.valid = set->docks[index].place.valid;
    d->place.x += 40;
    d->place.y -= 40;
    return set->count++;
}

int dock_set_remove(struct DockSet *set, int index)
{
    if (!set || index < 0 || index >= set->count || set->count == 1) return 0;
    memmove(set->docks + index, set->docks + index + 1,
            (size_t)(set->count - index - 1) * sizeof(set->docks[0]));
    set->count--;
    memset(set->docks + set->count, 0, sizeof(set->docks[0]));
    return 1;
}

/* ---- moving between docks --------------------------------------------- */

int dock_set_move(struct DockSet *set, int from_dock, int from_container, int index,
                  int to_dock, int to_container, int before)
{
    struct DockItem *items, item;
    int count, group;
    if (!set || from_dock < 0 || from_dock >= set->count || to_dock < 0 || to_dock >= set->count)
        return -1;
    if (from_dock == to_dock)
        return dock_move(&set->docks[from_dock].model, from_container, index, to_container, before);
    scratch_from = set->docks[from_dock].model;
    scratch_to = set->docks[to_dock].model;
    items = dock_items(&scratch_from, from_container, &count);
    if (!items || index < 0 || index >= count) return -1;
    item = items[index];
    if (item.kind == DOCK_GROUP) {
        /* The members travel with the group into a free group slot. */
        if (to_container) return -1;
        for (group = 0; group < DOCK_MAX_GROUPS && scratch_to.groups[group].used; group++) {}
        if (group == DOCK_MAX_GROUPS || scratch_to.count == DOCK_MAX_ITEMS) return -1;
        int target_count;
        if (!dock_items(&scratch_to, 0, &target_count) || before < 0 || before > target_count) return -1;
        scratch_to.groups[group] = scratch_from.groups[item.group - 1];
        item.group = group + 1;
        memmove(scratch_to.items + before + 1, scratch_to.items + before,
                (size_t)(scratch_to.count - before) * sizeof(item));
        scratch_to.items[before] = item;
        scratch_to.count++;
    } else {
        int result = dock_insert(&scratch_to, to_container, &item, before);
        if (result <= 0) return result;
    }
    if (dock_remove(&scratch_from, from_container, index) != 1) return -1;
    set->docks[from_dock].model = scratch_from;
    set->docks[to_dock].model = scratch_to;
    return 1;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int aligned(int align, int lo, int span, int size)
{
    if (align == DOCK_ALIGN_START) return lo + 8;
    if (align == DOCK_ALIGN_END) return lo + span - size - 8;
    return lo + (span - size) / 2;
}

void dock_place_resolve(const struct DockPlace *p, int sw, int sh, int top,
                        int width, int height, int *x, int *y)
{
    int usable = sh - top;
    switch (p->mode) {
    case DOCK_PLACE_TOP: *x = aligned(p->align, 0, sw, width); *y = top + p->offset; break;
    case DOCK_PLACE_BOTTOM: *x = aligned(p->align, 0, sw, width); *y = sh - height - p->offset; break;
    case DOCK_PLACE_LEFT: *x = p->offset; *y = aligned(p->align, top, usable, height); break;
    case DOCK_PLACE_RIGHT: *x = sw - width - p->offset; *y = aligned(p->align, top, usable, height); break;
    default:
        if (p->valid) { *x = p->x; *y = p->y; }
        else { *x = (sw - width) / 2; *y = sh - height - 8; }
        break;
    }
    /* Oversized bars anchor at the top-left of the usable area. */
    *x = clampi(*x, 0, sw - width > 0 ? sw - width : 0);
    *y = clampi(*y, top, sh - height > top ? sh - height : top);
}
