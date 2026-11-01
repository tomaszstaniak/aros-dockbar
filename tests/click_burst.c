/* Development probe, not shipped: presses the left button where the pointer
 * is, through input.device, at a time a host cannot hit (inside a resize
 * that a test build stretches to 600 or 1500 ms).
 *   click_burst TICKS N     wait TICKS (1/50 s), then N clicks (press, release)
 *   click_burst TICKS down  wait, then a press only
 *   click_burst TICKS up    wait, then a release only
 * Appends what it sent to RESULTS:dockbar-clicks.log. */
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int send(struct IOStdReq *io, UWORD code, UWORD qualifier)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE | qualifier;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ie;
    io->io_Length = sizeof ie;
    return DoIO((struct IORequest *)io) == 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) return 5;
    struct MsgPort *port = CreateMsgPort();
    struct IOStdReq *io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof *io) : NULL;
    if (!io || OpenDevice((CONST_STRPTR)"input.device", 0, (struct IORequest *)io, 0)) return 20;
    int ticks = atoi(argv[1]), sent = 0;
    if (ticks > 0) Delay(ticks);
    struct DateStamp d;
    DateStamp(&d);
    if (!strcmp(argv[2], "down"))
        sent += send(io, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON);
    else if (!strcmp(argv[2], "up"))
        sent += send(io, IECODE_LBUTTON | IECODE_UP_PREFIX, 0);
    else
        for (int i = 0, n = atoi(argv[2]); i < n; i++) {
            sent += send(io, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON);
            Delay(1);
            sent += send(io, IECODE_LBUTTON | IECODE_UP_PREFIX, 0);
            Delay(1);
        }
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    FILE *out = fopen("RESULTS:dockbar-clicks.log", "a");
    if (out) {
        fprintf(out, "%ld.%02ld %s %s: %d events\n", d.ds_Minute * 60L + d.ds_Tick / 50, (long)((d.ds_Tick % 50) * 2), argv[1], argv[2], sent);
        fclose(out);
    }
    return 0;
}
