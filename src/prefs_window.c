/* Nonblocking Zune preferences window; see item_editor.c for the ownership
 * pattern this follows. Strings come from prefs_strings.c. */
#include "prefs_window.h"
#include "prefs_strings.h"
#include "frame.h"
#include "layout.h"
#include "zune_input.h"

#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#define __MUIMASTER_NOLIBBASE__
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

#define new_object(class_name, ...) \
    MUI_NewObjectA((ClassID)(class_name), (struct TagItem *)(IPTR[]){ \
        AROS_PP_VARIADIC_CAST2IPTR(__VA_ARGS__) })

enum { ID_SAVE = 1, ID_CANCEL, ID_SELECT, ID_NEW, ID_DUPLICATE, ID_DELETE, ID_APPLY, ID_HELP, ID_DEPEND, ID_TAB };

/* One working copy at a time; far too large for the default AROS stack. */
static struct DockSet working;

static const char *orientations[3], *placements[6], *aligns[4], *presents[4], *text_positions[3],
    *button_modes[3], *label_modes[4], *d_themes[4], *d_backgrounds[3], *d_offon[3], *d_grips[3], *d_texts[3], *d_hovers[4], *tab_titles[5];

static void strings(void)
{
    const char **lists[] = { orientations, placements, aligns, presents, text_positions, button_modes, label_modes,
                             d_themes, d_backgrounds, d_offon, d_grips, d_texts, d_hovers, tab_titles };
    const int ids[][6] = {
        { PS_HORIZONTAL, PS_VERTICAL, -1 },
        { PS_FREE, PS_TOP, PS_BOTTOM, PS_LEFT, PS_RIGHT, -1 },
        { PS_START, PS_CENTRE, PS_END, -1 },
        { PS_ICONS, PS_TEXT_ONLY, PS_ICON_AND_TEXT, -1 },
        { PS_UNDER_ICON, PS_BESIDE_ICON, -1 },
        { PS_PLAIN, PS_PLATES, -1 },
        { PS_NONE, PS_ON_HOVER, PS_ALWAYS, -1 },
        { PS_DARK, PS_LIGHT, PS_AUTOMATIC, -1 },
        { PS_BAR, PS_NONE, -1 },
        { PS_OFF, PS_ON, -1 },
        { PS_SHOWN, PS_HIDDEN, -1 },
        { PS_SYSTEM, PS_SMOOTH, -1 },
        { PS_NONE, PS_LIFT, PS_MAGNIFY, -1 },
        { PS_TAB_PLACEMENT, PS_TAB_CONTENT, PS_TAB_APPEARANCE, PS_TAB_BEHAVIOUR, -1 },
    };
    for (unsigned l = 0; l < sizeof lists / sizeof *lists; l++) {
        int k = 0;
        for (; ids[l][k] >= 0; k++) lists[l][k] = PS(ids[l][k]);
        lists[l][k] = NULL;
    }
}

static Object *attach(struct Library *MUIMasterBase, Object *parent, Object *child)
{
    if (!child) return NULL;
    if (!DoMethod(parent, OM_ADDMEMBER, (IPTR)child)) {
        MUI_DisposeObject(child);
        return NULL;
    }
    return child;
}

static Object *label(struct Library *MUIMasterBase, Object *parent, const char *text)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Text,
        MUIA_Text_Contents, (IPTR)text, MUIA_Text_SetMax, TRUE, MUIA_Text_PreParse, (IPTR)"\33r",
        MUIA_HorizWeight, 0, TAG_DONE));
}

/* A small note under a field, in the second column. */
static int note(struct Library *MUIMasterBase, Object *parent, const char *text)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Rectangle, TAG_DONE)) &&
           attach(MUIMasterBase, parent, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
               MUIA_Font, (IPTR)MUIV_Font_Tiny, TAG_DONE));
}

static Object *slider(struct Library *MUIMasterBase, Object *parent, int minimum, int maximum)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Slider,
        MUIA_Numeric_Min, minimum, MUIA_Numeric_Max, maximum, MUIA_CycleChain, TRUE,
        /* Zune sizes a bare slider to its number; give the knob room. */
        MUIA_FixWidthTxt, (IPTR)"MMMMMMMMMMMM", TAG_DONE));
}

static Object *cycle(struct Library *MUIMasterBase, Object *parent, const char **entries)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Cycle,
        MUIA_Cycle_Entries, (IPTR)entries, MUIA_CycleChain, TRUE, TAG_DONE));
}

static Object *checkmark(struct Library *MUIMasterBase, Object *parent)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Image,
        MUIA_Image_Spec, (IPTR)MUII_CheckMark, MUIA_Image_FreeVert, TRUE,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Frame, MUIV_Frame_ImageButton,
        MUIA_Background, MUII_ButtonBack, MUIA_ShowSelState, FALSE, MUIA_CycleChain, TRUE, TAG_DONE));
}

static Object *button(struct Library *MUIMasterBase, Object *parent, const char *text,
                      Object *app, IPTR id)
{
    Object *b = attach(MUIMasterBase, parent, new_object(MUIC_Text,
        MUIA_Text_Contents, (IPTR)text, MUIA_Text_PreParse, (IPTR)"\33c",
        MUIA_Frame, MUIV_Frame_Button, MUIA_Background, MUII_ButtonBack,
        MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_CycleChain, TRUE, TAG_DONE));
    if (b && !DoMethod(b, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
                       MUIM_Application_ReturnID, id))
        return NULL;
    return b;
}

/* Choices whose change enables or disables other fields report it. */
static int depends(Object *object, Object *app)
{
    return object && DoMethod(object, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (IPTR)app, 2,
                              MUIM_Application_ReturnID, ID_DEPEND);
}

static IPTR attr_get(void *object, ULONG attribute)
{
    IPTR value = 0;
    GetAttr(attribute, (Object *)object, &value);
    return value;
}

static void set_disabled(void *object, int disabled)
{
    SetAttrs(object, MUIA_Disabled, disabled ? TRUE : FALSE, TAG_DONE);
}

/* "Inherit (value)" for each choice, from the defaults as they are now.
 * The cycles keep pointers to these texts; changing the active entry back
 * and forth redraws them. */
static void inherit_texts(struct DockPrefsWindow *w, int redraw)
{
    const struct DockAppearance *a = &w->set->defaults;
    char percent[16];
    snprintf(percent, sizeof percent, "%d %%", a->transparency);
    const char *values[7] = { d_themes[a->theme < 3 ? a->theme : 2], d_backgrounds[a->background ? 1 : 0], percent,
                              d_offon[a->glass ? 1 : 0], d_grips[a->grip ? 0 : 1], d_hovers[a->hover >= 0 && a->hover < 3 ? a->hover : 0],
                              d_offon[a->running ? 1 : 0] };
    for (int i = 0; i < 7; i++) snprintf(w->inherit[i], sizeof w->inherit[i], PS(PS_INHERIT), values[i]);
    if (!redraw) return;
    void *cycles[] = { w->theme, w->background, w->own_transparency, w->glass, w->grip, w->hover, w->running };
    for (int i = 0; i < 7; i++) {
        IPTR active = attr_get(cycles[i], MUIA_Cycle_Active);
        SetAttrs(cycles[i], MUIA_NoNotify, TRUE, MUIA_Cycle_Active, active ? 0 : 1, TAG_DONE);
        SetAttrs(cycles[i], MUIA_NoNotify, TRUE, MUIA_Cycle_Active, active, TAG_DONE);
    }
}

/* Only what applies can be changed; values of the others are kept. */
static void dependencies(struct DockPrefsWindow *w)
{
    int mode = (int)attr_get(w->placement, MUIA_Cycle_Active);
    int present = (int)attr_get(w->present, MUIA_Cycle_Active);
    int background = (int)attr_get(w->background, MUIA_Cycle_Active);
    int effective = background == 0 ? w->set->defaults.background : background - 1;
    int own = (int)attr_get(w->own_transparency, MUIA_Cycle_Active);
    set_disabled(w->align, mode == DOCK_PLACE_FREE);
    set_disabled(w->offset, mode == DOCK_PLACE_FREE);
    set_disabled(w->beside, present != 2);
    set_disabled(w->buttons, present == 0);
    set_disabled(w->own_transparency, effective == DOCK_BG_NONE);
    set_disabled(w->transparency, effective == DOCK_BG_NONE || !own);
    set_disabled(w->glass, effective == DOCK_BG_NONE ||
                 (w->current >= 0 && w->current < DOCK_MAX_DOCKS && dock_prefs_window_composed[w->current]));
    /* Kept, but not shown without a bar (the note says so). */
    set_disabled(w->hover, effective == DOCK_BG_NONE);
    int d_none = (int)attr_get(w->d_background, MUIA_Cycle_Active) == DOCK_BG_NONE;
    set_disabled(w->d_transparency, d_none);
    set_disabled(w->d_glass, d_none || dock_prefs_window_all_composed);
    /* d_hover stays: docks with a bar of their own inherit it. */
}

static void show_help(struct DockPrefsWindow *w)
{
    static const int tabs[] = { PS_HELP_PLACEMENT, PS_HELP_CONTENT, PS_HELP_APPEARANCE, PS_HELP_BEHAVIOUR };
    int tab = (int)attr_get(w->tabs, MUIA_Group_ActivePage);
    SetAttrs(w->help, MUIA_Text_Contents, (IPTR)PS(w->current < 0 ? PS_HELP_DEFAULTS : tabs[tab >= 0 && tab < 4 ? tab : 0]), TAG_DONE);
}

static void show(struct DockPrefsWindow *w)
{
    SetAttrs(w->pages, MUIA_Group_ActivePage, w->current < 0 ? 0 : 1, TAG_DONE);
    set_disabled(w->duplicate, w->current < 0);
    set_disabled(w->remove, w->current < 0);
    show_help(w);
    if (w->current < 0) { dependencies(w); return; }
    struct DockInstance *d = &w->set->docks[w->current];
    snprintf(w->header_text, sizeof w->header_text, PS(PS_DOCK_TITLE), d->name,
             placements[d->place.mode <= DOCK_PLACE_RIGHT ? d->place.mode : 0], orientations[d->look.orientation ? 1 : 0]);
    SetAttrs(w->header, MUIA_Text_Contents, (IPTR)w->header_text, TAG_DONE);
    SetAttrs(w->name, MUIA_String_Contents, (IPTR)d->name, TAG_DONE);
    SetAttrs(w->orientation, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, d->look.orientation, TAG_DONE);
    SetAttrs(w->placement, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, d->place.mode, TAG_DONE);
    SetAttrs(w->align, MUIA_Cycle_Active, d->place.align, TAG_DONE);
    SetAttrs(w->offset, MUIA_Numeric_Value, d->place.offset, TAG_DONE);
    SetAttrs(w->locked, MUIA_Selected, d->place.locked ? TRUE : FALSE, TAG_DONE);
    SetAttrs(w->icon, MUIA_Numeric_Value, d->look.icon, TAG_DONE);
    SetAttrs(w->spacing, MUIA_Numeric_Value, d->look.spacing, TAG_DONE);
    SetAttrs(w->corner, MUIA_Numeric_Value, d->look.corner, TAG_DONE);
    SetAttrs(w->labels, MUIA_Cycle_Active, d->look.labels, TAG_DONE);
    SetAttrs(w->present, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, d->look.present, TAG_DONE);
    SetAttrs(w->beside, MUIA_Cycle_Active, d->look.beside, TAG_DONE);
    SetAttrs(w->buttons, MUIA_Cycle_Active, d->look.buttons, TAG_DONE);
    SetAttrs(w->theme, MUIA_Cycle_Active, d->look.theme + 1, TAG_DONE);
    SetAttrs(w->background, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, d->look.background + 1, TAG_DONE);
    SetAttrs(w->own_transparency, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, d->look.transparency == DOCK_INHERIT ? 0 : 1, TAG_DONE);
    SetAttrs(w->transparency, MUIA_Numeric_Value,
             d->look.transparency == DOCK_INHERIT ? w->set->defaults.transparency : d->look.transparency, TAG_DONE);
    SetAttrs(w->glass, MUIA_Cycle_Active, d->look.glass + 1, TAG_DONE);
    SetAttrs(w->grip, MUIA_Cycle_Active, d->look.grip == DOCK_INHERIT ? 0 : d->look.grip ? 1 : 2, TAG_DONE);
    SetAttrs(w->hover, MUIA_Cycle_Active, d->look.hover + 1, TAG_DONE);
    SetAttrs(w->running, MUIA_Cycle_Active, d->look.running + 1, TAG_DONE);
    dependencies(w);
}

static void show_defaults(struct DockPrefsWindow *w)
{
    const struct DockAppearance *a = &w->set->defaults;
    SetAttrs(w->d_theme, MUIA_Cycle_Active, a->theme, TAG_DONE);
    SetAttrs(w->d_background, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, a->background, TAG_DONE);
    SetAttrs(w->d_transparency, MUIA_Numeric_Value, a->transparency, TAG_DONE);
    SetAttrs(w->d_glass, MUIA_Cycle_Active, a->glass ? 1 : 0, TAG_DONE);
    SetAttrs(w->d_grip, MUIA_Cycle_Active, a->grip ? 0 : 1, TAG_DONE);
    SetAttrs(w->d_text, MUIA_Cycle_Active, a->text ? 1 : 0, TAG_DONE);
    SetAttrs(w->d_hover, MUIA_Cycle_Active, a->hover, TAG_DONE);
    SetAttrs(w->d_running, MUIA_Cycle_Active, a->running ? 1 : 0, TAG_DONE);
}

static void take_defaults(struct DockPrefsWindow *w)
{
    struct DockAppearance *a = &w->set->defaults;
    a->theme = (int)attr_get(w->d_theme, MUIA_Cycle_Active);
    a->background = (int)attr_get(w->d_background, MUIA_Cycle_Active);
    a->transparency = (int)attr_get(w->d_transparency, MUIA_Numeric_Value);
    a->glass = attr_get(w->d_glass, MUIA_Cycle_Active) ? 1 : 0;
    a->grip = attr_get(w->d_grip, MUIA_Cycle_Active) ? 0 : 1;
    a->text = attr_get(w->d_text, MUIA_Cycle_Active) ? 1 : 0;
    a->hover = (int)attr_get(w->d_hover, MUIA_Cycle_Active);
    a->running = attr_get(w->d_running, MUIA_Cycle_Active) ? 1 : 0;
}

/* The fields of the shown page into the working set. */
static void take(struct DockPrefsWindow *w)
{
    take_defaults(w);
    if (w->current < 0) return;
    struct DockInstance *d = &w->set->docks[w->current];
    const char *name = (const char *)attr_get(w->name, MUIA_String_Contents);
    int mode = (int)attr_get(w->placement, MUIA_Cycle_Active);
    /* Keep the old name rather than save an empty or tabbed one. */
    if (name && *name && !strchr(name, '\t') && strlen(name) < DOCK_NAME) strcpy(d->name, name);
    d->look.orientation = (int)attr_get(w->orientation, MUIA_Cycle_Active);
    if (mode != d->place.mode && mode == DOCK_PLACE_FREE) d->place.valid = 0;
    d->place.mode = mode;
    d->place.align = (int)attr_get(w->align, MUIA_Cycle_Active);
    d->place.offset = (int)attr_get(w->offset, MUIA_Numeric_Value);
    d->place.locked = attr_get(w->locked, MUIA_Selected) ? 1 : 0;
    d->look.icon = (int)attr_get(w->icon, MUIA_Numeric_Value);
    d->look.spacing = (int)attr_get(w->spacing, MUIA_Numeric_Value);
    d->look.corner = (int)attr_get(w->corner, MUIA_Numeric_Value);
    d->look.labels = (int)attr_get(w->labels, MUIA_Cycle_Active);
    d->look.present = (int)attr_get(w->present, MUIA_Cycle_Active);
    d->look.beside = (int)attr_get(w->beside, MUIA_Cycle_Active);
    d->look.buttons = (int)attr_get(w->buttons, MUIA_Cycle_Active);
    d->look.theme = (int)attr_get(w->theme, MUIA_Cycle_Active) - 1;
    d->look.background = (int)attr_get(w->background, MUIA_Cycle_Active) - 1;
    d->look.transparency = attr_get(w->own_transparency, MUIA_Cycle_Active)
                           ? (int)attr_get(w->transparency, MUIA_Numeric_Value) : DOCK_INHERIT;
    d->look.glass = (int)attr_get(w->glass, MUIA_Cycle_Active) - 1;
    int grip = (int)attr_get(w->grip, MUIA_Cycle_Active);
    d->look.grip = grip == 0 ? DOCK_INHERIT : grip == 1 ? 1 : 0;
    d->look.hover = (int)attr_get(w->hover, MUIA_Cycle_Active) - 1;
    d->look.running = (int)attr_get(w->running, MUIA_Cycle_Active) - 1;
}

static void fill_list(struct DockPrefsWindow *w)
{
    DoMethod(w->list, MUIM_List_Clear);
    DoMethod(w->list, MUIM_List_InsertSingle, (IPTR)PS(PS_DEFAULTS_ENTRY), MUIV_List_Insert_Bottom);
    for (int i = 0; i < w->set->count; i++)
        DoMethod(w->list, MUIM_List_InsertSingle, (IPTR)w->set->docks[i].name, MUIV_List_Insert_Bottom);
    SetAttrs(w->list, MUIA_NoNotify, TRUE, MUIA_List_Active, w->current + 1, TAG_DONE);
    show(w);
}

void dock_prefs_window_close(struct DockPrefsWindow *w)
{
    struct Library *MUIMasterBase;
    if (!w) return;
    MUIMasterBase = w->mui_base;
    if (MUIMasterBase) {
        if (w->mui_window) SetAttrs(w->mui_window, MUIA_Window_Open, FALSE, TAG_DONE);
        if (w->application) MUI_DisposeObject(w->application);
        else if (w->mui_window) MUI_DisposeObject(w->mui_window);
        CloseLibrary(MUIMasterBase);
    }
    memset(w, 0, sizeof(*w));
}

/* Source line of the last failed open, for the log. */
int dock_prefs_window_failed;
unsigned char dock_prefs_window_composed[DOCK_MAX_DOCKS];
int dock_prefs_window_all_composed;
static int any_composed(void)
{
    for (int i = 0; i < DOCK_MAX_DOCKS; i++) if (dock_prefs_window_composed[i]) return 1;
    return 0;
}
#define FAIL do { dock_prefs_window_failed = __LINE__; goto fail; } while (0)

/* Two-column field group with one row per label. */
static Object *fields(struct Library *MUIMasterBase, Object *parent)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Group, MUIA_Group_Columns, 2, MUIA_VertWeight, 0, TAG_DONE));
}
static Object *page(struct Library *MUIMasterBase, Object *parent)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Group, TAG_DONE));
}
static int filler(struct Library *MUIMasterBase, Object *parent)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Rectangle, TAG_DONE)) != NULL;
}

int dock_prefs_window_open(struct DockPrefsWindow *w, struct Screen *screen,
                           const struct DockSet *set, int selected, int x, int y)
{
    struct Library *MUIMasterBase;
    Object *root, *top, *side, *docks, *right, *defaults_page, *dock_page, *f, *p, *row, *actions, *app;
    IPTR value = 0;
    if (!w || !screen || !set || w->mui_base || selected < -1 || selected >= set->count) return 0;
    memset(w, 0, sizeof(*w));
    strings();
    working = *set;
    w->set = &working;
    w->current = selected;
    inherit_texts(w, 0);
    const char *themes[] = { w->inherit[0], PS(PS_DARK), PS(PS_LIGHT), PS(PS_AUTOMATIC), NULL };
    const char *backgrounds[] = { w->inherit[1], PS(PS_BAR), PS(PS_NONE), NULL };
    const char *own[] = { w->inherit[2], PS(PS_OWN), NULL };
    const char *glasses[] = { w->inherit[3], PS(PS_OFF), PS(PS_ON), NULL };
    const char *grips[] = { w->inherit[4], PS(PS_SHOWN), PS(PS_HIDDEN), NULL };
    const char *hovers[] = { w->inherit[5], PS(PS_NONE), PS(PS_LIFT), PS(PS_MAGNIFY), NULL };
    const char *runnings[] = { w->inherit[6], PS(PS_OFF), PS(PS_ON), NULL };
    memcpy(w->themes, themes, sizeof themes);
    memcpy(w->runnings, runnings, sizeof runnings);
    memcpy(w->hovers, hovers, sizeof hovers);
    memcpy(w->backgrounds, backgrounds, sizeof backgrounds);
    memcpy(w->own, own, sizeof own);
    memcpy(w->glasses, glasses, sizeof glasses);
    memcpy(w->grips, grips, sizeof grips);
    MUIMasterBase = w->mui_base = OpenLibrary((CONST_STRPTR)"muimaster.library", 0);
    if (!MUIMasterBase) FAIL;
    app = w->application = new_object(MUIC_Application,
        MUIA_Application_Title, (IPTR)PS(PS_TITLE), MUIA_Application_Base, (IPTR)"DOCKBARPREFS", TAG_DONE);
    if (!app) FAIL;
    w->mui_window = attach(MUIMasterBase, app, new_object(MUIC_Window,
        MUIA_Window_Title, (IPTR)PS(PS_TITLE), MUIA_Window_Screen, (IPTR)screen,
        MUIA_Window_LeftEdge, x < 0 ? (IPTR)MUIV_Window_LeftEdge_Centered : (IPTR)x,
        MUIA_Window_TopEdge, y < 0 ? (IPTR)MUIV_Window_TopEdge_Centered : (IPTR)y,
        MUIA_Window_CloseGadget, TRUE, TAG_DONE));
    if (!w->mui_window) FAIL;
    root = new_object(MUIC_Group, MUIA_Group_Spacing, 6,
        MUIA_InnerLeft, 8, MUIA_InnerRight, 8, MUIA_InnerTop, 8, MUIA_InnerBottom, 8, TAG_DONE);
    if (!root) FAIL;
    SetAttrs(w->mui_window, MUIA_Window_RootObject, (IPTR)root, TAG_DONE);
    top = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    /* Left: what is being edited. */
    side = top ? attach(MUIMasterBase, top, new_object(MUIC_Group, MUIA_HorizWeight, 30, TAG_DONE)) : NULL;
    if (!side || !attach(MUIMasterBase, side, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)PS(PS_DOCKS), TAG_DONE))) FAIL;
    w->list = new_object(MUIC_List, MUIA_Frame, MUIV_Frame_InputList,
        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook, MUIV_List_DestructHook_String, MUIA_List_AdjustWidth, TRUE, TAG_DONE);
    if (!w->list) FAIL;
    docks = attach(MUIMasterBase, side, new_object(MUIC_Listview,
        MUIA_Listview_List, (IPTR)w->list, MUIA_CycleChain, TRUE, TAG_DONE));
    if (!docks) FAIL;  /* the listview disposed the list on failure */
    row = attach(MUIMasterBase, side, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !button(MUIMasterBase, row, PS(PS_NEW), app, ID_NEW) ||
        !(w->duplicate = button(MUIMasterBase, row, PS(PS_DUPLICATE), app, ID_DUPLICATE)) ||
        !(w->remove = button(MUIMasterBase, row, PS(PS_DELETE), app, ID_DELETE))) FAIL;
    /* Right: the defaults, or the selected dock in tabs. */
    right = attach(MUIMasterBase, top, new_object(MUIC_Group, MUIA_Group_PageMode, TRUE, MUIA_HorizWeight, 70, TAG_DONE));
    w->pages = right;
    defaults_page = right ? page(MUIMasterBase, right) : NULL;
    if (!defaults_page || !attach(MUIMasterBase, defaults_page, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)PS(PS_DEFAULTS_TITLE), TAG_DONE)))
        FAIL;
    f = attach(MUIMasterBase, defaults_page, new_object(MUIC_Group, MUIA_Group_Columns, 2, MUIA_Frame, MUIV_Frame_Group, MUIA_VertWeight, 0, TAG_DONE));
    if (!f ||
        !label(MUIMasterBase, f, PS(PS_COLOURS)) || !(w->d_theme = cycle(MUIMasterBase, f, d_themes)) ||
        !label(MUIMasterBase, f, PS(PS_BACKGROUND)) || !(w->d_background = cycle(MUIMasterBase, f, d_backgrounds)) ||
        !label(MUIMasterBase, f, PS(PS_TRANSPARENCY)) || !(w->d_transparency = slider(MUIMasterBase, f, 0, 100)) ||
        !label(MUIMasterBase, f, PS(PS_GLASS)) || !(w->d_glass = cycle(MUIMasterBase, f, d_offon)) ||
        (dock_prefs_window_all_composed && !note(MUIMasterBase, f, PS(PS_NOTE_GLASS))) ||
        !label(MUIMasterBase, f, PS(PS_SHOW_GRIP)) || !(w->d_grip = cycle(MUIMasterBase, f, d_grips)) ||
        !label(MUIMasterBase, f, PS(PS_TEXT)) || !(w->d_text = cycle(MUIMasterBase, f, d_texts)) ||
        !label(MUIMasterBase, f, PS(PS_HOVER)) || !(w->d_hover = cycle(MUIMasterBase, f, d_hovers)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_HOVER)) ||
        !label(MUIMasterBase, f, PS(PS_RUNNING)) || !(w->d_running = cycle(MUIMasterBase, f, d_offon)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_RUNNING)) ||
        !filler(MUIMasterBase, defaults_page)) FAIL;
    dock_page = page(MUIMasterBase, right);
    if (!dock_page || !(w->header = attach(MUIMasterBase, dock_page, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)"", TAG_DONE))))
        FAIL;
    w->tabs = attach(MUIMasterBase, dock_page, new_object(MUIC_Register, MUIA_Register_Titles, (IPTR)tab_titles, TAG_DONE));
    if (!w->tabs) FAIL;
    /* Placement */
    p = page(MUIMasterBase, w->tabs); f = p ? fields(MUIMasterBase, p) : NULL;
    if (!f ||
        !label(MUIMasterBase, f, PS(PS_NAME)) ||
        !(w->name = attach(MUIMasterBase, f, new_object(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_String_MaxLen, DOCK_NAME, MUIA_CycleChain, TRUE, TAG_DONE))) ||
        !label(MUIMasterBase, f, PS(PS_ORIENTATION)) || !(w->orientation = cycle(MUIMasterBase, f, orientations)) ||
        !label(MUIMasterBase, f, PS(PS_POSITION)) || !(w->placement = cycle(MUIMasterBase, f, placements)) ||
        !label(MUIMasterBase, f, PS(PS_ALIGNMENT)) || !(w->align = cycle(MUIMasterBase, f, aligns)) ||
        !label(MUIMasterBase, f, PS(PS_DISTANCE)) || !(w->offset = slider(MUIMasterBase, f, 0, 200)) ||
        !filler(MUIMasterBase, p)) FAIL;
    /* Content */
    p = page(MUIMasterBase, w->tabs); f = p ? fields(MUIMasterBase, p) : NULL;
    if (!f ||
        !label(MUIMasterBase, f, PS(PS_SHOW_AS)) || !(w->present = cycle(MUIMasterBase, f, presents)) ||
        !label(MUIMasterBase, f, PS(PS_TEXT_POSITION)) || !(w->beside = cycle(MUIMasterBase, f, text_positions)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_TEXT_POSITION)) ||
        !label(MUIMasterBase, f, PS(PS_TEXT_BUTTONS)) || !(w->buttons = cycle(MUIMasterBase, f, button_modes)) ||
        !label(MUIMasterBase, f, PS(PS_ICON_SIZE)) || !(w->icon = slider(MUIMasterBase, f, DOCK_ICON_MIN, DOCK_ICON_MAX)) ||
        !label(MUIMasterBase, f, PS(PS_SPACING)) || !(w->spacing = slider(MUIMasterBase, f, 0, 16)) ||
        !label(MUIMasterBase, f, PS(PS_ICON_LABELS)) || !(w->labels = cycle(MUIMasterBase, f, label_modes)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_ICON_LABELS)) ||
        !filler(MUIMasterBase, p)) FAIL;
    /* Appearance */
    p = page(MUIMasterBase, w->tabs); f = p ? fields(MUIMasterBase, p) : NULL;
    if (!f ||
        !label(MUIMasterBase, f, PS(PS_COLOURS)) || !(w->theme = cycle(MUIMasterBase, f, w->themes)) ||
        !label(MUIMasterBase, f, PS(PS_BACKGROUND)) || !(w->background = cycle(MUIMasterBase, f, w->backgrounds)) ||
        !label(MUIMasterBase, f, PS(PS_TRANSPARENCY)) ||
        !(row = attach(MUIMasterBase, f, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE))) ||
        !(w->own_transparency = cycle(MUIMasterBase, row, w->own)) ||
        !(w->transparency = slider(MUIMasterBase, row, 0, 100)) ||
        !label(MUIMasterBase, f, PS(PS_GLASS)) || !(w->glass = cycle(MUIMasterBase, f, w->glasses)) ||
        (any_composed() && !note(MUIMasterBase, f, PS(PS_NOTE_GLASS))) ||
        !label(MUIMasterBase, f, PS(PS_CORNERS)) || !(w->corner = slider(MUIMasterBase, f, 0, DOCK_CORNER_MAX)) ||
        !label(MUIMasterBase, f, PS(PS_HOVER)) || !(w->hover = cycle(MUIMasterBase, f, w->hovers)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_HOVER)) ||
        !label(MUIMasterBase, f, PS(PS_RUNNING)) || !(w->running = cycle(MUIMasterBase, f, w->runnings)) ||
        !note(MUIMasterBase, f, PS(PS_NOTE_RUNNING)) ||
        !filler(MUIMasterBase, p)) FAIL;
    /* Behaviour */
    p = page(MUIMasterBase, w->tabs); f = p ? fields(MUIMasterBase, p) : NULL;
    if (!f ||
        !label(MUIMasterBase, f, PS(PS_LOCK)) ||
        !(row = attach(MUIMasterBase, f, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE))) ||
        !(w->locked = checkmark(MUIMasterBase, row)) || !filler(MUIMasterBase, row) ||
        !label(MUIMasterBase, f, PS(PS_GRIP)) || !(w->grip = cycle(MUIMasterBase, f, w->grips)) ||
        !filler(MUIMasterBase, p)) FAIL;
    /* One line of help for what is shown, then the actions. */
    w->help = attach(MUIMasterBase, root, new_object(MUIC_Text, MUIA_Frame, MUIV_Frame_Text,
        MUIA_Background, MUII_TextBack, MUIA_Text_Contents, (IPTR)"", TAG_DONE));
    actions = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!w->help || !actions || !button(MUIMasterBase, actions, PS(PS_HELP), app, ID_HELP) ||
        !filler(MUIMasterBase, actions) ||
        !button(MUIMasterBase, actions, PS(PS_APPLY), app, ID_APPLY) ||
        !button(MUIMasterBase, actions, PS(PS_SAVE), app, ID_SAVE) ||
        !button(MUIMasterBase, actions, PS(PS_CANCEL), app, ID_CANCEL)) FAIL;
    if (!DoMethod(w->list, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_SELECT) ||
        !DoMethod(w->tabs, MUIM_Notify, MUIA_Group_ActivePage, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_TAB) ||
        !depends(w->placement, app) || !depends(w->present, app) || !depends(w->background, app) ||
        !depends(w->own_transparency, app) || !depends(w->d_background, app) ||
        !DoMethod(w->mui_window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL) ||
        !DoMethod(w->mui_window, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"esc", (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL))
        FAIL;
    show_defaults(w);
#ifdef DOCK_NO_FREETYPE
    set_disabled(w->d_text, 1);  /* no FreeType in this build: System only */
#endif
    fill_list(w);
    SetAttrs(w->mui_window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, w->mui_window, &value);
    if (!value) FAIL;
    GetAttr(MUIA_Window_Window, w->mui_window, &value);
    w->window = (struct Window *)value;
    if (!w->window) FAIL;
    return 1;
fail:
    dock_prefs_window_close(w);
    return 0;
}

int dock_prefs_window_poll(struct DockPrefsWindow *w)
{
    IPTR action;
    if (!w || !w->window) return -1;
    if (w->result) return w->result;
    action = dock_zune_input(w->application, &w->signals, &w->again);
    switch (action) {
    case ID_SELECT: {
        int next = (int)attr_get(w->list, MUIA_List_Active) - 1;
        if (next >= -1 && next < w->set->count && next != w->current) {
            take(w);
            inherit_texts(w, 1);
            w->current = next;
            fill_list(w);  /* names may have changed */
        }
        break;
    }
    case ID_NEW: case ID_DUPLICATE: {
        int made;
        take(w);
        if (action == ID_DUPLICATE && w->current < 0) break;
        made = action == ID_NEW ? dock_set_add(w->set, NULL) : dock_set_duplicate(w->set, w->current);
        if (made >= 0) { w->current = made; fill_list(w); }
        else DisplayBeep(NULL);
        break;
    }
    case ID_DELETE:
        if (w->current >= 0 && dock_set_remove(w->set, w->current)) {
            if (w->current >= w->set->count) w->current = w->set->count - 1;
            fill_list(w);
        } else DisplayBeep(NULL);  /* the last dock stays */
        break;
    case ID_DEPEND:
        take_defaults(w);
        if (w->current < 0) inherit_texts(w, 1);
        dependencies(w);
        break;
    case ID_TAB:
        show_help(w);
        break;
    case ID_APPLY: take(w); inherit_texts(w, 1); show(w); w->result = DOCK_PREFS_APPLY; break;
    case ID_SAVE: take(w); w->result = DOCK_PREFS_SAVE; break;
    case ID_HELP: w->result = DOCK_PREFS_HELP; break;
    case ID_CANCEL: w->result = DOCK_PREFS_CANCEL; break;
    default:
        if (action == (IPTR)MUIV_Application_ReturnID_Quit) w->result = DOCK_PREFS_CANCEL;
        break;
    }
    return w->result;
}

void dock_prefs_window_keep(struct DockPrefsWindow *w)
{
    if (w) w->result = 0;
}

void dock_prefs_window_message(struct DockPrefsWindow *w, const char *text)
{
    if (w && w->help) SetAttrs(w->help, MUIA_Text_Contents, (IPTR)text, TAG_DONE);
}
