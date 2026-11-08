/* Starting Dockbar with Wanderer; see autostart.h. */
#include "autostart.h"
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
#define AS_MAX_ENTRIES 16
#define AS_MAX_TOOLTYPES 24
/* "$VER: Dockbar " without the "$": a whole tag here would be found in
   this program by Version and by dockbar_version() itself. */
#define AS_VERSION_REST "VER: Dockbar "

enum { KIND_LAUNCHER_OURS = 1, KIND_LAUNCHER_OTHER, KIND_COPY };

struct entry {
    char base[108]; /* name in SYS:WBStartup without .info */
    int kind;
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

static int scan(struct DockAutostart *a, struct entry *entries, int *count)
{
    struct FileInfoBlock *fib;
    char self[AS_PATH];
    BPTR dir;
    int n = 0, have_copy = 0, have_other = 0, have_ours = 0, name_taken = 0;

    memset(a, 0, sizeof *a);
    *count = 0;
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
    while (ExNext(dir, fib)) {
        const char *name = (const char *)fib->fib_FileName;
        char base[108], path[AS_PATH], tool[AS_PATH], version[32] = "";
        struct DiskObject *icon;
        int kind = 0;
        size_t len = strlen(name);

        if (fib->fib_DirEntryType > 0 || !ends_info(name) || len - 5 >= sizeof base)
            continue;
        memcpy(base, name, len - 5);
        base[len - 5] = '\0';
        if (!join(path, sizeof path, AS_DIR, base))
            continue;
        icon = GetDiskObject((CONST_STRPTR)path);
        if (!icon)
            continue;
        if (icon->do_Type == WBPROJECT && tool_path((const char *)icon->do_DefaultTool, tool, sizeof tool) &&
            dockbar_version(tool, version, sizeof version)) {
            if (same(tool, self))
                kind = KIND_LAUNCHER_OURS, have_ours = 1;
            else {
                kind = KIND_LAUNCHER_OTHER;
                if (!have_other && !have_copy) {
                    BPTR l = Lock((CONST_STRPTR)tool, SHARED_LOCK);
                    if (!l || !NameFromLock(l, (STRPTR)a->found, sizeof a->found))
                        snprintf(a->found, sizeof a->found, "%s", tool);
                    if (l)
                        UnLock(l);
                    snprintf(a->version, sizeof a->version, "%s", version);
                }
                have_other = 1;
            }
        } else if (icon->do_Type == WBTOOL && !strncasecmp(base, "dockbar", 7) &&
                   dockbar_version(path, version, sizeof version)) {
            kind = KIND_COPY;
            if (!have_copy) {
                snprintf(a->found, sizeof a->found, "%s", path);
                snprintf(a->version, sizeof a->version, "%s", version);
            }
            have_copy = 1;
        }
        FreeDiskObject(icon);
        if (!kind) {
            if (!strcasecmp(base, program_name))
                name_taken = 1;
            continue;
        }
        if (n < AS_MAX_ENTRIES) {
            snprintf(entries[n].base, sizeof entries[n].base, "%s", base);
            entries[n].kind = kind;
            n++;
        }
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(dir);
    /* A file of that name without an icon would be the project file. */
    if (!name_taken) {
        char path[AS_PATH];
        int ours_named = 0;
        for (int i = 0; i < n; i++)
            if (!strcasecmp(entries[i].base, program_name))
                ours_named = 1;
        if (!ours_named && join(path, sizeof path, AS_DIR, program_name) && exists(path))
            name_taken = 1;
    }
    if (have_copy) {
        char settings[AS_PATH];
        a->copy_settings = join(settings, sizeof settings, AS_DIR, "docks.config") && exists(settings);
    }
    /* A copy or a launcher named like ours is replaced; anything else of
       that name is not, so no launcher can be written. */
    a->state = name_taken ? DOCK_AUTOSTART_BLOCKED : have_copy ? DOCK_AUTOSTART_COPY :
               have_other ? DOCK_AUTOSTART_OTHER : have_ours ? DOCK_AUTOSTART_ON : DOCK_AUTOSTART_OFF;
    *count = n;
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

int dock_autostart_check(struct DockAutostart *a)
{
    struct entry entries[AS_MAX_ENTRIES];
    int count, ok = scan(a, entries, &count);
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

static void settings_notice(char *message, size_t size)
{
    char here[AS_PATH] = "";
    BPTR dir = GetProgramDir();
    if (dir)
        NameFromLock(dir, (STRPTR)here, sizeof here);
    say(message, size, "This drawer (%s) has settings of its own, so the copy's were not taken over. "
        "They are still in " AS_DIR ".", here, NULL);
    say(message, size, "To use the copy's docks instead: quit Dockbar, copy docks.config, dock.pins and "
        "WidgetState (those that are there) from " AS_DIR " into %s, and start Dockbar.", here, NULL);
}

int dock_autostart_enable(int migrate, int no_compose, char *message, size_t size,
                          void (*note)(const char *format, ...))
{
    struct DockAutostart a;
    struct entry entries[AS_MAX_ENTRIES];
    char self[AS_PATH], launcher[AS_PATH];
    char *types[AS_MAX_TOOLTYPES + 1];
    static char type_text[AS_MAX_TOOLTYPES][128];
    int count, kept = 0, ok;
    struct DiskObject *icon, *old;

    if (size)
        message[0] = '\0';
    if (!scan(&a, entries, &count) || a.state == DOCK_AUTOSTART_UNKNOWN) {
        say(message, size, AS_DIR " or this program was not found; nothing was changed.", NULL, NULL);
        return 0;
    }
    if (a.state == DOCK_AUTOSTART_SELF_COPY) {
        say(message, size, "This Dockbar is a copy in " AS_DIR ". Start Dockbar from its own drawer "
            "and turn this on there.", NULL, NULL);
        return 0;
    }
    if (a.state == DOCK_AUTOSTART_BLOCKED) {
        say(message, size, AS_DIR "/%s is not a Dockbar start icon; nothing was changed.", program_name, NULL);
        return 0;
    }
    if (!program_path(self, sizeof self) || !join(launcher, sizeof launcher, AS_DIR, program_name))
        return 0;

    /* The copy's settings first: if that fails nothing else changes. */
    if (a.state == DOCK_AUTOSTART_COPY && a.copy_settings) {
        char theirs[AS_PATH];
        join(theirs, sizeof theirs, AS_DIR, "docks.config");
        if (!dock_migrate_has_settings()) {
            if (!migrate) {
                say(message, size, "A Dockbar is running; quit it first, so that the copy's docks are "
                    "taken over complete. Nothing was changed.", NULL, NULL);
                return 0;
            }
            if (dock_migrate_from_dir(AS_DIR, note) < 0) {
                say(message, size, "The copy's settings could not be copied; nothing was changed.", NULL, NULL);
                return 0;
            }
            say(message, size, "The docks of the copy in " AS_DIR " were taken over.", NULL, NULL);
        } else if (!same_contents(theirs, "PROGDIR:docks.config"))
            settings_notice(message, size);
    }

    /* Tool types of an existing launcher are kept. */
    for (int i = 0; i < count && !kept; i++)
        if (entries[i].kind != KIND_COPY) {
            char path[AS_PATH];
            if (!join(path, sizeof path, AS_DIR, entries[i].base) || !(old = GetDiskObject((CONST_STRPTR)path)))
                continue;
            for (STRPTR *t = old->do_ToolTypes; t && *t && kept < AS_MAX_TOOLTYPES - 3; t++)
                if (strlen((const char *)*t) < sizeof type_text[0])
                    snprintf(type_text[kept], sizeof type_text[kept], "%s", (const char *)*t), kept++;
            FreeDiskObject(old);
            if (!kept)
                kept = -1; /* a launcher without tool types */
        }
    if (kept < 0)
        kept = 0;
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
        say(message, size, "No icon could be made; nothing else was changed.", NULL, NULL);
        return 0;
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
        say(message, size, AS_DIR "/%s.info could not be written.", program_name, NULL);
        if (note)
            note("AUTOSTART write failed %s.info error=%ld", launcher, (long)IoErr());
        return 0;
    }
    if (note)
        note("AUTOSTART launcher %s.info -> %s", launcher, self);

    /* What started Dockbar before goes now; the copy's settings stay. */
    for (int i = 0; i < count; i++) {
        char path[AS_PATH], info[AS_PATH];
        int named = !strcasecmp(entries[i].base, program_name);
        if (!join(path, sizeof path, AS_DIR, entries[i].base) ||
            snprintf(info, sizeof info, "%s.info", path) >= (int)sizeof info)
            continue;
        if (entries[i].kind == KIND_COPY) {
            if (!DeleteFile((CONST_STRPTR)path))
                say(message, size, "The copy %s could not be deleted; delete it yourself.", path, NULL);
            else if (note)
                note("AUTOSTART removed copy %s", path);
        }
        if (!named && !DeleteFile((CONST_STRPTR)info))
            say(message, size, "%s could not be deleted; delete it yourself.", info, NULL);
    }
    if (a.state == DOCK_AUTOSTART_COPY)
        say(message, size, "The copy of Dockbar %s in " AS_DIR " was replaced: Wanderer now starts %s.",
            a.version, self);
    {
        int line = user_startup_line();
        char number[16];
        snprintf(number, sizeof number, "%d", line);
        if (line)
            say(message, size, "S:User-Startup also starts Dockbar (line %s). That start comes first and "
                "this one then ends quietly; you can remove one of them.", number, NULL);
    }
    return 1;
}

int dock_autostart_disable(char *message, size_t size, void (*note)(const char *format, ...))
{
    struct DockAutostart a;
    struct entry entries[AS_MAX_ENTRIES];
    int count, ok = 1;

    if (size)
        message[0] = '\0';
    if (!scan(&a, entries, &count) || a.state == DOCK_AUTOSTART_UNKNOWN) {
        say(message, size, AS_DIR " was not found; nothing was changed.", NULL, NULL);
        return 0;
    }
    if (a.state == DOCK_AUTOSTART_SELF_COPY) {
        say(message, size, "This Dockbar is a copy in " AS_DIR "; delete it there to stop it starting.", NULL, NULL);
        return 0;
    }
    for (int i = 0; i < count; i++) {
        char info[AS_PATH];
        if (entries[i].kind == KIND_COPY || !join(info, sizeof info, AS_DIR, entries[i].base) ||
            strlen(info) + 5 >= sizeof info)
            continue;
        strcat(info, ".info");
        if (!DeleteFile((CONST_STRPTR)info)) {
            say(message, size, "%s could not be deleted.", info, NULL);
            ok = 0;
        } else if (note)
            note("AUTOSTART removed launcher %s", info);
    }
    if (a.state == DOCK_AUTOSTART_COPY)
        say(message, size, "A copy of Dockbar %s in " AS_DIR " still starts with Wanderer: %s.", a.version, a.found);
    {
        int line = user_startup_line();
        char number[16];
        snprintf(number, sizeof number, "%d", line);
        if (line)
            say(message, size, "S:User-Startup still starts Dockbar (line %s).", number, NULL);
    }
    return ok;
}
