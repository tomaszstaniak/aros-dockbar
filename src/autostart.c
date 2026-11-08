/* Starting Dockbar with Wanderer; see autostart.h. */
#include "autostart.h"
#include "autostart_plan.h"
#include "migrate.h"

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/icon.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define AS_PATH 512
#define AS_DIR "SYS:WBStartup"
#define AS_MAX_TOOLTYPES 24
/* "$VER: Dockbar " without the "$": a whole tag here would be found in
   this program by Version and by dockbar_version() itself. */
#define AS_VERSION_REST "VER: Dockbar "

/* What a scan found, with the program each item starts. */
struct found {
    struct AsScan scan;
    char target[AS_MAX_ITEMS][AS_PATH];
};

static char program_name[108] = "Dockbar";

void dock_autostart_set_program(const char *name)
{
    if (name && *name && strlen(name) < sizeof program_name && !strchr(name, ':') && !strchr(name, '/'))
        snprintf(program_name, sizeof program_name, "%s", name);
}

static int exists(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock)
        UnLock(lock);
    return lock != BNULL;
}

static int join(char *out, size_t size, const char *dir, const char *name)
{
    if (snprintf(out, size, "%s", dir) >= (int)size)
        return 0;
    return AddPart((STRPTR)out, (CONST_STRPTR)name, size) != 0;
}

static void say(char *message, size_t size, const char *format, const char *a, const char *b)
{
    size_t used = strlen(message);
    if (used + 1 >= size)
        return;
    if (used)
        message[used++] = '\n', message[used] = '\0';
    snprintf(message + used, size - used, format, a ? a : "", b ? b : "");
}

/* The version in the program's own version string, when `path` is a
   Dockbar program; 0 when it is not one (or cannot be read). */
static int dockbar_version(const char *path, char *version, size_t size)
{
    /* Blocks overlap by WINDOW bytes, so that the tag and the version after
       it are always seen whole in one of them. */
    enum { BLOCK = 8192, WINDOW = 64 };
    static char buffer[BLOCK + WINDOW];
    const size_t tag = sizeof AS_VERSION_REST;
    size_t kept = 0;
    LONG got;
    int found = 0, end = 0;
    BPTR file = Open((CONST_STRPTR)path, MODE_OLDFILE);

    if (!file)
        return 0;
    while (!found && !end) {
        size_t total, limit;
        got = Read(file, buffer + kept, BLOCK);
        if (got < 0)
            break;
        end = got == 0;
        total = kept + (size_t)got;
        /* Before the end, a tag in the last WINDOW bytes is seen next time. */
        limit = end ? total : (total > WINDOW ? total - WINDOW : 0);
        for (size_t i = 0; i < limit && i + tag <= total; i++)
            if (buffer[i] == '$' && !memcmp(buffer + i + 1, AS_VERSION_REST, tag - 1)) {
                size_t k = 0, at = i + tag;
                while (at + k < total && k + 1 < size && buffer[at + k] > ' ' && buffer[at + k] != '(')
                    k++;
                if (!k || buffer[at] < '0' || buffer[at] > '9')
                    continue; /* not a version string */
                if (size) {
                    memcpy(version, buffer + at, k);
                    version[k] = '\0';
                }
                found = 1;
                break;
            }
        kept = total - limit;
        memmove(buffer, buffer + limit, kept);
    }
    Close(file);
    return found;
}

static int program_path(char *out, size_t size)
{
    BPTR dir = GetProgramDir();
    return dir && NameFromLock(dir, (STRPTR)out, size) && AddPart((STRPTR)out, (CONST_STRPTR)program_name, size);
}

static int same(const char *a, const char *b)
{
    BPTR la = Lock((CONST_STRPTR)a, SHARED_LOCK), lb = Lock((CONST_STRPTR)b, SHARED_LOCK);
    int result = la && lb && SameLock(la, lb) == LOCK_SAME;
    if (la)
        UnLock(la);
    if (lb)
        UnLock(lb);
    return result;
}

/* The default tool of a launcher as a full path: as workbench.library
   locks it, from the current drawer of ExecuteStartup (SYS:WBStartup). */
static int tool_path(const char *tool, char *out, size_t size)
{
    if (!tool || !*tool)
        return 0;
    if (strchr(tool, ':'))
        return snprintf(out, size, "%s", tool) < (int)size;
    return join(out, size, AS_DIR, tool);
}

static int ends_info(const char *name)
{
    size_t n = strlen(name);
    return n > 5 && !strcasecmp(name + n - 5, ".info");
}

static int has_type(struct DiskObject *icon, const char *name)
{
    char key[32];
    snprintf(key, sizeof key, "%s", name);
    return icon->do_ToolTypes && FindToolType(icon->do_ToolTypes, (STRPTR)key) != NULL;
}

static void add(struct DockAutostart *a, struct found *f, const char *base, int kind, const char *target,
                const char *version)
{
    struct AsScan *s = &f->scan;
    if (s->count >= AS_MAX_ITEMS) {
        s->complete = 0; /* more than fit: not decided on a part */
        return;
    }
    snprintf(s->item[s->count].base, sizeof s->item[s->count].base, "%s", base);
    s->item[s->count].kind = kind;
    snprintf(f->target[s->count], sizeof f->target[s->count], "%s", target ? target : "");
    s->count++;
    if (kind == AS_OURS)
        a->ours = 1;
    else if ((kind == AS_COPY || kind == AS_COPY_FILE || kind == AS_OTHER || kind == AS_MISSING) && !a->found[0]) {
        snprintf(a->found, sizeof a->found, "%s", target ? target : "");
        snprintf(a->version, sizeof a->version, "%s", version ? version : "");
    } else if ((kind == AS_UNREADABLE || kind == AS_FOREIGN) && !a->problem[0] &&
               join(a->problem, sizeof a->problem, AS_DIR, base) && kind == AS_UNREADABLE &&
               strlen(a->problem) + 5 < sizeof a->problem)
        strcat(a->problem, ".info");
}

/* One icon in SYS:WBStartup: what it starts. */
static void look_at(struct DockAutostart *a, struct found *f, const char *base, const char *self)
{
    char path[AS_PATH], tool[AS_PATH], target[AS_PATH], version[32] = "";
    struct DiskObject *icon;
    int kind = 0;

    if (!join(path, sizeof path, AS_DIR, base)) {
        add(a, f, base, AS_UNREADABLE, NULL, NULL);
        return;
    }
    icon = GetDiskObject((CONST_STRPTR)path);
    if (!icon) {
        add(a, f, base, AS_UNREADABLE, NULL, NULL);
        return;
    }
    target[0] = '\0';
    if (icon->do_Type == WBPROJECT && tool_path((const char *)icon->do_DefaultTool, tool, sizeof tool)) {
        if (dockbar_version(tool, version, sizeof version)) {
            BPTR l = Lock((CONST_STRPTR)tool, SHARED_LOCK);
            if (!l || !NameFromLock(l, (STRPTR)target, sizeof target))
                snprintf(target, sizeof target, "%s", tool);
            if (l)
                UnLock(l);
            kind = same(tool, self) ? AS_OURS : AS_OTHER;
        } else if (has_type(icon, "AUTOSTART") && has_type(icon, "DONOTWAIT") && !exists(tool)) {
            /* Written by Dockbar for a program that is gone, or on a
               volume that is not there now. */
            kind = AS_MISSING;
            snprintf(target, sizeof target, "%s", tool);
        }
    } else if (icon->do_Type == WBTOOL && !strncasecmp(base, "dockbar", 7) &&
               dockbar_version(path, version, sizeof version)) {
        kind = AS_COPY;
        snprintf(target, sizeof target, "%s", path);
    }
    FreeDiskObject(icon);
    if (!kind && !strcasecmp(base, program_name))
        kind = AS_FOREIGN;
    if (kind)
        add(a, f, base, kind, target, version);
}

static int settings_here_and_there(const char *name, char *here, size_t hsize, char *there, size_t tsize)
{
    return join(here, hsize, "PROGDIR:", name) && join(there, tsize, AS_DIR, name);
}

static const char *const setting_names[] = {"docks.config", "dock.pins", "WidgetState"};
#define SETTING_COUNT 3

static int same_contents(const char *a, const char *b);

/* 1 when the files or drawers `a` and `b` are both missing or the same. */
static int same_tree(const char *a, const char *b, int depth)
{
    struct FileInfoBlock *fa, *fb;
    BPTR la = Lock((CONST_STRPTR)a, SHARED_LOCK), lb = Lock((CONST_STRPTR)b, SHARED_LOCK);
    int result = 0;

    if (!la || !lb) {
        result = !la && !lb;
        goto out_locks;
    }
    fa = AllocDosObject(DOS_FIB, NULL);
    fb = AllocDosObject(DOS_FIB, NULL);
    if (!fa || !fb || !Examine(la, fa) || !Examine(lb, fb) || depth > 8)
        goto out;
    if ((fa->fib_DirEntryType > 0) != (fb->fib_DirEntryType > 0))
        goto out;
    if (fa->fib_DirEntryType <= 0) {
        UnLock(la), UnLock(lb), la = lb = BNULL;
        result = same_contents(a, b);
        goto out;
    }
    {
        int count_a = 0, count_b = 0;
        result = 1;
        while (result && ExNext(la, fa)) {
            char x[AS_PATH], y[AS_PATH];
            count_a++;
            if (!join(x, sizeof x, a, (const char *)fa->fib_FileName) ||
                !join(y, sizeof y, b, (const char *)fa->fib_FileName) || !exists(y) || !same_tree(x, y, depth + 1))
                result = 0;
        }
        if (result && IoErr() != ERROR_NO_MORE_ENTRIES)
            result = 0;
        while (result && ExNext(lb, fb))
            count_b++;
        if (result && (IoErr() != ERROR_NO_MORE_ENTRIES || count_a != count_b))
            result = 0;
    }
out:
    if (fa)
        FreeDosObject(DOS_FIB, fa);
    if (fb)
        FreeDosObject(DOS_FIB, fb);
out_locks:
    if (la)
        UnLock(la);
    if (lb)
        UnLock(lb);
    return result;
}

/* The copy's settings against this drawer's; `differ` gets the names of
   those that are not the same. */
static int settings_plan(char *differ, size_t size)
{
    char here[AS_PATH], there[AS_PATH];
    int copy_has = 0, copy_config = 0, all_same = 1;

    if (size)
        differ[0] = '\0';
    for (int i = 0; i < SETTING_COUNT; i++) {
        if (!settings_here_and_there(setting_names[i], here, sizeof here, there, sizeof there))
            return DOCK_SETTINGS_DIFFER;
        if (exists(there)) {
            copy_has = 1;
            if (i == 0)
                copy_config = 1;
        }
    }
    if (!copy_has)
        return DOCK_SETTINGS_NONE;
    if (!dock_migrate_has_settings())
        return copy_config ? DOCK_SETTINGS_TAKE : DOCK_SETTINGS_LEFT;
    for (int i = 0; i < SETTING_COUNT; i++) {
        settings_here_and_there(setting_names[i], here, sizeof here, there, sizeof there);
        if (!same_tree(here, there, 0)) {
            all_same = 0;
            if (size && strlen(differ) + strlen(setting_names[i]) + 3 < size)
                strcat(strcat(differ, differ[0] ? ", " : ""), setting_names[i]);
        }
    }
    return all_same ? DOCK_SETTINGS_SAME : DOCK_SETTINGS_DIFFER;
}

static int scan(struct DockAutostart *a, struct found *f)
{
    struct FileInfoBlock *fib;
    char self[AS_PATH];
    BPTR dir;
    int copy = 0;

    memset(a, 0, sizeof *a);
    memset(f, 0, sizeof *f);
    f->scan.complete = 1;
    dir = Lock((CONST_STRPTR)AS_DIR, SHARED_LOCK);
    if (!dir || !program_path(self, sizeof self)) {
        if (dir)
            UnLock(dir);
        a->state = DOCK_AUTOSTART_UNKNOWN;
        return 0;
    }
    {
        BPTR home = GetProgramDir();
        if (home && SameLock(home, dir) == LOCK_SAME) {
            UnLock(dir);
            f->scan.self_copy = 1;
            a->state = DOCK_AUTOSTART_SELF_COPY;
            return 1;
        }
    }
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib || !Examine(dir, fib)) {
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(dir);
        a->state = DOCK_AUTOSTART_UNKNOWN;
        return 0;
    }
    for (;;) {
        char base[AS_NAME];
        const char *name = (const char *)fib->fib_FileName;
        size_t len;
        if (!ExNext(dir, fib)) {
            if (IoErr() != ERROR_NO_MORE_ENTRIES)
                f->scan.complete = 0;
            break;
        }
        len = strlen(name);
        if (fib->fib_DirEntryType > 0 || !ends_info(name))
            continue;
        if (len - 5 >= sizeof base) {
            add(a, f, "?", AS_UNREADABLE, NULL, NULL);
            continue;
        }
        memcpy(base, name, len - 5);
        base[len - 5] = '\0';
        look_at(a, f, base, self);
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(dir);

    /* An entry of the program's name without an icon of its own: the old
       program, or something that would be opened as the project file. */
    {
        char path[AS_PATH], version[32];
        int covered = 0;
        for (int i = 0; i < f->scan.count; i++)
            if (!strcasecmp(f->scan.item[i].base, program_name) &&
                (f->scan.item[i].kind == AS_COPY || f->scan.item[i].kind == AS_FOREIGN))
                covered = 1;
        if (!covered && join(path, sizeof path, AS_DIR, program_name) && exists(path)) {
            if (dockbar_version(path, version, sizeof version))
                add(a, f, program_name, AS_COPY_FILE, path, version);
            else
                add(a, f, program_name, AS_FOREIGN, NULL, NULL);
        }
    }

    for (int i = 0; i < f->scan.count; i++)
        if (f->scan.item[i].kind == AS_COPY || f->scan.item[i].kind == AS_COPY_FILE)
            copy = 1;
    if (copy) {
        a->settings = settings_plan(NULL, 0);
        a->copy_settings = a->settings != DOCK_SETTINGS_NONE;
    }
    {
        int blocked = 0, other = 0;
        for (int i = 0; i < f->scan.count; i++) {
            int k = f->scan.item[i].kind;
            if (k == AS_UNREADABLE || (k == AS_FOREIGN && !strcasecmp(f->scan.item[i].base, program_name)))
                blocked = 1;
            if (k == AS_OTHER || k == AS_MISSING)
                other = 1;
        }
        a->state = !f->scan.complete ? DOCK_AUTOSTART_INCOMPLETE : blocked ? DOCK_AUTOSTART_BLOCKED :
                   copy ? DOCK_AUTOSTART_COPY : other ? DOCK_AUTOSTART_OTHER :
                   a->ours ? DOCK_AUTOSTART_ON : DOCK_AUTOSTART_OFF;
    }
    return 1;
}

/* First line of S:User-Startup that names Dockbar, not counting comments and
   Assign lines (an Assign is the usual first step for a path with spaces). */
static int user_startup_line(void)
{
    char line[512];
    int number = 0;
    BPTR file = Open((CONST_STRPTR)"S:User-Startup", MODE_OLDFILE);

    if (!file)
        return 0;
    while (FGets(file, (STRPTR)line, sizeof line)) {
        const char *p = line;
        number++;
        while (*p == ' ' || *p == '\t')
            p++;
        /* A comment, or an Assign that only names Dockbar's drawer. */
        if (*p == ';' || (!strncasecmp(p, "assign", 6) && (p[6] == ' ' || p[6] == '\t')))
            continue;
        for (; *p; p++)
            if (!strncasecmp(p, "dockbar", 7)) {
                Close(file);
                return number;
            }
    }
    Close(file);
    return 0;
}

static void user_startup_notice(char *message, size_t size, int on)
{
    int line = user_startup_line();
    char number[16];
    snprintf(number, sizeof number, "%d", line);
    if (line && on)
        say(message, size, "S:User-Startup also starts Dockbar (line %s). That start comes first and "
            "this one then ends quietly; you can remove one of them.", number, NULL);
    else if (line)
        say(message, size, "S:User-Startup still starts Dockbar (line %s).", number, NULL);
}

int dock_autostart_check(struct DockAutostart *a)
{
    static struct found f;
    int ok = scan(a, &f);
    a->user_startup = user_startup_line();
    return ok;
}

static int same_contents(const char *a, const char *b)
{
    static char x[4096], y[4096];
    BPTR fa = Open((CONST_STRPTR)a, MODE_OLDFILE), fb = Open((CONST_STRPTR)b, MODE_OLDFILE);
    int result = fa && fb;
    LONG ga, gb;

    while (result) {
        ga = Read(fa, x, sizeof x);
        gb = Read(fb, y, sizeof y);
        if (ga != gb || ga < 0 || memcmp(x, y, (size_t)ga))
            result = 0;
        else if (ga == 0)
            break;
    }
    if (fa)
        Close(fa);
    if (fb)
        Close(fb);
    return result;
}

static int has_tooltype(char **types, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        size_t n = strlen(name);
        if (!strncasecmp(types[i], name, n) && (types[i][n] == '\0' || types[i][n] == '='))
            return 1;
    }
    return 0;
}

static void here_name(char *here, size_t size)
{
    BPTR dir = GetProgramDir();
    here[0] = '\0';
    if (dir)
        NameFromLock(dir, (STRPTR)here, size);
}

/* Why nothing was changed. */
static void refusal(const struct AsPlan *plan, const struct DockAutostart *a, const struct found *f,
                    char *message, size_t size)
{
    switch (plan->verdict) {
    case AS_REFUSE_SELF_COPY:
        say(message, size, "This Dockbar is a copy in " AS_DIR ". Start Dockbar from its own drawer "
            "and change this there.", NULL, NULL);
        break;
    case AS_REFUSE_INCOMPLETE:
        say(message, size, AS_DIR " could not be read to the end, so it is not known what starts there; "
            "nothing was changed.", NULL, NULL);
        break;
    case AS_REFUSE_UNREADABLE:
        say(message, size, "The icon %s could not be read, so it is not known what it starts; nothing was "
            "changed.", a->problem, NULL);
        break;
    case AS_REFUSE_BLOCKED:
        say(message, size, "%s is not a Dockbar start icon; nothing was changed.", a->problem, NULL);
        break;
    case AS_REFUSE_NEEDS_REPLACE: {
        int k = f->scan.item[plan->culprit].kind;
        const char *what = f->target[plan->culprit];
        if (k == AS_COPY || k == AS_COPY_FILE)
            say(message, size, "A copy of Dockbar starts with Wanderer: %s. Nothing was changed; "
                "choose Replace to start this Dockbar instead.", what, NULL);
        else if (k == AS_MISSING)
            say(message, size, "A start icon for a Dockbar that was not found is in " AS_DIR " (%s). "
                "Nothing was changed; choose Replace to start this Dockbar instead.", what, NULL);
        else
            say(message, size, "Wanderer starts another Dockbar: %s. Nothing was changed; choose Replace "
                "to start this Dockbar instead.", what, NULL);
        break;
    }
    default:
        break;
    }
}

int dock_autostart_enable(int replace, int migrate, int no_compose, char *message, size_t size,
                          void (*note)(const char *format, ...))
{
    static struct found f;
    struct DockAutostart a;
    struct AsPlan plan;
    char self[AS_PATH], launcher[AS_PATH];
    char *types[AS_MAX_TOOLTYPES + 1];
    static char type_text[AS_MAX_TOOLTYPES][128];
    int failed[AS_MAX_ITEMS] = {0};
    int kept = 0, ok, copy = -1, other = -1, any_failed = 0;
    struct DiskObject *icon, *old;

    if (size)
        message[0] = '\0';
    if (!scan(&a, &f) || a.state == DOCK_AUTOSTART_UNKNOWN) {
        say(message, size, AS_DIR " or this program was not found; nothing was changed.", NULL, NULL);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    if (!as_plan_make(&f.scan, replace ? AS_REPLACE : AS_ON, program_name, &plan)) {
        refusal(&plan, &a, &f, message, size);
        if (note)
            note("AUTOSTART not changed verdict=%d", plan.verdict);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    if (!program_path(self, sizeof self) || !join(launcher, sizeof launcher, AS_DIR, program_name))
        return DOCK_AUTOSTART_UNCHANGED;
    for (int i = 0; i < f.scan.count; i++) {
        int k = f.scan.item[i].kind;
        if ((k == AS_COPY || k == AS_COPY_FILE) && copy < 0)
            copy = i;
        if ((k == AS_OTHER || k == AS_MISSING) && other < 0)
            other = i;
    }

    /* The copy's settings first: if that fails nothing else changes. */
    if (copy >= 0) {
        char differ[64], here[AS_PATH];
        int settings = settings_plan(differ, sizeof differ);
        here_name(here, sizeof here);
        if (settings == DOCK_SETTINGS_TAKE) {
            if (!migrate) {
                say(message, size, "A Dockbar is running; quit it first, so that the copy's docks are "
                    "taken over complete. Nothing was changed.", NULL, NULL);
                return DOCK_AUTOSTART_UNCHANGED;
            }
            if (dock_migrate_from_dir(AS_DIR, note) <= 0) {
                say(message, size, "The copy's settings could not be copied; nothing was changed.", NULL, NULL);
                return DOCK_AUTOSTART_UNCHANGED;
            }
            say(message, size, "The docks, pinned items and widget settings of the copy were copied into %s; "
                "the copy's files stay in " AS_DIR ".", here, NULL);
        } else if (settings == DOCK_SETTINGS_DIFFER) {
            say(message, size, "This drawer (%s) has settings of its own (%s differ from the copy's), so the "
                "copy's were not taken over. They are still in " AS_DIR ".", here, differ);
            say(message, size, "To use the copy's instead: quit Dockbar, copy docks.config, dock.pins and "
                "WidgetState (those that are there) from " AS_DIR " into %s, and start Dockbar.", here, NULL);
        } else if (settings == DOCK_SETTINGS_LEFT) {
            say(message, size, "The copy's widget settings were not taken over; they are still in " AS_DIR
                ". To use them: quit Dockbar, copy dock.pins and WidgetState (those that are there) from "
                AS_DIR " into %s, and start Dockbar.", here, NULL);
        }
    }

    /* Tool types of the start icon there are kept. */
    if (plan.types_from >= 0) {
        char path[AS_PATH];
        if (join(path, sizeof path, AS_DIR, f.scan.item[plan.types_from].base) &&
            (old = GetDiskObject((CONST_STRPTR)path))) {
            for (STRPTR *t = old->do_ToolTypes; t && *t && kept < AS_MAX_TOOLTYPES - 3; t++)
                if (strlen((const char *)*t) < sizeof type_text[0])
                    snprintf(type_text[kept], sizeof type_text[kept], "%s", (const char *)*t), kept++;
            FreeDiskObject(old);
        }
    }
    for (int i = 0; i < kept; i++)
        types[i] = type_text[i];
    if (!has_tooltype(types, kept, "DONOTWAIT"))
        snprintf(type_text[kept], sizeof type_text[kept], "DONOTWAIT"), types[kept] = type_text[kept], kept++;
    if (!has_tooltype(types, kept, "AUTOSTART"))
        snprintf(type_text[kept], sizeof type_text[kept], "AUTOSTART"), types[kept] = type_text[kept], kept++;
    if (no_compose && !has_tooltype(types, kept, "NOCOMPOSE"))
        snprintf(type_text[kept], sizeof type_text[kept], "NOCOMPOSE"), types[kept] = type_text[kept], kept++;
    types[kept] = NULL;

    /* The program's own icon gives the picture and the stack. */
    icon = GetDiskObject((CONST_STRPTR)self);
    if (!icon)
        icon = GetDefDiskObject(WBPROJECT);
    if (!icon) {
        say(message, size, "No icon could be made; the start icon was not written.", NULL, NULL);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    {
        UBYTE type = icon->do_Type;
        STRPTR tool = icon->do_DefaultTool;
        STRPTR *tooltypes = icon->do_ToolTypes;
        LONG x = icon->do_CurrentX, y = icon->do_CurrentY;
        icon->do_Type = WBPROJECT;
        icon->do_DefaultTool = (STRPTR)self;
        icon->do_ToolTypes = (STRPTR *)types;
        icon->do_CurrentX = icon->do_CurrentY = NO_ICON_POSITION;
        ok = PutDiskObject((CONST_STRPTR)launcher, icon) != FALSE;
        icon->do_Type = type;
        icon->do_DefaultTool = tool;
        icon->do_ToolTypes = tooltypes;
        icon->do_CurrentX = x;
        icon->do_CurrentY = y;
        FreeDiskObject(icon);
    }
    if (!ok) {
        if (note)
            note("AUTOSTART write failed %s.info error=%ld", launcher, (long)IoErr());
        say(message, size, AS_DIR "/%s.info could not be written; nothing else was changed.", program_name, NULL);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    if (note)
        note("AUTOSTART launcher %s.info -> %s", launcher, self);

    /* What started a Dockbar before goes now; the copy's settings stay. */
    for (int i = 0; i < f.scan.count; i++) {
        char path[AS_PATH], info[AS_PATH];
        if (!plan.remove[i])
            continue;
        if (!join(path, sizeof path, AS_DIR, f.scan.item[i].base) ||
            snprintf(info, sizeof info, "%s.info", path) >= (int)sizeof info) {
            failed[i] = plan.remove[i];
            continue;
        }
        if ((plan.remove[i] & AS_RM_ICON) && !DeleteFile((CONST_STRPTR)info))
            failed[i] |= AS_RM_ICON;
        if ((plan.remove[i] & AS_RM_FILE) && !DeleteFile((CONST_STRPTR)path))
            failed[i] |= AS_RM_FILE;
        if (note)
            note("AUTOSTART remove %s parts=%d failed=%d", path, plan.remove[i], failed[i]);
        if (failed[i])
            any_failed = 1;
    }
    if (any_failed) {
        say(message, size, "The start icon for this Dockbar was written, but not everything it replaces "
            "could be removed:", NULL, NULL);
        for (int i = 0; i < f.scan.count; i++) {
            char path[AS_PATH];
            if (!failed[i] || !join(path, sizeof path, AS_DIR, f.scan.item[i].base))
                continue;
            if (as_still_starts(&f.scan, &plan, failed, i, program_name))
                say(message, size, "- %s.info still starts with Wanderer: delete it yourself.", path, NULL);
            if (failed[i] & AS_RM_FILE)
                say(message, size, "- %s (the old program) is still there; it no longer starts, delete it "
                    "yourself.", path, NULL);
        }
    } else if (copy >= 0)
        say(message, size, "The copy of Dockbar %s in " AS_DIR " was replaced: Wanderer now starts %s.",
            a.version, self);
    else if (other >= 0)
        say(message, size, "Wanderer now starts %s instead of %s.", self, f.target[other]);
    user_startup_notice(message, size, 1);
    return any_failed ? DOCK_AUTOSTART_PARTIAL : DOCK_AUTOSTART_DONE;
}

int dock_autostart_disable(char *message, size_t size, void (*note)(const char *format, ...))
{
    static struct found f;
    struct DockAutostart a;
    struct AsPlan plan;
    int removed = 0, failed = 0, left = 0;

    if (size)
        message[0] = '\0';
    if (!scan(&a, &f) || a.state == DOCK_AUTOSTART_UNKNOWN) {
        say(message, size, AS_DIR " was not found; nothing was changed.", NULL, NULL);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    if (!as_plan_make(&f.scan, AS_OFF, program_name, &plan)) {
        say(message, size, "This Dockbar is a copy in " AS_DIR "; delete it there to stop it starting.", NULL, NULL);
        return DOCK_AUTOSTART_UNCHANGED;
    }
    for (int i = 0; i < f.scan.count; i++) {
        char info[AS_PATH];
        if (!(plan.remove[i] & AS_RM_ICON))
            continue;
        if (!join(info, sizeof info, AS_DIR, f.scan.item[i].base) || strlen(info) + 5 >= sizeof info ||
            !DeleteFile((CONST_STRPTR)strcat(info, ".info"))) {
            say(message, size, "%s could not be deleted; it still starts this Dockbar.", info, NULL);
            failed++;
        } else {
            removed++;
            if (note)
                note("AUTOSTART removed launcher %s", info);
        }
    }
    /* Other installations' start icons and copies are theirs. */
    for (int i = 0; i < f.scan.count; i++) {
        int k = f.scan.item[i].kind;
        if (k == AS_OTHER || k == AS_MISSING)
            say(message, size, "%s still starts with Wanderer; it is not this Dockbar's and was left alone.",
                f.target[i], NULL), left = 1;
        else if (k == AS_COPY)
            say(message, size, "A copy of Dockbar in " AS_DIR " still starts with Wanderer: %s.", f.target[i],
                NULL), left = 1;
    }
    if (left)
        say(message, size, "To start this Dockbar instead, turn Start with Wanderer on and choose Replace.",
            NULL, NULL);
    if (!f.scan.complete)
        say(message, size, AS_DIR " could not be read to the end; other start icons of this Dockbar may "
            "still be there.", NULL, NULL);
    user_startup_notice(message, size, 0);
    if (failed)
        return removed ? DOCK_AUTOSTART_PARTIAL : DOCK_AUTOSTART_UNCHANGED;
    return f.scan.complete ? DOCK_AUTOSTART_DONE : DOCK_AUTOSTART_PARTIAL;
}
