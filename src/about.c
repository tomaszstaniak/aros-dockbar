#include "about.h"

#include <exec/libraries.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#define __MUIMASTER_NOLIBBASE__
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
/* Both are opened only for the moment they are needed. */
#define __IFFPARSE_NOLIBBASE__
#include <proto/iffparse.h>
#define __NOLIBBASE__
#include <proto/openurl.h>
#undef __NOLIBBASE__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zune_input.h"

#define new_object(class_name, ...) \
    MUI_NewObjectA((ClassID)(class_name), (struct TagItem *)(IPTR[]){ \
        AROS_PP_VARIADIC_CAST2IPTR(__VA_ARGS__) })

#define AUTHOR_URL "https://tomaszstaniak.com"
enum { ID_CLOSE = 1, ID_COPY, ID_LICENSES, ID_LICENSES_CLOSE, ID_LINK };

static Object *attach(struct Library *MUIMasterBase, Object *parent, Object *child)
{
    if (!child) return NULL;
    if (!DoMethod(parent, OM_ADDMEMBER, (IPTR)child)) {
        MUI_DisposeObject(child);
        return NULL;
    }
    return child;
}

static Object *line(struct Library *MUIMasterBase, Object *parent, const char *text, IPTR font)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text,
        MUIA_Text_PreParse, (IPTR)"\33c", MUIA_Font, font, TAG_DONE));
}

static Object *button(struct Library *MUIMasterBase, Object *parent, Object *app, const char *text, IPTR id)
{
    Object *b = attach(MUIMasterBase, parent,
        new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text, MUIA_Text_PreParse, (IPTR)"\33c",
            MUIA_Frame, MUIV_Frame_Button, MUIA_Background, MUII_ButtonBack, MUIA_HorizWeight, 0,
            MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_CycleChain, TRUE, TAG_DONE));
    if (b && !DoMethod(b, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, id))
        return NULL;
    return b;
}

static Object *space(struct Library *MUIMasterBase, Object *parent)
{
    return attach(MUIMasterBase, parent, new_object(MUIC_Rectangle, TAG_DONE));
}

/* Plain text, IFF FTXT on clipboard unit 0, as every AROS text field reads it. */
static int copy_text(const char *text)
{
    struct Library *IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 0);
    struct IFFHandle *iff;
    int ok = 0;
    LONG len = (LONG)strlen(text);
    if (!IFFParseBase) return 0;
    if ((iff = AllocIFF())) {
        if ((iff->iff_Stream = (IPTR)OpenClipboard(0))) {
            InitIFFasClip(iff);
            if (!OpenIFF(iff, IFFF_WRITE)) {
                ok = !PushChunk(iff, MAKE_ID('F','T','X','T'), ID_FORM, IFFSIZE_UNKNOWN) &&
                     !PushChunk(iff, 0, MAKE_ID('C','H','R','S'), IFFSIZE_UNKNOWN) &&
                     WriteChunkBytes(iff, (APTR)text, len) == len &&
                     !PopChunk(iff) && !PopChunk(iff);
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    CloseLibrary(IFFParseBase);
    return ok;
}

/* openurl.library starts the browser chosen in Prefs/OpenURL; 0 when none
 * could be started. The address stays readable either way. */
static int open_link(void)
{
    struct Library *OpenURLBase = OpenLibrary((CONST_STRPTR)"openurl.library", 0);
    ULONG ok = 0;
    if (!OpenURLBase) return 0;
    ok = URL_OpenA((STRPTR)AUTHOR_URL, NULL);
    CloseLibrary(OpenURLBase);
    return ok != 0;
}

static char *read_notices(const char *path)
{
    BPTR f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    char *text = NULL;
    LONG size = 0, got = 0;
    if (f) {
        Seek(f, 0, OFFSET_END);
        size = Seek(f, 0, OFFSET_BEGINNING);
        if (size > 0 && size < 512 * 1024 && (text = malloc((size_t)size + 1)))
            got = Read(f, text, size);
        Close(f);
    }
    if (text && got > 0) { text[got] = 0; return text; }
    free(text);
    const char *missing = "The NOTICES file of this installation could not be read.\n"
                          "It belongs next to the Dockbar program and lists the licences\n"
                          "of everything the release contains.";
    text = malloc(strlen(missing) + 1);
    if (text) strcpy(text, missing);
    return text;
}

void dock_about_close(struct DockAbout *a)
{
    struct Library *MUIMasterBase;
    if (!a) return;
    MUIMasterBase = a->mui_base;
    if (MUIMasterBase) {
        if (a->licenses) SetAttrs(a->licenses, MUIA_Window_Open, FALSE, TAG_DONE);
        if (a->mui_window) SetAttrs(a->mui_window, MUIA_Window_Open, FALSE, TAG_DONE);
        if (a->application) MUI_DisposeObject(a->application);
        else if (a->mui_window) MUI_DisposeObject(a->mui_window);
        CloseLibrary(MUIMasterBase);
    }
    free(a->notices);
    memset(a, 0, sizeof(*a));
}

int dock_about_open(struct DockAbout *a, struct Screen *screen, const char *version,
                    const char *info, const char *notices_path)
{
    struct Library *MUIMasterBase;
    Object *app, *root, *block, *row, *link, *list, *lroot;
    IPTR value = 0;
    if (!a || !screen || a->mui_base) return 0;
    memset(a, 0, sizeof(*a));
    snprintf(a->info, sizeof a->info, "%s", info);
    snprintf(a->notices_path, sizeof a->notices_path, "%s", notices_path);
    MUIMasterBase = a->mui_base = OpenLibrary((CONST_STRPTR)"muimaster.library", 0);
    if (!MUIMasterBase) goto fail;
    app = a->application = new_object(MUIC_Application,
        MUIA_Application_Title, (IPTR)"About Dockbar",
        MUIA_Application_Base, (IPTR)"DOCKBARABOUT", TAG_DONE);
    if (!app) goto fail;
    a->mui_window = attach(MUIMasterBase, app, new_object(MUIC_Window,
        MUIA_Window_Title, (IPTR)"About Dockbar", MUIA_Window_Screen, (IPTR)screen,
        MUIA_Window_CloseGadget, TRUE, TAG_DONE));
    if (!a->mui_window) goto fail;
    root = new_object(MUIC_Group, MUIA_Group_Spacing, 16,
        MUIA_InnerLeft, 28, MUIA_InnerRight, 28, MUIA_InnerTop, 22, MUIA_InnerBottom, 16, TAG_DONE);
    if (!root) goto fail;
    SetAttrs(a->mui_window, MUIA_Window_RootObject, (IPTR)root, TAG_DONE);
    a->root = root;
    /* Name first and largest; the version under it in the normal font. */
    block = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Spacing, 4, TAG_DONE));
    if (!block || !line(MUIMasterBase, block, "\33bDockbar", MUIV_Font_Big) ||
        !line(MUIMasterBase, block, version, MUIV_Font_Normal)) goto fail;
    if (!line(MUIMasterBase, root, "A customizable desktop dock for AROS.", MUIV_Font_Normal)) goto fail;
    block = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Spacing, 4, TAG_DONE));
    if (!block || !line(MUIMasterBase, block, "Tomasz Staniak", MUIV_Font_Normal)) goto fail;
    row = attach(MUIMasterBase, block, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !space(MUIMasterBase, row)) goto fail;
    /* A link only where a browser can be asked to open it (openurl.library). */
    struct Library *OpenURLBase = OpenLibrary((CONST_STRPTR)"openurl.library", 0);
    int linked = OpenURLBase != NULL;
    if (OpenURLBase) CloseLibrary(OpenURLBase);
    link = a->link = attach(MUIMasterBase, row, new_object(MUIC_Text,
        MUIA_Text_Contents, (IPTR)(linked ? "\33utomaszstaniak.com" : "tomaszstaniak.com"), MUIA_Text_SetMax, FALSE,
        MUIA_InputMode, linked ? MUIV_InputMode_RelVerify : MUIV_InputMode_None, MUIA_CycleChain, linked, TAG_DONE));
    if (!link || !space(MUIMasterBase, row) || (linked &&
        !DoMethod(link, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_LINK))) goto fail;
    row = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !space(MUIMasterBase, row) || !button(MUIMasterBase, row, app, "Licenses", ID_LICENSES) ||
        !space(MUIMasterBase, row)) goto fail;
    row = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !(a->copy = button(MUIMasterBase, row, app, "Copy version info", ID_COPY)) ||
        !space(MUIMasterBase, row) || !button(MUIMasterBase, row, app, "Close", ID_CLOSE)) goto fail;
    if (!DoMethod(a->mui_window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CLOSE) ||
        !DoMethod(a->mui_window, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"esc", (IPTR)app, 2, MUIM_Application_ReturnID, ID_CLOSE))
        goto fail;
    /* Licenses: a second window of the same application, filled when opened. */
    a->licenses = attach(MUIMasterBase, app, new_object(MUIC_Window,
        MUIA_Window_Title, (IPTR)"Dockbar licenses", MUIA_Window_Screen, (IPTR)screen,
        MUIA_Window_Width, MUIV_Window_Width_Visible(60), MUIA_Window_Height, MUIV_Window_Height_Visible(60),
        MUIA_Window_CloseGadget, TRUE, TAG_DONE));
    lroot = a->licenses ? new_object(MUIC_Group, MUIA_Group_Spacing, 8, TAG_DONE) : NULL;
    if (!lroot) goto fail;
    SetAttrs(a->licenses, MUIA_Window_RootObject, (IPTR)lroot, TAG_DONE);
    a->licenses_text = new_object(MUIC_Floattext, MUIA_Frame, MUIV_Frame_ReadList, TAG_DONE);
    if (!a->licenses_text) goto fail;
    list = attach(MUIMasterBase, lroot, new_object(MUIC_Listview, MUIA_Listview_List, (IPTR)a->licenses_text,
        MUIA_Listview_Input, FALSE, MUIA_CycleChain, TRUE, TAG_DONE));
    if (!list) { a->licenses_text = NULL; goto fail; }
    row = attach(MUIMasterBase, lroot, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !space(MUIMasterBase, row) || !button(MUIMasterBase, row, app, "Close", ID_LICENSES_CLOSE) ||
        !DoMethod(a->licenses, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_LICENSES_CLOSE))
        goto fail;
    SetAttrs(a->mui_window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, a->mui_window, &value);
    if (!value) goto fail;
    GetAttr(MUIA_Window_Window, a->mui_window, &value);
    a->window = (struct Window *)value;
    if (!a->window) goto fail;
    return 1;
fail:
    dock_about_close(a);
    return 0;
}

struct Window *dock_about_licenses_window(const struct DockAbout *a)
{
    IPTR open = 0, window = 0;
    if (!a || !a->licenses) return NULL;
    GetAttr(MUIA_Window_Open, a->licenses, &open);
    if (open) GetAttr(MUIA_Window_Window, a->licenses, &window);
    return open ? (struct Window *)window : NULL;
}

void dock_about_raise(struct DockAbout *a)
{
    if (!a || !a->window) return;
    SetAttrs(a->copy, MUIA_Text_Contents, (IPTR)"Copy version info", TAG_DONE);
    WindowToFront(a->window);
    ActivateWindow(a->window);
}

int dock_about_poll(struct DockAbout *a)
{
    struct Library *MUIMasterBase;
    IPTR id;
    if (!a || !a->window) return 0;
    MUIMasterBase = a->mui_base;
    (void)MUIMasterBase;
    id = dock_zune_input(a->application, &a->signals, &a->again);
    switch (id) {
    case ID_CLOSE: case (IPTR)MUIV_Application_ReturnID_Quit: return 1;
    case ID_COPY:
        SetAttrs(a->copy, MUIA_Text_Contents, (IPTR)(copy_text(a->info) ? "Copied" : "Could not copy"), TAG_DONE);
        break;
    case ID_LINK:
        /* Said where the address is, so a click that opens nothing is explained. */
        {
            const char *said = open_link() ? "\33utomaszstaniak.com\33n (sent to the browser)"
                                           : "tomaszstaniak.com (no web browser could be started; see Prefs/OpenURL)";
            /* A longer text needs a new layout, or it would be cut. */
            DoMethod(a->root, MUIM_Group_InitChange);
            SetAttrs(a->link, MUIA_Text_Contents, (IPTR)said, TAG_DONE);
            DoMethod(a->root, MUIM_Group_ExitChange);
        }
        break;
    case ID_LICENSES:
        if (!a->notices && (a->notices = read_notices(a->notices_path)))
            SetAttrs(a->licenses_text, MUIA_Floattext_Text, (IPTR)a->notices, TAG_DONE);
        SetAttrs(a->licenses, MUIA_Window_Open, TRUE, TAG_DONE);
        break;
    case ID_LICENSES_CLOSE: SetAttrs(a->licenses, MUIA_Window_Open, FALSE, TAG_DONE); break;
    }
    return 0;
}
