#include "native_icons.h"
#include "elapsed.h"
#include "resample.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <datatypes/pictureclass.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/workbench.h>
#include <workbench/icon.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>

#define DOCK_NATIVE_PATH_CAPACITY 4096
#define DOCK_NATIVE_MAX_SIZE 512

struct DockNativeTiming dock_native_timing;

static APTR no_requesters(void);
static void requesters_back(APTR old);

static unsigned long micros(void)
{
    return (unsigned long)dock_elapsed_us();
}

static void strip_info_suffix(char *path)
{
    size_t length = strlen(path);
    char *suffix;
    if (length <= 5)
        return;
    suffix = path + length - 5;
    if (suffix[-1] == '/' || suffix[-1] == ':')
        return;
    if (suffix[0] == '.' && tolower((unsigned char)suffix[1]) == 'i' &&
        tolower((unsigned char)suffix[2]) == 'n' &&
        tolower((unsigned char)suffix[3]) == 'f' &&
        tolower((unsigned char)suffix[4]) == 'o')
        *suffix = '\0';
}

static int copy_object_path(char *buffer, const char *path)
{
    size_t length;
    if (!path || !*path)
        return 0;
    length = strlen(path);
    if (length >= DOCK_NATIVE_PATH_CAPACITY)
        return 0;
    memcpy(buffer, path, length + 1);
    strip_info_suffix(buffer);
    return 1;
}

/* IconControl returns icon.library-owned original pixels. Indexed conversion
 * is temporary; only the resampler's bounded output survives loading. */
struct NativeSource {
    const unsigned char *pixels;
    unsigned char *converted;
    int width, height, mode;
};

static int native_source(struct DiskObject *object, int selected,
                         struct NativeSource *source)
{
    const ULONG *argb = NULL;
    const UBYTE *indices = NULL;
    const struct ColorRegister *palette = NULL;
    ULONG width = 0, height = 0, colors = 0;
    LONG transparent = -1;
    struct TagItem original[] = {
        { selected ? ICONCTRLA_GetARGBImageData2 : ICONCTRLA_GetARGBImageData1,
          (IPTR)&argb },
        { ICONCTRLA_GetWidth, (IPTR)&width },
        { ICONCTRLA_GetHeight, (IPTR)&height },
        { TAG_DONE, 0 }
    };
    struct TagItem indexed[] = {
        { selected ? ICONCTRLA_GetImageData2 : ICONCTRLA_GetImageData1, (IPTR)&indices },
        { selected ? ICONCTRLA_GetPalette2 : ICONCTRLA_GetPalette1, (IPTR)&palette },
        { selected ? ICONCTRLA_GetPaletteSize2 : ICONCTRLA_GetPaletteSize1, (IPTR)&colors },
        { selected ? ICONCTRLA_GetTransparentColor2 : ICONCTRLA_GetTransparentColor1,
          (IPTR)&transparent },
        { TAG_DONE, 0 }
    };
    size_t count, i;

    /* PNG decoding can update the face size, including for image 2. Query the
     * dimensions after each corresponding ARGB request, never after LayoutIcon. */
    IconControlA(object, original);
    if (!width && object->do_Gadget.Width > 0)
        width = object->do_Gadget.Width;
    if (!height && object->do_Gadget.Height > 0)
        height = object->do_Gadget.Height;
    if (width < 1 || height < 1 || width > 1024 || height > 1024)
        return -1;
    source->width = (int)width;
    source->height = (int)height;
    if (argb) {
        source->pixels = (const unsigned char *)argb;
        source->mode = DOCK_NATIVE_ARGB;
        return 1;
    }
    IconControlA(object, indexed);
    if (!indices || !palette)
        return 0;
    if (colors < 1 || colors > 256)
        return -1;
    count = (size_t)width * height;
    source->converted = malloc(count * 4);
    if (!source->converted)
        return -1;
    for (i = 0; i < count; ++i) {
        unsigned int color = indices[i];
        unsigned char *pixel = source->converted + i * 4;
        if ((LONG)color == transparent) {
            memset(pixel, 0, 4);
        } else if (color < colors) {
            pixel[0] = 255;
            pixel[1] = palette[color].red;
            pixel[2] = palette[color].green;
            pixel[3] = palette[color].blue;
        } else {
            free(source->converted);
            source->converted = NULL;
            return -1;
        }
    }
    source->pixels = source->converted;
    source->mode = DOCK_NATIVE_PALETTE;
    return 1;
}

static void fit_source(int width, int height, int size, int *outw, int *outh)
{
    *outw = width;
    *outh = height;
    if (width <= size && height <= size)
        return;
    if (width >= height) {
        *outw = size;
        *outh = (height * size + width / 2) / width;
    } else {
        *outh = size;
        *outw = (width * size + height / 2) / height;
    }
    if (*outw < 1) *outw = 1;
    if (*outh < 1) *outh = 1;
}

static int load_icon(DockNativeIcon *icon, const char *path,
                     struct Screen *screen, int size, unsigned long *scale_us);

int dock_native_load(DockNativeIcon *icon, const char *path,
                     struct Screen *screen, int size)
{
    unsigned long scale = 0, start = micros();
    int result = load_icon(icon, path, screen, size, &scale);
    unsigned long total = micros() - start;
    dock_native_timing.loads++;
    dock_native_timing.scale_us += scale;
    dock_native_timing.read_us += total > scale ? total - scale : 0;
    return result;
}

static int load_icon(DockNativeIcon *icon, const char *path,
                     struct Screen *screen, int size, unsigned long *scale_us)
{
    char base[DOCK_NATIVE_PATH_CAPACITY];
    struct Rectangle bounds;
    struct TagItem scaling[] = {
        { ICONCTRLA_SetScaleBox, 0 }, { TAG_DONE, 0 }
    };
    struct TagItem drawing[] = {
        { ICONDRAWA_Borderless, TRUE }, { TAG_DONE, 0 }
    };
    struct TagItem loading[] = {
        { ICONGETA_RemapIcon, FALSE },
        { ICONGETA_GetPaletteMappedIcon, FALSE },
        { ICONGETA_GenerateImageMasks, FALSE },
        { TAG_DONE, 0 }
    };
    struct TagItem fallback[] = {
        { ICONGETA_GetDefaultType, WBTOOL },
        { ICONGETA_GetDefaultName, (IPTR)"Tool" },
        { ICONGETA_FailIfUnavailable, FALSE },
        { TAG_MORE, (IPTR)loading }
    };
    struct TagItem identified[] = {
        { ICONGETA_FailIfUnavailable, FALSE },
        { TAG_MORE, (IPTR)loading }
    };
    struct DiskObject *object;
    struct NativeSource source = { 0 };
    int result, origin;

    if (!icon || icon->object || !screen || size < 1 ||
        size > DOCK_NATIVE_MAX_SIZE)
        return 0;
    memset(icon, 0, sizeof(*icon));
    /* GetDiskObject can remap to the global screen before our scale is set.
     * Load without mapping so LayoutIcon cannot take its same-screen shortcut.
     * Supplying the default name also avoids ABIv11's recursive default-type
     * lookup, which drops these tags and would map the fallback prematurely. */
    if (!copy_object_path(base, path))
        return 0;
    object = GetIconTagList((CONST_STRPTR)base, loading);
    origin = DOCK_ICON_OWN;
    if (!object) {
        /* Same lookup as Wanderer's icon list (FailIfUnavailable FALSE):
         * icon.library identifies drawers, volumes, tools and datatypes. */
        object = GetIconTagList((CONST_STRPTR)base, identified);
        origin = DOCK_ICON_IDENTIFIED;
    }
    if (!object) {
        object = GetIconTagList(NULL, fallback);
        origin = DOCK_ICON_GENERIC;
    }
    if (!object)
        return 0;
    icon->object = object;
    icon->origin = origin;
    result = native_source(object, 0, &source);
    icon->source_width = source.width;
    icon->source_height = source.height;
    if (result < 0)
        goto failed;
    fit_source(source.width, source.height, size, &icon->width, &icon->height);
    if (result > 0) {
        icon->mode = source.mode;
        icon->pixel_width[0] = icon->width;
        icon->pixel_height[0] = icon->height;
        unsigned long scale_start = micros();
        result = dock_resample_argb(source.pixels, source.width, source.height,
                                   icon->width, icon->height, &icon->pixels[0]);
        *scale_us += micros() - scale_start;
        free(source.converted);
        source.converted = NULL;
        if (!result)
            goto failed;

        memset(&source, 0, sizeof(source));
        result = native_source(object, 1, &source);
        if (result > 0) {
            int width, height;
            fit_source(source.width, source.height, size, &width, &height);
            unsigned long scale_start = micros();
            int scaled = dock_resample_argb(source.pixels, source.width, source.height,
                                            width, height, &icon->pixels[1]);
            *scale_us += micros() - scale_start;
            if (scaled) {
                icon->pixel_width[1] = width;
                icon->pixel_height[1] = height;
                if (width > icon->width) icon->width = width;
                if (height > icon->height) icon->height = height;
            }
        }
        free(source.converted);
        /* Missing/invalid selected data uses the normal cache. Each successful
         * state keeps its own aspect ratio and is centered in a common box. */
        return 1;
    }

    /* Pure planar imagery has no exposed ARGB or palette buffer. Keep the
     * native renderer for this explicit fallback, with no enlargement. Its
     * downscaling still uses the platform's nearest-sample implementation. */
    scaling[0].ti_Data = PACK_ICON_SCALEBOX(icon->width, icon->height);
    IconControlA(object, scaling);
    if (!LayoutIconA(object, screen, NULL) ||
        !GetIconRectangleA(&screen->RastPort, object, NULL, &bounds, drawing))
        goto failed;
    icon->width = bounds.MaxX - bounds.MinX + 1;
    icon->height = bounds.MaxY - bounds.MinY + 1;
    if (icon->width < 1 || icon->height < 1 ||
        icon->width > size || icon->height > size)
        goto failed;
    return 1;

failed:
    dock_native_free(icon);
    return 0;
}

void dock_native_draw(DockNativeIcon *icon, struct RastPort *rp,
                      int x, int y, int selected)
{
    struct TagItem drawing[] = {
        { ICONDRAWA_Borderless, TRUE },
        { ICONDRAWA_EraseBackground, FALSE },
        { TAG_DONE, 0 }
    };
    if (!icon || !icon->object || !rp)
        return;
    if (icon->pixels[0]) {
        int state = selected && icon->pixels[1] ? 1 : 0;
        int width = icon->pixel_width[state];
        int height = icon->pixel_height[state];
        WritePixelArrayAlpha(icon->pixels[state], 0, 0, width * 4, rp,
                             x + (icon->width - width) / 2,
                             y + (icon->height - height) / 2,
                             width, height, 0xffffffffUL);
    } else {
        DrawIconStateA(rp, icon->object, NULL, x, y,
                       selected ? IDS_SELECTED : IDS_NORMAL, drawing);
    }
}

const char *dock_icon_origin_name(int origin)
{
    switch (origin) {
    case DOCK_ICON_OWN: return "own";
    case DOCK_ICON_IDENTIFIED: return "identified";
    case DOCK_ICON_GENERIC: return "generic";
    default: return "none";
    }
}

/* Identity and type checks run during a drop, for every item in the place:
 * an item on a disk that is not inserted must not stop the dock behind an
 * "insert volume" requester. A missing object compares unequal. */
static APTR no_requesters(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    return old;
}

static void requesters_back(APTR old)
{
    ((struct Process *)FindTask(NULL))->pr_WindowPtr = old;
}

int dock_object_is_drawer(const char *path)
{
    char base[DOCK_NATIVE_PATH_CAPACITY];
    struct FileInfoBlock *fib;
    BPTR lock;
    int result = -1;
    APTR old;
    if (!copy_object_path(base, path)) return -1;
    old = no_requesters();
    lock = Lock((CONST_STRPTR)base, ACCESS_READ);
    requesters_back(old);
    if (!lock) return -1;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib))
        result = fib->fib_DirEntryType > 0 ? 1 : 0;
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return result;
}

void dock_native_free(DockNativeIcon *icon)
{
    if (!icon)
        return;
    free(icon->pixels[0]);
    free(icon->pixels[1]);
    if (icon->object)
        FreeDiskObject(icon->object);
    memset(icon, 0, sizeof(*icon));
}

struct MsgPort *dock_quiet_port(void)
{
    struct MsgPort *port = AllocMem(sizeof(*port), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port)
        return NULL;
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_Flags = PA_IGNORE;
    port->mp_SigTask = FindTask(NULL);
    NEWLIST(&port->mp_MsgList);
    return port;
}

/* DeleteMsgPort would free mp_SigBit, a signal this port never had. */
void dock_quiet_port_free(struct MsgPort *port)
{
    if (port)
        FreeMem(port, sizeof(*port));
}

int dock_drop_open(DockDropTarget *target, struct Window *window)
{
    if (!target || target->port || target->window || !window)
        return 0;
    target->port = dock_quiet_port();
    if (!target->port)
        return 0;
    target->quiet = 1;
    target->window = AddAppWindowA(0, 0, window, target->port, NULL);
    if (!target->window) {
        dock_quiet_port_free(target->port);
        target->port = NULL;
        target->quiet = 0;
        return 0;
    }
    return 1;
}

/* The exact path of a dropped object, ".info" included when that is what
 * was dropped: the same path may be a document handed to a program. Only
 * a shortcut is normalised to its object, by the model (dock_add_kind). */
static int argument_path(char *path, const struct WBArg *argument)
{
    if (argument->wa_Lock) {
        if (!NameFromLock(argument->wa_Lock, (STRPTR)path, DOCK_NATIVE_PATH_CAPACITY))
            return 0;
        if (argument->wa_Name && *argument->wa_Name &&
            !AddPart((STRPTR)path, (CONST_STRPTR)argument->wa_Name, DOCK_NATIVE_PATH_CAPACITY))
            return 0;
        return 1;
    }
    /* A missing lock cannot resolve a relative name independently of the
     * sender's CurrentDir. Accept only an already absolute DOS path. */
    if (!argument->wa_Name || !strchr((const char *)argument->wa_Name, ':') ||
        strlen((const char *)argument->wa_Name) >= DOCK_NATIVE_PATH_CAPACITY)
        return 0;
    strcpy(path, (const char *)argument->wa_Name);
    return 1;
}

struct DropPath {
    struct DropPath *next;
    char path[DOCK_NATIVE_PATH_CAPACITY];
};

/* Copies every path of one message before the message is replied: after
 * ReplyMsg the sender's locks and names are no longer ours to read. */
static int poll_messages(DockDropTarget *target, DockDropCallback each,
                         DockDropBatchCallback batch, void *context)
{
    struct AppMessage *message;
    int delivered = 0;
    if (!target || !target->port)
        return 0;
    while ((message = (struct AppMessage *)GetMsg(target->port)) != NULL) {
        struct DropPath *head = NULL, **tail = &head, *entry;
        int count = 0, missing = 0;
        LONG index;
        int mouse_x = message->am_MouseX, mouse_y = message->am_MouseY;
        if ((each || batch) && message->am_Type == AMTYPE_APPWINDOW &&
            message->am_ArgList) {
            for (index = 0; index < message->am_NumArgs; ++index) {
                entry = malloc(sizeof(*entry));
                /* Every object that could not be copied is counted, so that
                 * an action on files never runs on part of them unawares. */
                if (!entry || !argument_path(entry->path, &message->am_ArgList[index])) {
                    free(entry);
                    ++missing;
                    continue;
                }
                entry->next = NULL;
                *tail = entry;
                tail = &entry->next;
                ++count;
            }
        }
        /* Reply invalid messages and allocation failures too. */
        ReplyMsg(&message->am_Message);
        target->mouse_x = mouse_x;
        target->mouse_y = mouse_y;
        if (batch && (count || missing)) {
            const char **paths = count ? malloc(sizeof(*paths) * (size_t)count) : NULL;
            int i = 0;
            if (count && !paths) { missing += count; count = 0; }
            for (entry = head; entry && paths; entry = entry->next)
                paths[i++] = entry->path;
            batch(paths, count, missing, context);
            delivered += count;
            free(paths);
        }
        while ((entry = head) != NULL) {
            head = entry->next;
            if (each) {
                /* Single-path callers pin shortcuts: the object, not its icon. */
                strip_info_suffix(entry->path);
                each(entry->path, context);
                ++delivered;
            }
            free(entry);
        }
    }
    return delivered;
}

int dock_drop_poll(DockDropTarget *target, DockDropCallback callback, void *context)
{
    return poll_messages(target, callback, NULL, context);
}

int dock_drop_poll_batch(DockDropTarget *target, DockDropBatchCallback callback, void *context)
{
    return poll_messages(target, NULL, callback, context);
}

void dock_drop_close(DockDropTarget *target)
{
    if (!target)
        return;
    /* Removal prevents new drops; a final drain then covers messages queued
     * between the last poll and RemoveAppWindow. */
    if (target->window) {
        RemoveAppWindow(target->window);
        target->window = NULL;
    }
    if (target->port) {
        dock_drop_poll(target, NULL, NULL);
        if (target->quiet)
            dock_quiet_port_free(target->port);
        else
            DeleteMsgPort(target->port);
        target->port = NULL;
        target->quiet = 0;
    }
}

/* workbench.library starts a Shell program from the caller's process and
 * opens its CON: output window there, which keeps copies of the caller's
 * home drawer and current directory: Dockbar's drawer stayed "in use" after
 * Dockbar quit. The started program has no use for either; paths passed
 * here are absolute. */
static int open_object(const char *base, struct TagItem *tags)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR home = me->pr_HomeDir;
    int result;
    me->pr_HomeDir = BNULL;
    BPTR cwd = CurrentDir(BNULL);
    result = OpenWorkbenchObjectA((STRPTR)base, tags) ? 1 : 0;
    CurrentDir(cwd);
    me->pr_HomeDir = home;
    return result;
}

int dock_launch(const char *path)
{
    char base[DOCK_NATIVE_PATH_CAPACITY];
    return copy_object_path(base, path) && open_object(base, NULL);
}

int dock_launch_with(const char *program, const char *const *files, int count)
{
    enum { MAX_FILES = 64 };
    char base[DOCK_NATIVE_PATH_CAPACITY], parent[DOCK_NATIVE_PATH_CAPACITY];
    struct TagItem tags[2 * MAX_FILES + 1];
    BPTR locks[MAX_FILES];
    int used = 0, result;
    if (!copy_object_path(base, program) || !files || count < 1 || count > MAX_FILES)
        return 0;
    for (int i = 0; i < count; ++i) {
        const char *name = (const char *)FilePart((CONST_STRPTR)files[i]);
        size_t length = *name ? (size_t)((const char *)PathPart((CONST_STRPTR)files[i]) - files[i])
                              : strlen(files[i]);  /* a volume or assign root: no parent */
        if (length >= sizeof(parent)) break;
        memcpy(parent, files[i], length);
        parent[length] = 0;
        locks[used] = Lock((CONST_STRPTR)parent, SHARED_LOCK);
        if (!locks[used]) break;
        tags[2 * used].ti_Tag = WBOPENA_ArgLock;
        tags[2 * used].ti_Data = (IPTR)locks[used];
        tags[2 * used + 1].ti_Tag = WBOPENA_ArgName;
        tags[2 * used + 1].ti_Data = (IPTR)name;
        ++used;
    }
    if (used != count) {
        while (used > 0) UnLock(locks[--used]);
        return 0;
    }
    tags[2 * used].ti_Tag = TAG_DONE;
    result = open_object(base, tags);
    /* Workbench has made its own copies of the locks by now. */
    while (used > 0) UnLock(locks[--used]);
    return result;
}

static int same_object(const char *first, const char *second);

int dock_program_is_shell(const char *program, long *stack)
{
    char base[DOCK_NATIVE_PATH_CAPACITY];
    struct DiskObject *icon;
    int shell;
    *stack = 0;
    if (!copy_object_path(base, program)) return 0;
    /* Only an executable file can be a Shell program: a project or a drawer
     * without an icon is Workbench's to open. */
    {
        APTR old = no_requesters();
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        BPTR lock = fib ? Lock((CONST_STRPTR)base, SHARED_LOCK) : BNULL;
        int executable = lock && Examine(lock, fib) && fib->fib_DirEntryType < 0 &&
                         !(fib->fib_Protection & FIBF_EXECUTE);
        if (lock) UnLock(lock);
        if (fib) FreeDosObject(DOS_FIB, fib);
        requesters_back(old);
        if (!executable) return 0;
    }
    /* Only the program's own icon counts, as for Workbench (isDefaultIcon). */
    icon = GetIconTags((CONST_STRPTR)base, ICONGETA_FailIfUnavailable, TRUE, TAG_DONE);
    if (!icon) return 1;
    shell = icon->do_Type == WBTOOL && FindToolType(icon->do_ToolTypes, (STRPTR)"CLI") != NULL;
    *stack = icon->do_StackSize;
    FreeDiskObject(icon);
    return shell;
}

int dock_same_object(const char *first, const char *second)
{
    APTR old = no_requesters();
    int same = same_object(first, second);
    requesters_back(old);
    return same;
}

static int same_object(const char *first, const char *second)
{
    char first_base[DOCK_NATIVE_PATH_CAPACITY];
    char second_base[DOCK_NATIVE_PATH_CAPACITY];
    const unsigned char *a, *b;
    BPTR first_lock, second_lock;
    int same;

    if (!copy_object_path(first_base, first) ||
        !copy_object_path(second_base, second))
        return 0;
    a = (const unsigned char *)first_base;
    b = (const unsigned char *)second_base;
    while (*a && tolower(*a) == tolower(*b)) {
        ++a;
        ++b;
    }
    if (!*a && !*b)
        return 1;
    /* Keep assignments such as SYS: in saved settings while detecting their
     * canonical volume names when the user drops the same object again. */
    /* A project icon such as AROS One's System/Shell has no object of its
     * own, only the .info file; compare those when the object is missing. */
    first_lock = Lock((CONST_STRPTR)first_base, ACCESS_READ);
    second_lock = Lock((CONST_STRPTR)second_base, ACCESS_READ);
    if (!first_lock || !second_lock) {
        if (first_lock) UnLock(first_lock);
        if (second_lock) UnLock(second_lock);
        if (strlen(first_base) + 6 > sizeof first_base || strlen(second_base) + 6 > sizeof second_base)
            return 0;
        strcat(first_base, ".info");
        strcat(second_base, ".info");
        first_lock = Lock((CONST_STRPTR)first_base, ACCESS_READ);
        second_lock = Lock((CONST_STRPTR)second_base, ACCESS_READ);
    }
    if (!first_lock) {
        if (second_lock) UnLock(second_lock);
        return 0;
    }
    same = second_lock && SameLock(first_lock, second_lock) == LOCK_SAME;
    if (second_lock)
        UnLock(second_lock);
    UnLock(first_lock);
    return same;
}
