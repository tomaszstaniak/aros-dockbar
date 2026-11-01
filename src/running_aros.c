#include "running.h"
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/bptr.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

/* What may be read of another process, and when.
 *
 * The walk runs under Forbid(): on the uniprocessor exec of the pc builds
 * no other task runs until Permit(), so no process can leave the task lists
 * or free its structures meanwhile, as long as nothing in between waits.
 * Nothing does: only copies, and TypeOfMem(). Every value is copied there;
 * after Permit() only the copies are used, and lock fields are compared as
 * numbers, never followed.
 *
 * A process that is ending stays in the lists while its exit code waits for
 * packets, and some of what it points to is freed by then: its home drawer
 * lock is unlocked before the task goes (rom/dos/createnewproc.c, both
 * trees, without clearing pr_HomeDir), and its CLI structure with the
 * command name is freed before pr_CLI is cleared (FreeDosObject, then
 * UnLock of the path, which waits). Such a pointer can lead into freed
 * memory: each one is checked to lie in system memory before it is read,
 * and text is copied with a bound, so a stale pointer reads harmless bytes
 * and cannot fault or run on. What it reads can only be the ending
 * program's old values or garbage that would have to match three lock
 * fields and a name; the next scan no longer sees the process.
 *
 * An SMP exec would let other processors run during the walk, and Forbid()
 * would not protect the lists; that is not supported. */

static int readable(const void *p)
{
    return p && TypeOfMem((APTR)p) != 0;
}

static void copy_text(char *to, const char *from, int size)
{
    int n = 0;
    if (readable(from))
        while (n < size - 1 && from[n]) {
            to[n] = from[n];
            n++;
        }
    to[n] = 0;
}

static int copy_process(struct Task *t, struct DockRunProcess *p)
{
    struct Process *pr = (struct Process *)t;
    if (t->tc_Node.ln_Type != NT_PROCESS) return 0;
    memset(p, 0, sizeof *p);
    if (pr->pr_CLI) {
        /* A Shell: its program is the command it runs now, if any. */
        struct CommandLineInterface *cli = BADDR(pr->pr_CLI);
        if (readable(cli) && cli->cli_Module && cli->cli_CommandName)
            copy_text(p->name, (const char *)AROS_BSTR_ADDR(cli->cli_CommandName), sizeof p->name);
    } else copy_text(p->name, t->tc_Node.ln_Name, sizeof p->name);
    if (pr->pr_HomeDir) {
        struct FileLock *fl = BADDR(pr->pr_HomeDir);
        if (readable(fl)) {
            p->has_home = 1;
            p->volume = (unsigned long)(IPTR)fl->fl_Volume;
            p->key = (unsigned long)fl->fl_Key;
            p->port = (unsigned long)(IPTR)fl->fl_Task;
        }
    }
    return 1;
}

int dock_running_scan(struct DockRunProcess *seen, int max)
{
    struct Node *n;
    int count = 0;
    Forbid();
    if (count < max) count += copy_process(FindTask(NULL), &seen[count]);
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ && count < max; n = n->ln_Succ)
        count += copy_process((struct Task *)n, &seen[count]);
    for (n = SysBase->TaskWait.lh_Head; n->ln_Succ && count < max; n = n->ln_Succ)
        count += copy_process((struct Task *)n, &seen[count]);
    Permit();
    /* Names as items name them: the file, without the path a Shell command
     * was given with. */
    for (int i = 0; i < count; i++) {
        const char *file = (const char *)FilePart((STRPTR)seen[i].name);
        if (file != seen[i].name) memmove(seen[i].name, file, strlen(file) + 1);
    }
    return count;
}

void dock_running_target(struct DockRunTarget *t, const char *path)
{
    char drawer[512];
    memset(t, 0, sizeof *t);
    if (!path || !*path) return;
    const char *file = (const char *)FilePart((STRPTR)path);
    int len = (int)((const char *)PathPart((STRPTR)path) - path);
    if (!*file || strlen(file) >= sizeof t->file || len < 0 || len >= (int)sizeof drawer) return;
    strcpy(t->file, file);
    memcpy(drawer, path, len);
    drawer[len] = 0;
    /* "Clock" alone: the current drawer, which is not where it was found. */
    if (!len) return;
    BPTR lock = Lock((STRPTR)drawer, SHARED_LOCK);
    if (!lock) return;
    struct FileLock *fl = BADDR(lock);
    BPTR parent = ParentDir(lock);
    unsigned long parent_key = parent ? (unsigned long)((struct FileLock *)BADDR(parent))->fl_Key : 0;
    t->volume = (unsigned long)(IPTR)fl->fl_Volume;
    t->key = (unsigned long)fl->fl_Key;
    t->port = (unsigned long)(IPTR)fl->fl_Task;
    t->usable = dock_running_keys_usable(t->key, parent != BNULL, parent_key);
    if (parent) UnLock(parent);
    UnLock(lock);
}
