/* Nonblocking native Zune item editor. MUI owns its entire event queue. */
#include "group_panel.h"
#include "widget.h"

#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/view.h>
#define __MUIMASTER_NOLIBBASE__
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "zune_input.h"

/* Use the A entry point so construction uses our explicit library reference,
 * including SDKs that implement variadic MUI_NewObject through a global stub. */
#define new_object(class_name, ...) \
    MUI_NewObjectA((ClassID)(class_name), (struct TagItem *)(IPTR[]){ \
        AROS_PP_VARIADIC_CAST2IPTR(__VA_ARGS__) })

/* Each editor owns its library reference; calls use a local MUIMasterBase. */
enum { EDIT_SAVE = 1, EDIT_REMOVE = 2, EDIT_CANCEL = 3, EDIT_TYPE, EDIT_DROP, EDIT_HELP, EDIT_HELP_CLOSE, EDIT_PICK_COMMAND, EDIT_PICK_DIR, EDIT_PICK_ICON };

static Object *attach(struct Library *MUIMasterBase, Object *parent, Object *child)
{
    if (!child) return NULL;
    if (!DoMethod(parent, OM_ADDMEMBER, (IPTR)child)) {
        MUI_DisposeObject(child);
        return NULL;
    }
    return child;
}

static Object *text_object(struct Library *MUIMasterBase, const char *text)
{
    return new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
                        TAG_DONE);
}

/* Only form labels have a fixed maximum width. Explanatory text in the
 * outer group stays flexible so it does not cap the whole editor's width. */
static Object *field_label(struct Library *MUIMasterBase, const char *text)
{
    return new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
        MUIA_Text_SetMax, TRUE, MUIA_HorizWeight, 0, TAG_DONE);
}

static Object *button(struct Library *MUIMasterBase, Object *parent,
                      const char *text)
{
    return attach(MUIMasterBase, parent,
        new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
                      MUIA_Text_PreParse, (IPTR)"\33c",
                      MUIA_Frame, MUIV_Frame_Button,
                      MUIA_Background, MUII_ButtonBack,
                      MUIA_InputMode, MUIV_InputMode_RelVerify,
                      MUIA_CycleChain, TRUE, TAG_DONE));
}

static Object *checkmark(struct Library *MUIMasterBase, Object *parent, int selected)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Image,
        MUIA_Image_Spec, (IPTR)MUII_CheckMark, MUIA_Image_FreeVert, TRUE,
        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Frame, MUIV_Frame_ImageButton,
        MUIA_Background, MUII_ButtonBack, MUIA_ShowSelState, FALSE,
        MUIA_Selected, selected ? TRUE : FALSE, MUIA_CycleChain, TRUE, TAG_DONE));
}

static const char *presentations[] = { "Use dock setting", "Icon", "Text", "Icon and text", NULL };
static const char *command_types[] = { "Command", "Script file", NULL };
static const char *program_drops[] = { "Add a shortcut", "Open the files", NULL };
static const char *command_drops[] = { "Add a shortcut", "Run with the files", NULL };
static const char *drawer_opens[] = { "Wanderer window", "Panel", NULL };

static Object *cycle(struct Library *MUIMasterBase, Object *parent, const char **entries, int active)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Cycle, MUIA_Cycle_Entries, (IPTR)entries,
        MUIA_Cycle_Active, active, MUIA_CycleChain, TRUE, TAG_DONE));
}

static int notify_action(Object *object, Object *app, IPTR action)
{
    return DoMethod(object, MUIM_Notify, MUIA_Pressed, FALSE,
                    (IPTR)app, 2, MUIM_Application_ReturnID, action) != 0;
}

void dock_editor_close(struct DockEditor *editor)
{
    struct Library *MUIMasterBase;
    if (!editor) return;
    MUIMasterBase = editor->mui_base;
    if (MUIMasterBase) {
        if (editor->help_window)
            SetAttrs(editor->help_window, MUIA_Window_Open, FALSE, TAG_DONE);
        if (editor->mui_window)
            SetAttrs(editor->mui_window, MUIA_Window_Open, FALSE, TAG_DONE);
        if (editor->application)
            MUI_DisposeObject(editor->application);
        /* Only needed if construction failed before the window was attached. */
        else if (editor->mui_window)
            MUI_DisposeObject(editor->mui_window);
        CloseLibrary(MUIMasterBase);
    }
    if (editor->screen && editor->hint_pen >= 0 && editor->hint_style[0])
        ReleasePen(editor->screen->ViewPort.ColorMap, (ULONG)editor->hint_pen);
    memset(editor, 0, sizeof(*editor));
}

static const char command_help[] =
    "\33bCommand\33n\n"
    "An AmigaDOS command line, as you would type it in a Shell, for example Dir SYS:Prefs ALL. "
    "It starts in the working directory (blank: SYS:).\n\n"
    "\33bScript file\33n\n"
    "A file of Shell commands, run with Execute. A name without a disk or drawer is looked "
    "for in the working directory.\n\n"
    "\33bOpen a Shell window\33n\n"
    "Shows what the command prints, and its errors, in a console window that stays open until "
    "you close it. Without it nothing is shown, not even an error.\n\n"
    "\33bFiles dropped on the item\33n\n"
    "With On drop set to Run with the files, write {files} once in the command line, as a word "
    "of its own and not in quotes, where the files should go, for example List {files}. "
    "Dockbar puts each file in quotes itself, so names with spaces work.\n\n"
    "A script takes the files with .KEY FILES/M and uses <FILES>, not in quotes:\n\n"
    "    .KEY FILES/M\n"
    "    List <FILES>\n\n"
    "\33bIcon\33n\n"
    "A .info or .svg file. Default: the Shell's icon.";

static Object *popfile(struct Library *MUIMasterBase, Object *parent, const char *contents,
                       int max, int drawers, const char *title, Object **string, Object **button)
{
    *string = new_object(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, TRUE,
        MUIA_String_MaxLen, max, MUIA_String_Contents, (IPTR)contents, TAG_DONE);
    Object *b = MUI_MakeObject(MUIO_PopButton, (IPTR)(drawers ? MUII_PopDrawer : MUII_PopFile));
    if (!*string || !b) {
        if (*string) MUI_DisposeObject(*string);
        if (b) MUI_DisposeObject(b);
        *string = NULL;
        return NULL;
    }
    SetAttrs(b, MUIA_CycleChain, TRUE, TAG_DONE);
    if (button) *button = b;
    return attach(MUIMasterBase, parent, new_object(MUIC_Popasl,
        MUIA_Popstring_String, (IPTR)*string, MUIA_Popstring_Button, (IPTR)b,
        MUIA_Popasl_Type, ASL_FileRequest, ASLFR_TitleText, (IPTR)title,
        ASLFR_DrawersOnly, drawers ? TRUE : FALSE, TAG_DONE));
}

/* The second column of a row: the field with its hint or error line under
 * it, in a group of their own, so that a hidden line cannot shift the grid
 * (Zune leaves hidden children out of the columns). */
static Object *cell(struct Library *MUIMasterBase, Object *fields)
{
    return attach(MUIMasterBase, fields, new_object(MUIC_Group, MUIA_Group_Spacing, 1, TAG_DONE));
}

/* A hint in the normal font, set apart by its pen; an error line (style
 * NULL) in the text pen and bold, so it is not taken for a hint. */
static Object *note(struct Library *MUIMasterBase, Object *cell, const char *style, const char *text, int shown)
{
    return attach(MUIMasterBase, cell, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
        MUIA_Text_PreParse, (IPTR)(style ? style : "\33b"), MUIA_ShowMe, shown ? TRUE : FALSE, TAG_DONE));
}

/* What On drop asks of the command field. A script cannot take {files}:
 * the files reach it as its arguments. */
static const char *drop_hint_text(int script, int drop)
{
    if (drop != DOCK_DROP_OPEN) return NULL;
    return script ? "The script must accept dropped files. See Help for an example."
                  : "Use {files} where the dropped files should go.";
}

static void hint_pen(struct DockEditor *e)
{
    struct DrawInfo *dri = GetScreenDrawInfo(e->screen);
    ULONG text[3], back[3];
    e->hint_pen = -1;
    e->hint_style[0] = 0;
    if (!dri) return;
    GetRGB32(e->screen->ViewPort.ColorMap, dri->dri_Pens[TEXTPEN], 1, text);
    GetRGB32(e->screen->ViewPort.ColorMap, dri->dri_Pens[BACKGROUNDPEN], 1, back);
    FreeScreenDrawInfo(e->screen, dri);
    e->hint_pen = ObtainBestPen(e->screen->ViewPort.ColorMap,
        (text[0] >> 1) + (back[0] >> 1), (text[1] >> 1) + (back[1] >> 1), (text[2] >> 1) + (back[2] >> 1),
        OBP_Precision, PRECISION_GUI, TAG_DONE);
    if (e->hint_pen >= 0) snprintf(e->hint_style, sizeof e->hint_style, "\33P[%ld]", e->hint_pen);
}

/* Command / Script: a short form; what a choice means shows only when it is
 * chosen, details are in Help. */
static int command_form(struct Library *MUIMasterBase, struct DockEditor *e, Object *root,
                        const struct DockItem *item, int presentation)
{
    int script = (e->run & DOCK_RUN_SCRIPT) != 0;
    Object *c, *string = NULL, *row;
    Object *fields = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Columns, 2,
        MUIA_Group_VertSpacing, 6, TAG_DONE));
    if (!fields || !attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Name")) || !(c = cell(MUIMasterBase, fields)))
        return 0;
    e->name_object = attach(MUIMasterBase, c, new_object(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        MUIA_CycleChain, TRUE, MUIA_String_MaxLen, DOCK_LABEL, MUIA_String_Contents, (IPTR)e->label, TAG_DONE));
    if (!e->name_object || !(e->name_error = note(MUIMasterBase, c, NULL, "", 0))) return 0;
    if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Type")) ||
        !(e->type_object = cycle(MUIMasterBase, fields, command_types, script)) ||
        !(e->command_label = attach(MUIMasterBase, fields, field_label(MUIMasterBase, script ? "Script file" : "Command"))) ||
        !(c = cell(MUIMasterBase, fields)))
        return 0;
    /* One field for both: a command line, or a script with its chooser. */
    if (!(e->command_pop = popfile(MUIMasterBase, c, e->command, DOCK_PATH, 0, "Choose a script file", &string, (Object **)&e->command_button)))
        return 0;
    e->command_object = string;
    SetAttrs(e->command_button, MUIA_ShowMe, script ? TRUE : FALSE, TAG_DONE);
    if (!(e->command_error = note(MUIMasterBase, c, NULL, "", 0))) return 0;
    if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Working directory")) || !(c = cell(MUIMasterBase, fields)) ||
        !(e->dir_pop = popfile(MUIMasterBase, c, e->dir, DOCK_DIR, 1, "Choose the working directory", &string, NULL))) return 0;
    e->dir_object = string;
    if (!note(MUIMasterBase, c, e->hint_style, "Default: SYS:", 1)) return 0;
    if (!attach(MUIMasterBase, fields, new_object(MUIC_Rectangle, MUIA_HorizWeight, 0, TAG_DONE))) return 0;
    row = attach(MUIMasterBase, fields, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !(e->shell_object = checkmark(MUIMasterBase, row, e->run & DOCK_RUN_SHELL)) ||
        !attach(MUIMasterBase, row, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)"Open a Shell window", TAG_DONE)) ||
        !attach(MUIMasterBase, row, new_object(MUIC_Rectangle, TAG_DONE))) return 0;
    if (presentation) {
        if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "On drop")) || !(c = cell(MUIMasterBase, fields)) ||
            !(e->drop_object = cycle(MUIMasterBase, c, command_drops, e->on_drop)) ||
            !(e->drop_hint = note(MUIMasterBase, c, e->hint_style, drop_hint_text(script, e->on_drop) ? drop_hint_text(script, e->on_drop) : "",
                                  drop_hint_text(script, e->on_drop) != NULL))) return 0;
    }
    /* Appearance in the same grid, so both parts line up. */
    if (!attach(MUIMasterBase, fields, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)"\33bAppearance",
            MUIA_HorizWeight, 0, TAG_DONE)) ||
        !attach(MUIMasterBase, fields, new_object(MUIC_Rectangle, TAG_DONE))) return 0;
    if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Icon")) || !(c = cell(MUIMasterBase, fields)) ||
        !(e->icon_pop = popfile(MUIMasterBase, c, e->icon, DOCK_PATH, 0, "Choose an icon (.info or .svg)", &string, NULL))) return 0;
    e->icon_object = string;
    if (!note(MUIMasterBase, c, e->hint_style, "Default: Shell icon", 1)) return 0;
    if (presentation && dock_kind_shown(item->kind) &&
        (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Show as")) ||
         !(e->present_object = cycle(MUIMasterBase, fields, presentations, e->present)))) return 0;
    return 1;
}

static void show_note(struct Library *MUIMasterBase, struct DockEditor *e, void *note, const char *text)
{
    (void)MUIMasterBase;
    if (!note) return;
    DoMethod(e->form_group, MUIM_Group_InitChange);
    SetAttrs(note, MUIA_Text_Contents, (IPTR)(text ? text : ""), MUIA_ShowMe, text && text[0] ? TRUE : FALSE, TAG_DONE);
    DoMethod(e->form_group, MUIM_Group_ExitChange);
}

void dock_editor_refuse(struct DockEditor *e, int field, const char *message)
{
    struct Library *MUIMasterBase;
    if (!e || !e->window) return;
    MUIMasterBase = e->mui_base;
    show_note(MUIMasterBase, e, e->name_error, field == 0 ? message : NULL);
    show_note(MUIMasterBase, e, e->command_error, field == 1 ? message : NULL);
    SetAttrs(e->mui_window, MUIA_Window_ActiveObject, (IPTR)(field == 1 && e->command_object ? e->command_object : e->name_object), TAG_DONE);
    e->result = 0;
}

/* Source line of the last failed open, for the log. */
int dock_editor_failed;
#define EDITOR_FAIL do { dock_editor_failed = __LINE__; goto fail; } while (0)

int dock_editor_open(struct DockEditor *editor, struct Screen *screen,
                     const struct DockItem *item, int x, int y, int presentation)
{
    struct Library *MUIMasterBase;
    Object *root, *fields, *actions, *save, *cancel, *remove;
    IPTR value = 0;
    if (!editor || !screen || !item || editor->mui_base || editor->window)
        return 0;
    memset(editor, 0, sizeof(*editor));
    editor->screen = screen;
    hint_pen(editor);
    memcpy(editor->label, item->label, sizeof(editor->label));
    editor->label[sizeof(editor->label)-1] = 0;
    memcpy(editor->icon, item->icon, sizeof(editor->icon));
    editor->icon[sizeof(editor->icon)-1] = 0;
    editor->kind = item->kind;
    editor->present = item->present;
    snprintf(editor->command, sizeof editor->command, "%s", item->path);
    snprintf(editor->dir, sizeof editor->dir, "%s", item->dir);
    editor->run = item->run;
    editor->on_drop = item->on_drop;
    editor->open = item->open;
    if (item->kind == DOCK_CLOCK) {
        struct DockWidget clock;
        if (dock_widget_start(&clock, item->path)) editor->clock_options = clock.options;
        dock_widget_stop(&clock);
    }
    MUIMasterBase = editor->mui_base = OpenLibrary((CONST_STRPTR)"muimaster.library", 0);
    if (!MUIMasterBase) EDITOR_FAIL;
    editor->application = new_object(MUIC_Application,
        MUIA_Application_Title, (IPTR)"Dockbar item editor",
        MUIA_Application_Base, (IPTR)"DOCKBAREDITOR", TAG_DONE);
    if (!editor->application) EDITOR_FAIL;
    /* Build without nested constructors: every allocated child immediately
     * gains an owner, so allocation failure can dispose the application tree. */
    editor->mui_window = attach(MUIMasterBase, editor->application,
        new_object(MUIC_Window,
            MUIA_Window_Title, (IPTR)(item->kind == DOCK_COMMAND ? (item->path[0] ? "Edit command" : "Add command") :
                item->kind == DOCK_GROUP ? "Edit group" :
                item->kind == DOCK_CLOCK ? "Edit clock" : item->kind == DOCK_SEPARATOR ? "Separator" :
                item->kind == DOCK_SPACER ? "Space" : "Edit shortcut"),
            MUIA_Window_Screen, (IPTR)screen,
            MUIA_Window_LeftEdge, (IPTR)x,
            MUIA_Window_TopEdge, (IPTR)y,
            MUIA_Window_Width, 540,
            MUIA_Window_CloseGadget, TRUE, TAG_DONE));
    if (!editor->mui_window) EDITOR_FAIL;
    root = new_object(MUIC_Group,
        MUIA_Group_Spacing, 10,
        MUIA_InnerLeft, 12, MUIA_InnerRight, 12,
        MUIA_InnerTop, 12, MUIA_InnerBottom, 12, TAG_DONE);
    if (!root) EDITOR_FAIL;
    SetAttrs(editor->mui_window, MUIA_Window_RootObject, (IPTR)root, TAG_DONE);
    editor->form_group = root;
    if (item->kind == DOCK_COMMAND) {
        if (!command_form(MUIMasterBase, editor, root, item, presentation)) EDITOR_FAIL;
    } else if (item->kind == DOCK_SEPARATOR || item->kind == DOCK_SPACER) {
        if (!attach(MUIMasterBase, root, text_object(MUIMasterBase,
                "Remove takes this item off the dock."))) EDITOR_FAIL;
    } else {
    fields = attach(MUIMasterBase, root,
        new_object(MUIC_Group, MUIA_Group_Columns, 2, TAG_DONE));
    if (!fields || !attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Name")))
        EDITOR_FAIL;
    editor->name_object = attach(MUIMasterBase, fields,
        new_object(MUIC_String, MUIA_Frame, MUIV_Frame_String,
            MUIA_CycleChain, TRUE,
            MUIA_String_MaxLen, DOCK_LABEL,
            MUIA_String_Contents, (IPTR)editor->label, TAG_DONE));
    if (!editor->name_object) EDITOR_FAIL;
    if (item->kind == DOCK_CLOCK) {
        if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Show seconds")) ||
            !(editor->seconds_object = checkmark(MUIMasterBase, fields,
                  editor->clock_options & DOCK_CLOCK_SECONDS)) ||
            !attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Show date")) ||
            !(editor->date_object = checkmark(MUIMasterBase, fields,
                  editor->clock_options & DOCK_CLOCK_DATE))) EDITOR_FAIL;
    } else {
        /* Files dropped on the middle of this item (programs and commands on
         * the dock), and what a click on a drawer shows. */
        if (presentation && item->kind == DOCK_APP &&
            (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "On drop")) ||
             !(editor->drop_object = cycle(MUIMasterBase, fields,
                   item->kind == DOCK_APP ? program_drops : command_drops, editor->on_drop)))) EDITOR_FAIL;
        if (item->kind == DOCK_DRAWER &&
            (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Open on click")) ||
             !(editor->open_object = cycle(MUIMasterBase, fields, drawer_opens, editor->open)))) EDITOR_FAIL;
        if (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Icon"))) EDITOR_FAIL;
        editor->icon_object = attach(MUIMasterBase, fields,
            new_object(MUIC_String, MUIA_Frame, MUIV_Frame_String,
                MUIA_CycleChain, TRUE,
                MUIA_String_MaxLen, DOCK_PATH,
                MUIA_String_Contents, (IPTR)editor->icon, TAG_DONE));
        if (!editor->icon_object) EDITOR_FAIL;
        if (presentation && dock_kind_shown(item->kind) &&
            (!attach(MUIMasterBase, fields, field_label(MUIMasterBase, "Show as")) ||
             !(editor->present_object = cycle(MUIMasterBase, fields, presentations, editor->present)))) EDITOR_FAIL;
        if (!attach(MUIMasterBase, root,
            text_object(MUIMasterBase, item->kind == DOCK_DRAWER
                ? "Panel: the drawer's contents beside the dock (nothing is copied into it).\n"
                  ".info or .svg path; blank = automatic"
                : ".info or .svg path; blank = automatic"))) EDITOR_FAIL;
    }
    if (item->kind == DOCK_GROUP && !attach(MUIMasterBase, root,
        text_object(MUIMasterBase, "Remove deletes this group and its shortcuts only."))) EDITOR_FAIL;
    }
    if (item->kind == DOCK_COMMAND) {
        /* Remove on its own at the left, away from Save; none while adding. */
        Object *help;
        actions = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
        if (!actions) EDITOR_FAIL;
        remove = item->path[0] ? button(MUIMasterBase, actions, "Remove") : NULL;
        help = button(MUIMasterBase, actions, "Help");
        if (!attach(MUIMasterBase, actions, new_object(MUIC_Rectangle, TAG_DONE))) EDITOR_FAIL;
        cancel = button(MUIMasterBase, actions, "Cancel");
        save = button(MUIMasterBase, actions, "Save");
        if ((item->path[0] && !remove) || !help || !cancel || !save ||
            !notify_action(help, editor->application, EDIT_HELP) ||
            !DoMethod(editor->command_pop, MUIM_Notify, MUIA_Popasl_Active, FALSE,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_PICK_COMMAND) ||
            !DoMethod(editor->dir_pop, MUIM_Notify, MUIA_Popasl_Active, FALSE,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_PICK_DIR) ||
            !DoMethod(editor->icon_pop, MUIM_Notify, MUIA_Popasl_Active, FALSE,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_PICK_ICON) ||
            !DoMethod(editor->type_object, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_TYPE) ||
            (editor->drop_object && !DoMethod(editor->drop_object, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_DROP))) EDITOR_FAIL;
        /* Help: details that the form leaves out, in a window of its own. */
        Object *hroot, *htext, *hlist, *hrow, *hclose;
        editor->help_window = attach(MUIMasterBase, editor->application, new_object(MUIC_Window,
            MUIA_Window_Title, (IPTR)"Command help", MUIA_Window_Screen, (IPTR)screen,
            MUIA_Window_Width, MUIV_Window_Width_Visible(45), MUIA_Window_Height, MUIV_Window_Height_Visible(55),
            MUIA_Window_CloseGadget, TRUE, TAG_DONE));
        hroot = editor->help_window ? new_object(MUIC_Group, MUIA_Group_Spacing, 8, TAG_DONE) : NULL;
        if (!hroot) EDITOR_FAIL;
        SetAttrs(editor->help_window, MUIA_Window_RootObject, (IPTR)hroot, TAG_DONE);
        htext = new_object(MUIC_Floattext, MUIA_Frame, MUIV_Frame_ReadList, MUIA_Floattext_Text, (IPTR)command_help, TAG_DONE);
        hlist = htext ? attach(MUIMasterBase, hroot, new_object(MUIC_Listview, MUIA_Listview_List, (IPTR)htext,
            MUIA_Listview_Input, FALSE, TAG_DONE)) : NULL;
        hrow = hlist ? attach(MUIMasterBase, hroot, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE)) : NULL;
        if (!hrow || !attach(MUIMasterBase, hrow, new_object(MUIC_Rectangle, TAG_DONE)) ||
            !(hclose = button(MUIMasterBase, hrow, "Close")) ||
            !notify_action(hclose, editor->application, EDIT_HELP_CLOSE) ||
            !DoMethod(editor->help_window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_HELP_CLOSE)) EDITOR_FAIL;
    } else {
        actions = attach(MUIMasterBase, root,
            new_object(MUIC_Group, MUIA_Group_Horiz, TRUE,
                MUIA_Group_SameWidth, TRUE, TAG_DONE));
        if (!actions) EDITOR_FAIL;
        remove = button(MUIMasterBase, actions, "Remove");
        cancel = button(MUIMasterBase, actions, "Cancel");
        /* A separator or space has nothing to save. */
        save = editor->name_object ? button(MUIMasterBase, actions, "Save") : NULL;
        if (!remove || !cancel || (editor->name_object && !save)) EDITOR_FAIL;
    }
    if ((save && !notify_action(save, editor->application, EDIT_SAVE)) ||
        (remove && !notify_action(remove, editor->application, EDIT_REMOVE)) ||
        !notify_action(cancel, editor->application, EDIT_CANCEL) ||
        !DoMethod(editor->mui_window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
            (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_CANCEL) ||
        !DoMethod(editor->mui_window, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"esc",
            (IPTR)editor->application, 2, MUIM_Application_ReturnID, EDIT_CANCEL)) EDITOR_FAIL;
    SetAttrs(editor->mui_window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, editor->mui_window, &value);
    if (!value) EDITOR_FAIL;
    GetAttr(MUIA_Window_Window, editor->mui_window, &value);
    editor->window = (struct Window *)value;
    if (!editor->window) EDITOR_FAIL;
    if (editor->name_object)
        SetAttrs(editor->mui_window, MUIA_Window_ActiveObject,
                 (IPTR)editor->name_object, TAG_DONE);
    return 1;
fail:
    dock_editor_close(editor);
    return 0;
}

int dock_editor_poll(struct DockEditor *editor)
{
    IPTR action, text = 0;
    struct Library *MUIMasterBase;
    if (!editor || !editor->window) return -1;
    MUIMasterBase = editor->mui_base;
    if (editor->result) return editor->result;
    /* Main keeps servicing the dock and panel; never GetMsg this window. */
    action = dock_zune_input(editor->application, &editor->signals, &editor->again);
    if (action == EDIT_TYPE) {
        /* The field follows the type: a script gets its file chooser. */
        IPTR script = 0;
        GetAttr(MUIA_Cycle_Active, editor->type_object, &script);
        DoMethod(editor->form_group, MUIM_Group_InitChange);
        SetAttrs(editor->command_label, MUIA_Text_Contents, (IPTR)(script ? "Script file" : "Command"), TAG_DONE);
        SetAttrs(editor->command_button, MUIA_ShowMe, script ? TRUE : FALSE, TAG_DONE);
        DoMethod(editor->form_group, MUIM_Group_ExitChange);
    }
    if (action == EDIT_TYPE || action == EDIT_DROP) {
        IPTR script = 0, drop = 0;
        if (!editor->drop_object) return 0;
        GetAttr(MUIA_Cycle_Active, editor->type_object, &script);
        GetAttr(MUIA_Cycle_Active, editor->drop_object, &drop);
        show_note(MUIMasterBase, editor, editor->drop_hint, drop_hint_text(script != 0, (int)drop));
        return 0;
    }
    if (action == EDIT_PICK_COMMAND || action == EDIT_PICK_DIR || action == EDIT_PICK_ICON) {
        /* The requester answers relative to Dockbar's current directory when
         * the field was empty; the item must not depend on that. A name typed
         * by hand stays as typed (a script name is looked for in the working
         * directory). */
        Object *field = action == EDIT_PICK_COMMAND ? editor->command_object
                      : action == EDIT_PICK_DIR ? editor->dir_object : editor->icon_object;
        IPTR now = 0;
        char full[DOCK_PATH];
        GetAttr(MUIA_String_Contents, field, &now);
        if (now && ((const char *)now)[0] && !strchr((const char *)now, ':')) {
            BPTR lock = Lock((CONST_STRPTR)now, SHARED_LOCK);
            if (lock) {
                if (NameFromLock(lock, (STRPTR)full, sizeof full))
                    SetAttrs(field, MUIA_String_Contents, (IPTR)full, TAG_DONE);
                UnLock(lock);
            }
        }
        if (action == EDIT_PICK_COMMAND) show_note(MUIMasterBase, editor, editor->command_error, NULL);
        return 0;
    }
    if (action == EDIT_HELP || action == EDIT_HELP_CLOSE) {
        SetAttrs(editor->help_window, MUIA_Window_Open, action == EDIT_HELP ? TRUE : FALSE, TAG_DONE);
        return 0;
    }
    if (action == EDIT_SAVE) {
        GetAttr(MUIA_String_Contents, editor->name_object, &text);
        if (!text) return editor->result = -1;
        strncpy(editor->label, (const char *)text, sizeof(editor->label)-1);
        editor->label[sizeof(editor->label)-1] = 0;
        if (editor->icon_object) {
            text = 0;
            GetAttr(MUIA_String_Contents, editor->icon_object, &text);
            if (!text) return editor->result = -1;
            strncpy(editor->icon, (const char *)text, sizeof(editor->icon)-1);
            editor->icon[sizeof(editor->icon)-1] = 0;
        }
        if (editor->present_object) {
            IPTR active = 0;
            GetAttr(MUIA_Cycle_Active, editor->present_object, &active);
            editor->present = (int)active;
        }
        if (editor->drop_object) {
            IPTR active = 0;
            GetAttr(MUIA_Cycle_Active, editor->drop_object, &active);
            editor->on_drop = (int)active;
        }
        if (editor->open_object) {
            IPTR active = 0;
            GetAttr(MUIA_Cycle_Active, editor->open_object, &active);
            editor->open = (int)active;
        }
        if (editor->command_object) {
            IPTR on = 0, type = 0;
            text = 0;
            GetAttr(MUIA_String_Contents, editor->command_object, &text);
            snprintf(editor->command, sizeof editor->command, "%s", text ? (const char *)text : "");
            text = 0;
            GetAttr(MUIA_String_Contents, editor->dir_object, &text);
            snprintf(editor->dir, sizeof editor->dir, "%s", text ? (const char *)text : "");
            GetAttr(MUIA_Selected, editor->shell_object, &on);
            GetAttr(MUIA_Cycle_Active, editor->type_object, &type);
            editor->run = (on ? DOCK_RUN_SHELL : 0) | (type ? DOCK_RUN_SCRIPT : 0);
        }
        if (editor->seconds_object) {
            IPTR on = 0;
            editor->clock_options = 0;
            GetAttr(MUIA_Selected, editor->seconds_object, &on);
            if (on) editor->clock_options |= DOCK_CLOCK_SECONDS;
            on = 0;
            GetAttr(MUIA_Selected, editor->date_object, &on);
            if (on) editor->clock_options |= DOCK_CLOCK_DATE;
        }
        editor->result = 1;
    } else if (action == EDIT_REMOVE) editor->result = 2;
    else if (action == EDIT_CANCEL || action == (IPTR)MUIV_Application_ReturnID_Quit)
        editor->result = -1;
    return editor->result;
}
