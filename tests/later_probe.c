/* Development probe, not shipped: runs one command after a delay, with no
 * window of its own, so that a screen change can be timed into the middle
 * of a drag or an open editor. Started from Wanderer (WBRun); ENV:later
 * holds "SECONDS COMMAND...", for example
 *   10 SYS:Prefs/ScreenMode FROM RAM:sm1280.prefs USE */
#include <exec/types.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    static char line[300];
    if (GetVar((CONST_STRPTR)"later", (STRPTR)line, sizeof line, 0) <= 0) return 20;
    char *command = strchr(line, ' ');
    if (!command) return 20;
    *command++ = 0;
    Delay(atoi(line) * 50);
    BPTR nil = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    LONG rc = SystemTags((CONST_STRPTR)command, SYS_Input, (IPTR)nil, SYS_Output, (IPTR)NULL, TAG_END);
    if (rc == -1) Close(nil);
    return rc == -1 ? 20 : 0;
}
