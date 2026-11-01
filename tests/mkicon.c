/* Development probe, not shipped: gives a program a tool icon.
 *   mkicon PROGRAM STACK [CLI]
 * writes PROGRAM.info from the default tool icon with that stack size and,
 * with CLI, the tool type CLI (Workbench then starts it as a Shell command). */
#include <workbench/workbench.h>
#include <proto/icon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    static STRPTR cli[] = { (STRPTR)"CLI", NULL }, none[] = { NULL };
    struct DiskObject *icon;
    if (argc < 3) { printf("usage: mkicon PROGRAM STACK [CLI]\n"); return 10; }
    if (!(icon = GetDefDiskObject(WBTOOL))) { printf("no default tool icon\n"); return 10; }
    icon->do_StackSize = atol(argv[2]);
    icon->do_ToolTypes = argc > 3 && !strcmp(argv[3], "CLI") ? cli : none;
    icon->do_CurrentX = icon->do_CurrentY = (LONG)NO_ICON_POSITION;
    int ok = PutDiskObject((CONST_STRPTR)argv[1], icon);
    printf("%s %s.info stack=%s%s\n", ok ? "wrote" : "FAILED", argv[1], argv[2], argc > 3 ? " CLI" : "");
    icon->do_ToolTypes = NULL;
    FreeDiskObject(icon);
    return ok ? 0 : 10;
}
