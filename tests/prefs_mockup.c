/* Development mockup, not shipped: the proposed layout of Dockbar's
 * Preferences, for review before it is implemented. Nothing is read or
 * saved; values are a sample dock. ENV:mockview selects what is shown first:
 * "defaults", or "placement", "content", "appearance", "behaviour". The
 * dock list on the left switches between the defaults view and a dock.
 * All user-visible strings are in one table (S_...), as the real window
 * will have them for later translation. */
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>

struct Library *MUIMasterBase;

enum {
    S_TITLE, S_DOCKS, S_DEFAULTS_ENTRY, S_NEW, S_DUPLICATE, S_DELETE,
    S_PLACEMENT, S_CONTENT, S_APPEARANCE, S_BEHAVIOUR,
    S_NAME, S_ORIENTATION, S_POSITION, S_ALIGNMENT, S_DISTANCE,
    S_SHOW_AS, S_TEXT_POSITION, S_TEXT_BUTTONS, S_ICON_SIZE, S_SPACING, S_LABELS,
    S_COLOURS, S_BACKGROUND, S_TRANSPARENCY, S_GLASS, S_SHADOW, S_CORNERS,
    S_LOCK, S_GRIP, S_TEXT,
    S_HELP_PLACEMENT, S_HELP_CONTENT, S_HELP_APPEARANCE, S_HELP_BEHAVIOUR, S_HELP_DEFAULTS,
    S_HELP, S_APPLY, S_SAVE, S_CANCEL, S_DEFAULTS_TITLE, S_DOCK_TITLE,
    S_LABELS_NOTE, S_TEXTPOS_NOTE, S_COUNT
};
static const char *S[S_COUNT] = {
    "Dockbar Preferences", "Docks", "\33bDefaults for all docks", "New", "Duplicate", "Delete",
    "Placement", "Content", "Appearance", "Behaviour",
    "Name", "Orientation", "Position", "Alignment", "Distance from edge",
    "Show items as", "Text position", "Text buttons", "Icon size", "Spacing", "Icon labels",
    "Colours", "Background", "Transparency %", "Frosted glass", "Drop shadow", "Corner radius",
    "Lock position and contents", "Grip", "Text",
    "Where the dock sits. Alignment and distance apply at a screen edge.",
    "What items look like. Icon labels apply only to items shown as icons.",
    "Inherit uses the value set under Defaults for all docks (shown in brackets).",
    "A locked dock cannot be moved or changed; launching still works.\nWith the grip hidden, Ctrl-click a dock or start Dockbar again.",
    "These values are used by every dock whose setting is Inherit.",
    "Help", "Apply", "Save", "Cancel", "\33bDefaults for all docks", "\33bDock: Text  (bottom edge, horizontal)",
    "(items shown as icons only)", "(only for Icon and text)",
};

static const char *orientations[] = { "Horizontal", "Vertical", NULL };
static const char *positions[] = { "Free", "Top edge", "Bottom edge", "Left edge", "Right edge", NULL };
static const char *aligns[] = { "Start", "Centre", "End", NULL };
static const char *show_as[] = { "Icons", "Text", "Icon and text", NULL };
static const char *text_pos[] = { "Under icon", "Beside icon", NULL };
static const char *buttons[] = { "Plain", "Plates", NULL };
static const char *labels[] = { "None", "On hover", "Always", NULL };
static const char *colours[] = { "Inherit (Automatic)", "Dark", "Light", "Automatic", NULL };
static const char *backgrounds[] = { "Inherit (Bar)", "Bar", "None", NULL };
static const char *transp[] = { "Inherit (0 %)", "Own:", NULL };
static const char *onoff_glass[] = { "Inherit (Off)", "Off", "On", NULL };
static const char *onoff_shadow[] = { "Inherit (Off)", "Off", "On", NULL };
static const char *grips[] = { "Inherit (Shown)", "Shown", "Hidden", NULL };
static const char *d_colours[] = { "Dark", "Light", "Automatic", NULL };
static const char *d_backgrounds[] = { "Bar", "None", NULL };
static const char *d_onoff[] = { "Off", "On", NULL };
static const char *d_text[] = { "System", "Smooth", NULL };
static const char *tabs[5];

#define ROW(label, ...) Child, Label2(label), Child, __VA_ARGS__
#define CYC(entries, active) CycleObject, MUIA_Cycle_Entries, (IPTR)(entries), MUIA_Cycle_Active, (active), End
#define SLD(min, max, val) SliderObject, MUIA_Numeric_Min, (min), MUIA_Numeric_Max, (max), MUIA_Numeric_Value, (val), End
#define NOTE(text) TextObject, MUIA_Text_Contents, (IPTR)(text), MUIA_Font, MUIV_Font_Tiny, End

int main(void)
{
    if (!(MUIMasterBase = OpenLibrary((CONST_STRPTR)"muimaster.library", 0))) return 20;
    tabs[0] = S[S_PLACEMENT]; tabs[1] = S[S_CONTENT]; tabs[2] = S[S_APPEARANCE]; tabs[3] = S[S_BEHAVIOUR]; tabs[4] = NULL;
    Object *list, *pages, *reg, *help, *header, *textpos, *align, *dist, *window, *app;
    Object *b_help, *b_apply, *b_save, *b_cancel;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"Dockbar Prefs mockup",
        SubWindow, window = WindowObject, MUIA_Window_Title, (IPTR)S[S_TITLE], MUIA_Window_ID, MAKE_ID('D','B','P','M'),
            WindowContents, VGroup,
                Child, HGroup,
                    /* Left: what is being edited. */
                    Child, VGroup, MUIA_HorizWeight, 30,
                        Child, TextObject, MUIA_Text_Contents, (IPTR)S[S_DOCKS], End,
                        Child, ListviewObject, MUIA_Listview_List, list = ListObject, InputListFrame, End, End,
                        Child, HGroup,
                            Child, SimpleButton(S[S_NEW]), Child, SimpleButton(S[S_DUPLICATE]), Child, SimpleButton(S[S_DELETE]),
                        End,
                    End,
                    /* Right: the selected dock in tabs, or the defaults. */
                    Child, pages = VGroup, MUIA_Group_PageMode, TRUE, MUIA_HorizWeight, 70,
                        Child, VGroup,
                            Child, TextObject, MUIA_Text_Contents, (IPTR)S[S_DEFAULTS_TITLE], End,
                            Child, ColGroup(2), GroupFrame,
                                ROW(S[S_COLOURS], CYC(d_colours, 2)),
                                ROW(S[S_BACKGROUND], CYC(d_backgrounds, 0)),
                                ROW(S[S_TRANSPARENCY], SLD(0, 100, 0)),
                                ROW(S[S_GLASS], CYC(d_onoff, 0)),
                                ROW(S[S_SHADOW], CYC(d_onoff, 0)),
                                ROW(S[S_GRIP], CYC(grips + 1, 0)),
                                ROW(S[S_TEXT], CYC(d_text, 1)),
                            End,
                            Child, VSpace(0),
                        End,
                        Child, VGroup,
                            Child, header = TextObject, MUIA_Text_Contents, (IPTR)S[S_DOCK_TITLE], End,
                            Child, reg = RegisterGroup(tabs),
                                Child, VGroup,
                                    Child, ColGroup(2),
                                        ROW(S[S_NAME], StringObject, StringFrame, MUIA_String_Contents, (IPTR)"Text", End),
                                        ROW(S[S_ORIENTATION], CYC(orientations, 0)),
                                        ROW(S[S_POSITION], CYC(positions, 2)),
                                        ROW(S[S_ALIGNMENT], align = CYC(aligns, 1)),
                                        ROW(S[S_DISTANCE], dist = SLD(0, 200, 8)),
                                    End,
                                    Child, VSpace(0),
                                End,
                                Child, VGroup,
                                    Child, ColGroup(2),
                                        ROW(S[S_SHOW_AS], CYC(show_as, 1)),
                                        ROW(S[S_TEXT_POSITION], textpos = CYC(text_pos, 0)),
                                        Child, HSpace(0), Child, NOTE(S[S_TEXTPOS_NOTE]),
                                        ROW(S[S_TEXT_BUTTONS], CYC(buttons, 0)),
                                        ROW(S[S_ICON_SIZE], SLD(32, 72, 40)),
                                        ROW(S[S_SPACING], SLD(0, 16, 0)),
                                        ROW(S[S_LABELS], CYC(labels, 1)),
                                        Child, HSpace(0), Child, NOTE(S[S_LABELS_NOTE]),
                                    End,
                                    Child, VSpace(0),
                                End,
                                Child, VGroup,
                                    Child, ColGroup(2),
                                        ROW(S[S_COLOURS], CYC(colours, 0)),
                                        ROW(S[S_BACKGROUND], CYC(backgrounds, 0)),
                                        ROW(S[S_TRANSPARENCY], HGroup, Child, CYC(transp, 0), Child, SLD(0, 100, 0), End),
                                        ROW(S[S_GLASS], CYC(onoff_glass, 0)),
                                        ROW(S[S_SHADOW], CYC(onoff_shadow, 0)),
                                        ROW(S[S_CORNERS], SLD(0, 16, 5)),
                                    End,
                                    Child, VSpace(0),
                                End,
                                Child, VGroup,
                                    Child, ColGroup(2),
                                        ROW(S[S_LOCK], ImageObject, ImageButtonFrame, MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Image_Spec, MUII_CheckMark, MUIA_Background, MUII_ButtonBack, MUIA_ShowSelState, FALSE, End),
                                        ROW(S[S_GRIP], CYC(grips, 0)),
                                    End,
                                    Child, VSpace(0),
                                End,
                            End,
                        End,
                    End,
                End,
                /* Short help for what is shown, instead of a paragraph. */
                Child, help = TextObject, TextFrame, MUIA_Background, MUII_TextBack, MUIA_Text_Contents, (IPTR)S[S_HELP_PLACEMENT], End,
                Child, HGroup,
                    Child, b_help = SimpleButton(S[S_HELP]), Child, HSpace(0),
                    Child, b_apply = SimpleButton(S[S_APPLY]), Child, b_save = SimpleButton(S[S_SAVE]), Child, b_cancel = SimpleButton(S[S_CANCEL]),
                End,
            End,
        End,
    End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    (void)b_help; (void)b_apply; (void)b_save;
    DoMethod(list, MUIM_List_InsertSingle, (IPTR)S[S_DEFAULTS_ENTRY], MUIV_List_Insert_Bottom);
    DoMethod(list, MUIM_List_InsertSingle, (IPTR)"Text", MUIV_List_Insert_Bottom);
    DoMethod(list, MUIM_List_InsertSingle, (IPTR)"Side", MUIV_List_Insert_Bottom);
    DoMethod(list, MUIM_List_InsertSingle, (IPTR)"Free", MUIV_List_Insert_Bottom);
    /* Dependencies, shown on the sample: Text position only for Icon and
     * text (the sample shows Text), alignment and distance only at an
     * edge (the sample is at the bottom edge: enabled). */
    SetAttrs(textpos, MUIA_Disabled, TRUE, TAG_DONE);
    (void)align; (void)dist; (void)header;
    char view[32] = "";
    GetVar((CONST_STRPTR)"mockview", (STRPTR)view, sizeof view, 0);
    int page = 0;
    const char *names[] = { "placement", "content", "appearance", "behaviour" };
    for (int i = 0; i < 4; i++) if (!strcmp(view, names[i])) page = i;
    SetAttrs(reg, MUIA_Group_ActivePage, page, TAG_DONE);
    int defaults = !strcmp(view, "defaults");
    SetAttrs(list, MUIA_List_Active, defaults ? 0 : 1, TAG_DONE);
    SetAttrs(pages, MUIA_Group_ActivePage, defaults ? 0 : 1, TAG_DONE);
    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(b_cancel, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(window, MUIA_Window_Open, TRUE, TAG_DONE);
    const char *helps[] = { S[S_HELP_PLACEMENT], S[S_HELP_CONTENT], S[S_HELP_APPEARANCE], S[S_HELP_BEHAVIOUR] };
    ULONG sigs = 0;
    int last_page = -1, last_list = -1;
    for (;;) {
        IPTR id = DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
        if (id == (IPTR)MUIV_Application_ReturnID_Quit) break;
        IPTR active = 0, tab = 0;
        GetAttr(MUIA_List_Active, list, &active);
        GetAttr(MUIA_Group_ActivePage, reg, &tab);
        if ((int)active != last_list || (int)tab != last_page) {
            last_list = (int)active; last_page = (int)tab;
            SetAttrs(pages, MUIA_Group_ActivePage, active == 0 ? 0 : 1, TAG_DONE);
            SetAttrs(help, MUIA_Text_Contents, (IPTR)(active == 0 ? S[S_HELP_DEFAULTS] : helps[tab < 4 ? tab : 0]), TAG_DONE);
        }
        if (sigs && !(sigs = Wait(sigs | SIGBREAKF_CTRL_C)) ) break;
        if (sigs & SIGBREAKF_CTRL_C) break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
