/* Test helper: moves the system time by SECONDS (negative: back), as Prefs/Time,
 * a time server or SetClock would, and prints the time before and after.
 *   timestep -3600 */
#include <stdio.h>
#include <stdlib.h>
#include <exec/exec.h>
#include <devices/timer.h>
#include <proto/exec.h>

int main(int argc, char **argv)
{
    struct MsgPort *port;
    struct timerequest *tr;
    long step;
    if (argc != 2) { printf("usage: timestep SECONDS\n"); return 10; }
    step = strtol(argv[1], NULL, 10);
    if (!(port = CreateMsgPort())) return 20;
    if (!(tr = (struct timerequest *)CreateIORequest(port, sizeof *tr))) { DeleteMsgPort(port); return 20; }
    if (OpenDevice((CONST_STRPTR)"timer.device", UNIT_MICROHZ, (struct IORequest *)tr, 0)) {
        DeleteIORequest((struct IORequest *)tr); DeleteMsgPort(port); return 20;
    }
    tr->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)tr);
    printf("before %lu\n", (unsigned long)tr->tr_time.tv_secs);
    tr->tr_time.tv_secs += step;
    tr->tr_node.io_Command = TR_SETSYSTIME;
    DoIO((struct IORequest *)tr);
    tr->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)tr);
    printf("after %lu\n", (unsigned long)tr->tr_time.tv_secs);
    fflush(stdout);
    CloseDevice((struct IORequest *)tr);
    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
    return 0;
}
