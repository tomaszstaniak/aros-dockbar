/* Development probe, not shipped: drops files on the window at a screen
 * point, the way Wanderer does (workbench.library SendAppWindowMessage),
 * with names a Wanderer drag cannot produce: a .info file, an assign path.
 *   drop_send X Y FILE...      (Workbench screen coordinates)
 *   drop_send X Y @LIST        (names from LIST, one per line: names that
 *                               a Shell line cannot carry, e.g. a " before a ;)
 * Appends the result to RESULTS:dockbar-dropsend.log. */
#include <intuition/intuitionbase.h>
#include <graphics/layers.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/workbench.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *out = fopen("RESULTS:dockbar-dropsend.log", "a");
    if (argc < 4 || !out) return 20;
    int x = atoi(argv[1]), y = atoi(argv[2]);
    static char names[64][512];
    static char *list[64];
    char **files = argv + 3;
    int nfiles = argc - 3;
    if (argv[3][0] == '@') {
        FILE *in = fopen(argv[3] + 1, "r");
        int n = 0;
        if (!in) return 20;
        while (n < 64 && fgets(names[n], sizeof names[n], in)) {
            names[n][strcspn(names[n], "\r\n")] = 0;
            if (names[n][0]) { list[n] = names[n]; n++; }
        }
        fclose(in);
        files = list;
        nfiles = n;
    }
    struct Screen *s = LockPubScreen(NULL);
    struct Window *win = NULL;
    if (s) {
        struct Layer *l = WhichLayer(&s->LayerInfo, x, y);
        if (l) win = l->Window;
    }
    BOOL ok = FALSE;
    if (win) ok = SendAppWindowMessage(win, nfiles, files, 0, x - win->LeftEdge, y - win->TopEdge, 0, 0);
    fprintf(out, "send at %d,%d window=%p result=%d files=%d first=%s\n", x, y, (void *)win, ok, nfiles, nfiles ? files[0] : "");
    fclose(out);
    if (s) UnlockPubScreen(NULL, s);
    return ok ? 0 : 5;
}
