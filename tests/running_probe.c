/* Development probe, not shipped: which programs are running, found by the
 * same code as Dockbar's running indicators (src/running_aros.c).
 *   running_probe LOG PATH...
 * One scan of the system's processes; for each PATH (a program file) the
 * fields of a lock on its drawer, whether its file system's keys can tell
 * drawers apart, and the verdict:
 *   confirmed  a process with the file name and the same home drawer
 *              (what shows a dot)
 *   name-only  a process with the file name, home drawer elsewhere or not
 *              comparable
 *   none
 * LOG gets the process table too, and the cost of 1000 scans. */
#include "running.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MAXP 160
static struct DockRunProcess seen[MAXP];

int main(int argc, char **argv)
{
    FILE *log;
    struct DateStamp a, b;
    int count;
    if (argc < 3 || !(log = fopen(argv[1], "w"))) return 20;
    count = dock_running_scan(seen, MAXP);
    fprintf(log, "processes %d\n", count);
    for (int i = 0; i < count; i++)
        fprintf(log, "  [%s] home=%s vol=%lx key=%lx port=%lx\n", seen[i].name, seen[i].has_home ? "yes" : "no",
                seen[i].volume, seen[i].key, seen[i].port);
    for (int p = 2; p < argc; p++) {
        struct DockRunTarget t;
        int name_only = 0;
        dock_running_target(&t, argv[p]);
        for (int i = 0; i < count; i++)
            if (seen[i].name[0] && !strcasecmp(seen[i].name, t.file)) name_only++;
        int confirmed = dock_running_match(&t, seen, count);
        fprintf(log, "%s: %s (file %s; drawer vol=%lx key=%lx port=%lx usable=%d; processes with the name %d)\n", argv[p],
                confirmed ? "confirmed" : name_only ? "name-only" : "none", t.file, t.volume, t.key, t.port, t.usable,
                name_only);
    }
    DateStamp(&a);
    for (int k = 0; k < 1000; k++) dock_running_scan(seen, MAXP);
    DateStamp(&b);
    fprintf(log, "1000 scans: %ld ms\n", ((b.ds_Minute - a.ds_Minute) * 60L * 50 + (b.ds_Tick - a.ds_Tick)) * 20L);
    fclose(log);
    return 0;
}
