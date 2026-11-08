/* Settings of a package this one replaced; see migrate.h. */
#include "migrate.h"

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MIGRATE_PATH 512
#define MIGRATE_DEPTH 8
#define MIGRATE_SUFFIX ".migrating"

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

/* Removes `path` and, for a drawer, everything in it. Used only for this
   module's own MIGRATE_SUFFIX copies. */
static int remove_tree(const char *path, int depth)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    int ok = 1;

    if (depth > MIGRATE_DEPTH)
        return 0;
    lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (!lock)
        return 1;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return 0;
    }
    if (Examine(lock, fib) && fib->fib_DirEntryType > 0) {
        /* Take the first entry again after every deletion: ExNext must not
           continue over a drawer that changed. */
        for (;;) {
            char child[MIGRATE_PATH];
            if (!Examine(lock, fib) || !ExNext(lock, fib))
                break;
            if (!join(child, sizeof child, path, (const char *)fib->fib_FileName) ||
                !remove_tree(child, depth + 1)) {
                ok = 0;
                break;
            }
        }
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    if (ok && !DeleteFile((CONST_STRPTR)path))
        ok = 0;
    return ok;
}

static int copy_file(const char *from, const char *to, const struct FileInfoBlock *info)
{
    static UBYTE buffer[4096];
    BPTR in = Open((CONST_STRPTR)from, MODE_OLDFILE), out;
    LONG got;
    int ok = 1;

    if (!in)
        return 0;
    out = Open((CONST_STRPTR)to, MODE_NEWFILE);
    if (!out) {
        Close(in);
        return 0;
    }
    while ((got = Read(in, buffer, sizeof buffer)) > 0)
        if (Write(out, buffer, got) != got) {
            ok = 0;
            break;
        }
    if (got < 0)
        ok = 0;
    Close(in);
    if (!Close(out))
        ok = 0;
    if (ok && info) {
        SetFileDate((CONST_STRPTR)to, (struct DateStamp *)&info->fib_Date);
        SetProtection((CONST_STRPTR)to, info->fib_Protection);
    }
    return ok;
}

static int copy_tree(const char *from, const char *to, int depth)
{
    struct FileInfoBlock *fib;
    BPTR lock, made;
    int ok = 1;

    if (depth > MIGRATE_DEPTH)
        return 0;
    lock = Lock((CONST_STRPTR)from, SHARED_LOCK);
    if (!lock)
        return 0;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib || !Examine(lock, fib)) {
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return 0;
    }
    if (fib->fib_DirEntryType <= 0) {
        UnLock(lock);
        ok = copy_file(from, to, fib);
        FreeDosObject(DOS_FIB, fib);
        return ok;
    }
    made = CreateDir((CONST_STRPTR)to);
    if (!made)
        ok = 0;
    else
        UnLock(made);
    while (ok && ExNext(lock, fib)) {
        char a[MIGRATE_PATH], b[MIGRATE_PATH];
        if (!join(a, sizeof a, from, (const char *)fib->fib_FileName) ||
            !join(b, sizeof b, to, (const char *)fib->fib_FileName) ||
            !copy_tree(a, b, depth + 1))
            ok = 0;
    }
    if (ok && IoErr() != ERROR_NO_MORE_ENTRIES)
        ok = 0;
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return ok;
}

struct item {
    const char *name;
    int found;
    char from[MIGRATE_PATH], staged[MIGRATE_PATH], final[MIGRATE_PATH];
};

int dock_migrate_has_settings(void)
{
    return exists("PROGDIR:docks.config") || exists("PROGDIR:dock.config") ||
           exists("PROGDIR:dock.pins") || exists("PROGDIR:WidgetState");
}

int dock_migrate_from_sibling(const char *new_name, const char *old_name,
                              void (*note)(const char *format, ...))
{
    char here[MIGRATE_PATH], old[MIGRATE_PATH];
    BPTR progdir = GetProgramDir();

    if (!progdir || !NameFromLock(progdir, (STRPTR)here, sizeof here))
        return 0;
    if (strcasecmp((const char *)FilePart((STRPTR)here), new_name) != 0)
        return 0;
    snprintf(old, sizeof old, "%s", here);
    *PathPart((STRPTR)old) = '\0';
    if (!AddPart((STRPTR)old, (CONST_STRPTR)old_name, sizeof old))
        return 0;
    return dock_migrate_from_dir(old, note);
}

int dock_migrate_from_dir(const char *old, void (*note)(const char *format, ...))
{
    /* docks.config last: once it is in place the migration is done, and an
       interrupted one is started again from the beginning. */
    struct item items[] = {{"WidgetState", 0, "", "", ""}, {"dock.pins", 0, "", "", ""},
                           {"docks.config", 0, "", "", ""}};
    const int count = (int)(sizeof items / sizeof items[0]);
    int i, ok = 1;

    if (dock_migrate_has_settings())
        return 0;
    for (i = 0; i < count; i++) {
        if (!join(items[i].from, sizeof items[i].from, old, items[i].name) ||
            !join(items[i].final, sizeof items[i].final, "PROGDIR:", items[i].name) ||
            snprintf(items[i].staged, sizeof items[i].staged, "%s" MIGRATE_SUFFIX, items[i].final) >=
                (int)sizeof items[i].staged)
            return 0;
        items[i].found = exists(items[i].from);
    }
    if (!items[count - 1].found)
        return 0;

    note("MIGRATE settings from %s", old);
    for (i = 0; ok && i < count; i++) {
        if (!items[i].found)
            continue;
        remove_tree(items[i].staged, 0);
        if (!copy_tree(items[i].from, items[i].staged, 0)) {
            note("MIGRATE FAIL copying %s (error %ld)", items[i].name, (long)IoErr());
            ok = 0;
        }
    }
    for (i = 0; ok && i < count; i++)
        if (items[i].found && !Rename((CONST_STRPTR)items[i].staged, (CONST_STRPTR)items[i].final)) {
            note("MIGRATE FAIL renaming %s (error %ld)", items[i].name, (long)IoErr());
            ok = 0;
        }
    if (!ok) {
        /* Nothing half-copied stays: what was renamed already is removed
           again (docks.config, renamed last, never is). */
        for (i = 0; i < count; i++)
            if (items[i].found) {
                remove_tree(items[i].staged, 0);
                if (i < count - 1)
                    remove_tree(items[i].final, 0);
            }
        return -1;
    }
    note("MIGRATE done: the settings in %s were copied; that drawer was not changed", old);
    return 1;
}
