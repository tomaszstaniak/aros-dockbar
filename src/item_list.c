#include "item_list.h"

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
#include "zune_input.h"

#define new_object(class_name, ...) \
    MUI_NewObjectA((ClassID)(class_name), (struct TagItem *)(IPTR[]){ \
        AROS_PP_VARIADIC_CAST2IPTR(__VA_ARGS__) })

static Object *attach(struct Library *MUIMasterBase, Object *parent, Object *child)
{
    if (!child) return NULL;
    if (!DoMethod(parent, OM_ADDMEMBER, (IPTR)child)) {
        MUI_DisposeObject(child);
        return NULL;
    }
    return child;
}

static Object *button(struct Library *MUIMasterBase, Object *parent, Object *app, const char *text, IPTR id)
{
    Object *b = attach(MUIMasterBase, parent,
        new_object(MUIC_Text, MUIA_Text_Contents, (IPTR)text, MUIA_Text_PreParse, (IPTR)"\33c",
            MUIA_Frame, MUIV_Frame_Button, MUIA_Background, MUII_ButtonBack,
            MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_CycleChain, TRUE, TAG_DONE));
    if (b && !DoMethod(b, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, id))
        return NULL;
    return b;
}

void dock_item_list_close(struct DockItemList *l)
{
    struct Library *MUIMasterBase;
    if (!l) return;
    MUIMasterBase = l->mui_base;
    if (MUIMasterBase) {
        if (l->mui_window) SetAttrs(l->mui_window, MUIA_Window_Open, FALSE, TAG_DONE);
        if (l->application) MUI_DisposeObject(l->application);
        else if (l->mui_window) MUI_DisposeObject(l->mui_window);
        CloseLibrary(MUIMasterBase);
    }
    memset(l, 0, sizeof(*l));
}

int dock_item_list_open(struct DockItemList *l, struct Screen *screen, const char *title, int dock_id)
{
    struct Library *MUIMasterBase;
    Object *root, *view, *row, *app;
    IPTR value = 0;
    if (!l || !screen || l->mui_base) return 0;
    memset(l, 0, sizeof(*l));
    l->dock_id = dock_id;
    snprintf(l->title, sizeof l->title, "%s", title);
    MUIMasterBase = l->mui_base = OpenLibrary((CONST_STRPTR)"muimaster.library", 0);
    if (!MUIMasterBase) goto fail;
    app = l->application = new_object(MUIC_Application,
        MUIA_Application_Title, (IPTR)"Dockbar items",
        MUIA_Application_Base, (IPTR)"DOCKBARITEMS", TAG_DONE);
    if (!app) goto fail;
    l->mui_window = attach(MUIMasterBase, app, new_object(MUIC_Window,
        MUIA_Window_Title, (IPTR)l->title, MUIA_Window_Screen, (IPTR)screen,
        MUIA_Window_Width, 360, MUIA_Window_Height, 300,
        MUIA_Window_CloseGadget, TRUE, TAG_DONE));
    if (!l->mui_window) goto fail;
    root = new_object(MUIC_Group, MUIA_Group_Spacing, 8,
        MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop, 10, MUIA_InnerBottom, 10, TAG_DONE);
    if (!root) goto fail;
    SetAttrs(l->mui_window, MUIA_Window_RootObject, (IPTR)root, TAG_DONE);
    l->note = attach(MUIMasterBase, root, new_object(MUIC_Text, MUIA_Text_Contents,
        (IPTR)"All items of this dock, in order, also the ones you cannot see.", TAG_DONE));
    l->list = new_object(MUIC_List, MUIA_Frame, MUIV_Frame_InputList,
        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook, MUIV_List_DestructHook_String, TAG_DONE);
    if (!l->note || !l->list) goto fail;
    view = attach(MUIMasterBase, root, new_object(MUIC_Listview,
        MUIA_Listview_List, (IPTR)l->list, MUIA_CycleChain, TRUE, TAG_DONE));
    if (!view) { l->list = NULL; goto fail; }  /* the listview disposed it */
    row = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_SameWidth, TRUE, TAG_DONE));
    if (!row || !(l->edit = button(MUIMasterBase, row, app, "Edit...", DOCK_LIST_EDIT)) ||
        !(l->remove = button(MUIMasterBase, row, app, "Remove", DOCK_LIST_REMOVE)) ||
        !(l->up = button(MUIMasterBase, row, app, "Move up", DOCK_LIST_UP)) ||
        !(l->down = button(MUIMasterBase, row, app, "Move down", DOCK_LIST_DOWN))) goto fail;
    row = attach(MUIMasterBase, root, new_object(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE));
    if (!row || !attach(MUIMasterBase, row, new_object(MUIC_Rectangle, TAG_DONE)) ||
        !button(MUIMasterBase, row, app, "Close", DOCK_LIST_CLOSE)) goto fail;
    if (!DoMethod(view, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, DOCK_LIST_EDIT) ||
        !DoMethod(l->mui_window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, DOCK_LIST_CLOSE) ||
        !DoMethod(l->mui_window, MUIM_Notify, MUIA_Window_InputEvent, (IPTR)"esc", (IPTR)app, 2, MUIM_Application_ReturnID, DOCK_LIST_CLOSE))
        goto fail;
    SetAttrs(l->mui_window, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, l->mui_window, &value);
    if (!value) goto fail;
    GetAttr(MUIA_Window_Window, l->mui_window, &value);
    l->window = (struct Window *)value;
    if (!l->window) goto fail;
    return 1;
fail:
    dock_item_list_close(l);
    return 0;
}

void dock_item_list_fill(struct DockItemList *l, const char *const *lines, int count, int active, int locked)
{
    struct Library *MUIMasterBase;
    if (!l || !l->list) return;
    MUIMasterBase = l->mui_base;
    (void)MUIMasterBase;
    SetAttrs(l->list, MUIA_List_Quiet, TRUE, TAG_DONE);
    DoMethod(l->list, MUIM_List_Clear);
    for (int i = 0; i < count; i++)
        DoMethod(l->list, MUIM_List_InsertSingle, (IPTR)lines[i], MUIV_List_Insert_Bottom);
    if (active >= count) active = count - 1;
    SetAttrs(l->list, MUIA_List_Quiet, FALSE, MUIA_List_Active, active < 0 ? MUIV_List_Active_Off : active, TAG_DONE);
    l->count = count;
    SetAttrs(l->note, MUIA_Text_Contents, (IPTR)(locked
        ? "This dock is locked (see Preferences): its items cannot be changed."
        : "All items of this dock, in order, also the ones you cannot see."), TAG_DONE);
    SetAttrs(l->edit, MUIA_Disabled, locked || !count, TAG_DONE);
    SetAttrs(l->remove, MUIA_Disabled, locked || !count, TAG_DONE);
    SetAttrs(l->up, MUIA_Disabled, locked || count < 2, TAG_DONE);
    SetAttrs(l->down, MUIA_Disabled, locked || count < 2, TAG_DONE);
}

int dock_item_list_poll(struct DockItemList *l, int *index)
{
    IPTR id, active = (IPTR)MUIV_List_Active_Off;
    *index = -1;
    if (!l || !l->window) return 0;
    id = dock_zune_input(l->application, &l->signals, &l->again);
    if (id == (IPTR)MUIV_Application_ReturnID_Quit) return DOCK_LIST_CLOSE;
    if (id < DOCK_LIST_EDIT || id > DOCK_LIST_CLOSE) return 0;
    GetAttr(MUIA_List_Active, l->list, &active);
    *index = (LONG)active >= 0 && (int)active < l->count ? (int)active : -1;
    return (int)id;
}

void dock_item_list_raise(struct DockItemList *l)
{
    if (l && l->window) { WindowToFront(l->window); ActivateWindow(l->window); }
}
