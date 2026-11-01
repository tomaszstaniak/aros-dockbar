/* Development probe, not shipped: stands in for a widget provider that is
 * slow, to test how Dockbar ends a request it stopped waiting for. Put it
 * in the widget drawer under the provider's name, the real provider next to
 * it as <name>.real, and one line in PROGDIR:slowprov.mode:
 *   late N [M] ignore Ctrl-C, wait N s, then run the real provider (a late
 *              answer) and stay M s more before leaving
 *   break N  wait up to N s, leave at once on Ctrl-C without an answer
 * Each event is appended to PROGDIR:slowprov.log. */
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void say(const char *what, const char *request)
{
    FILE *f = fopen("PROGDIR:slowprov.log", "a");
    if (!f) return;
    struct DateStamp d;
    DateStamp(&d);
    fprintf(f, "%ld.%02ld %s %s\n", d.ds_Minute * 60L + d.ds_Tick / 50, (long)((d.ds_Tick % 50) * 2), what, request);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 5;
    char mode[16] = "late";
    int seconds = 70, linger = 0;
    FILE *m = fopen("PROGDIR:slowprov.mode", "r");
    if (m) { if (fscanf(m, "%15s %d %d", mode, &seconds, &linger) < 1) strcpy(mode, "late"); fclose(m); }
    say(mode, argv[1]);
    for (int i = 0; i < seconds * 10; i++) {
        Delay(5);
        if (!strcmp(mode, "break") && (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)) {
            say("break received, leaving without an answer", argv[1]);
            return 10;
        }
    }
    /* PROGDIR: of the Shell that runs the command is not this drawer. */
    char dir[600], line[1400];
    if (!NameFromLock(GetProgramDir(), (STRPTR)dir, sizeof dir)) return 20;
    size_t n = strlen(dir);
    if (n && dir[n - 1] != ':' && dir[n - 1] != '/' && n + 1 < sizeof dir) { dir[n] = '/'; dir[n + 1] = 0; }
    snprintf(line, sizeof line, "\"%s%s.real\" \"%s\"", dir, FilePart((STRPTR)argv[0]), argv[1]);
    say(line, "");
    say("calling the real provider", argv[1]);
    LONG rc = SystemTags((CONST_STRPTR)line, TAG_END);
    say(rc == 0 ? "answered" : "real provider failed", argv[1]);
    if (linger > 0) {
        Delay(linger * 50);
        say("leaving after the answer", argv[1]);
    }
    return (int)rc;
}
