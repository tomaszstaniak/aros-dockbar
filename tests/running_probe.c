/* Development probe, not shipped: which programs of a dock are running?
 *   running_probe LOG PATH...
 * One scan of the system's processes, the way a running indicator would do
 * it, and for each PATH (a program file) what ties a process to it.
 *
 * Under Forbid() it walks the ready and waiting lists (and itself) and copies
 * plain fields only: the task name, for a Shell process the command it runs
 * now (cli_Module set, cli_CommandName), and the home drawer's lock fields
 * (fl_Volume, fl_Key). No DOS call touches another process's lock: SameLock()
 * would send a packet, which breaks Forbid(), and the lock can be freed
 * meanwhile (a Shell unlocks its home drawer after each command). After
 * Permit() the copied fields are compared with a lock of our own on the
 * item's drawer, field by field, as SameLock()'s fallback does.
 *
 * Verdicts per PATH:
 *   confirmed  home drawer and file name both match
 *   name-only  a process has the file name, in another drawer (or none)
 *   home-only  a process has the drawer, under another name
 *   none
 * LOG gets the process table too, and the cost of 1000 scans. */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/bptr.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MAXP 160
struct Seen {
    char name[64], cmd[108];
    int cli, running;              /* a Shell; running a command now */
    int has_home;
    BPTR volume;
    IPTR key;
};
static struct Seen seen[MAXP];
static int count;

static void copy_task(struct Task *t)
{
    struct Process *pr = (struct Process *)t;
    struct Seen *s;
    if (count >= MAXP || t->tc_Node.ln_Type != NT_PROCESS) return;
    s = &seen[count++];
    memset(s, 0, sizeof *s);
    if (t->tc_Node.ln_Name) strncpy(s->name, t->tc_Node.ln_Name, sizeof s->name - 1);
    if (pr->pr_CLI) {
        struct CommandLineInterface *cli = BADDR(pr->pr_CLI);
        s->cli = 1;
        s->running = cli->cli_Module != BNULL;
        if (cli->cli_CommandName) {
            int n = strlen((const char *)AROS_BSTR_ADDR(cli->cli_CommandName));
            if (n > (int)sizeof s->cmd - 1) n = sizeof s->cmd - 1;
            memcpy(s->cmd, AROS_BSTR_ADDR(cli->cli_CommandName), n);
            s->cmd[n] = 0;
        }
    }
    if (pr->pr_HomeDir) {
        struct FileLock *fl = BADDR(pr->pr_HomeDir);
        s->has_home = 1;
        s->volume = fl->fl_Volume;
        s->key = fl->fl_Key;
    }
}

static void scan(void)
{
    struct Node *n;
    count = 0;
    Forbid();
    copy_task(FindTask(NULL));
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ) copy_task((struct Task *)n);
    for (n = SysBase->TaskWait.lh_Head; n->ln_Succ; n = n->ln_Succ) copy_task((struct Task *)n);
    Permit();
}

/* The name a process gives for its program: a Shell's current command, or
 * the task name; the last path part, as the item's file name is compared. */
static const char *program_of(const struct Seen *s)
{
    const char *name = s->cli ? (s->running ? s->cmd : "") : s->name;
    return (const char *)FilePart((STRPTR)name);
}

int main(int argc, char **argv)
{
    FILE *log;
    struct DateStamp a, b;
    if (argc < 3 || !(log = fopen(argv[1], "w"))) return 20;
    scan();
    fprintf(log, "processes %d\n", count);
    for (int i = 0; i < count; i++)
        fprintf(log, "  [%s] cli=%d running=%d cmd=[%s] home=%s vol=%p key=%lx\n", seen[i].name, seen[i].cli,
                seen[i].running, seen[i].cmd, seen[i].has_home ? "yes" : "no", (void *)seen[i].volume,
                (unsigned long)seen[i].key);
    for (int p = 2; p < argc; p++) {
        char drawer[256];
        const char *file = (const char *)FilePart((STRPTR)argv[p]);
        BPTR lock;
        struct FileLock *own;
        int confirmed = 0, name_only = 0, home_only = 0;
        snprintf(drawer, sizeof drawer, "%.*s", (int)(PathPart((STRPTR)argv[p]) - (STRPTR)argv[p]), argv[p]);
        lock = Lock((STRPTR)drawer, SHARED_LOCK);
        own = lock ? BADDR(lock) : NULL;
        for (int i = 0; i < count; i++) {
            int same_home = own && seen[i].has_home && seen[i].volume == own->fl_Volume && seen[i].key == own->fl_Key;
            int same_name = !strcasecmp(program_of(&seen[i]), file);
            if (same_home && same_name) confirmed++;
            else if (same_name) name_only++;
            else if (same_home && !seen[i].cli) home_only++;
        }
        fprintf(log, "%s: %s (confirmed %d, name-only %d, home-only %d; drawer lock key=%lx)\n", argv[p],
                confirmed ? "confirmed" : name_only ? "name-only" : home_only ? "home-only" : "none", confirmed,
                name_only, home_only, own ? (unsigned long)own->fl_Key : 0ul);
        if (lock) UnLock(lock);
    }
    DateStamp(&a);
    for (int k = 0; k < 1000; k++) scan();
    DateStamp(&b);
    fprintf(log, "1000 scans: %ld ms\n", ((b.ds_Minute - a.ds_Minute) * 60L * 50 + (b.ds_Tick - a.ds_Tick)) * 20L);
    fclose(log);
    return 0;
}
