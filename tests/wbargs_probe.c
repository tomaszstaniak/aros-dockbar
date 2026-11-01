/* Development probe, not shipped: writes every argument it was started
 * with, so that a test sees the whole list a dock passed, not just what a
 * viewer chose to show. Appends to RESULTS:dockbar-wbargs.log by default
 * (ENV:wbargs_log overrides).
 * From Workbench: one line per WBArg, the full path from its lock + name.
 * From a Shell: one line per argv entry, exactly as parsed.
 * Then how it was started: the current directory, PROGDIR:, the stack it
 * got, and the first line of the relative file "rel.txt" (so a test sees
 * which directory a relative name resolves in). */
#include <dos/dos.h>
#include <workbench/startup.h>
#include <exec/tasks.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char log[256] = "RESULTS:dockbar-wbargs.log", path[1024];
    FILE *out;
    GetVar((CONST_STRPTR)"wbargs_log", (STRPTR)log, sizeof log, 0);
    if (!(out = fopen(log, "a"))) return 20;
    if (argc == 0) {
        struct WBStartup *wb = (struct WBStartup *)argv;
        fprintf(out, "WB args=%ld\n", (long)wb->sm_NumArgs);
        for (LONG i = 0; i < wb->sm_NumArgs; i++) {
            struct WBArg *a = &wb->sm_ArgList[i];
            path[0] = 0;
            if (a->wa_Lock) NameFromLock(a->wa_Lock, (STRPTR)path, sizeof path);
            if (a->wa_Name && *a->wa_Name) AddPart((STRPTR)path, (CONST_STRPTR)a->wa_Name, sizeof path);
            fprintf(out, "WB %ld [%s] name=[%s]\n", (long)i, path, a->wa_Name ? (char *)a->wa_Name : "");
        }
    } else {
        /* The line exactly as the program got it, before C startup split it:
         * this is what the Shell made of a script's <ARG> substitution. */
        const char *raw = (const char *)GetArgStr();
        size_t rawlen = raw ? strlen(raw) : 0;
        while (rawlen && (raw[rawlen - 1] == '\n' || raw[rawlen - 1] == ' ')) rawlen--;
        fprintf(out, "RAW [%.*s]\n", (int)rawlen, raw ? raw : "");
        fprintf(out, "CLI argc=%d\n", argc);
        for (int i = 0; i < argc; i++) fprintf(out, "CLI %d [%s]\n", i, argv[i]);
    }
    {
        struct Task *me = FindTask(NULL);
        BPTR cd = CurrentDir(BNULL), home = ((struct Process *)me)->pr_HomeDir;
        char line[128] = "";
        FILE *rel;
        CurrentDir(cd);
        path[0] = 0;
        if (cd) NameFromLock(cd, (STRPTR)path, sizeof path);
        fprintf(out, "CWD [%s]\n", cd ? path : "(none: boot volume)");
        path[0] = 0;
        if (home) NameFromLock(home, (STRPTR)path, sizeof path);
        fprintf(out, "PROGDIR [%s]\n", home ? path : "(none)");
        fprintf(out, "STACK %ld\n", (long)((char *)me->tc_SPUpper - (char *)me->tc_SPLower));
        if ((rel = fopen("rel.txt", "r"))) {
            if (fgets(line, sizeof line, rel)) line[strcspn(line, "\r\n")] = 0;
            fclose(rel);
            fprintf(out, "REL [%s]\n", line);
        } else fprintf(out, "REL (not found)\n");
    }
    fprintf(out, "END\n");
    fclose(out);
    return 0;
}
