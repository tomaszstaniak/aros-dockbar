/* Native finite probe. Run with an explicit output file argument.
 * Tests owned path copies and reply/cleanup sequencing using real Exec ports
 * and DOS locks. It does not simulate Wanderer's GUI hit testing. */
#include "native_icons.h"
#include <stdio.h>
#include <string.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>

struct Probe {
    struct MsgPort *replies;
    struct AppMessage *message;
    DockDropTarget *target;
    char *sender_name;
    char expected[3][4096];
    int count;
    int failures;
};

static void accept_path(const char *path, void *context)
{
    struct Probe *probe = context;
    if (probe->count == 0) {
        if (GetMsg(probe->replies) != &probe->message->am_Message)
            ++probe->failures;
        /* The sender is free to reuse every argument once it has its reply. */
        strcpy(probe->sender_name, "overwritten after reply");
        probe->message->am_ArgList = NULL;
        probe->message->am_MouseX = 999;
        probe->message->am_MouseY = 999;
    }
    if (probe->target->mouse_x != 37 || probe->target->mouse_y != -12)
        ++probe->failures;
    /* Each next callback must receive the message snapshot again. */
    probe->target->mouse_x = 123;
    probe->target->mouse_y = 456;
    if (probe->count >= 3 || strcmp(path, probe->expected[probe->count]))
        ++probe->failures;
    ++probe->count;
}

int main(int argc, char **argv)
{
    DockDropTarget target = { 0 };
    static struct Probe probe;
    struct AppMessage message = { 0 };
    struct WBArg arguments[5] = { { 0 } };
    char sender_name[100] = "SYS:Tools/Other App.info";
    static char too_long[4200];
    BPTR lock = 0, system_lock = 0;
    FILE *output;
    int failures = 0;
    int result = 1;

    if (argc != 2)
        return 20;
    output = fopen(argv[1], "w");
    if (!output)
        return 20;
    fputs("Native drop probe: x86_64 ABIv11 2026.09 SDK; synthetic AppMessages\n", output);
    target.port = CreateMsgPort();
    probe.replies = CreateMsgPort();
    lock = Lock((CONST_STRPTR)"RAM:", ACCESS_READ);
    if (!target.port || !probe.replies || !lock) {
        fputs("FAIL: setup\n", output);
        goto cleanup;
    }
    if (!NameFromLock(lock, (STRPTR)probe.expected[0], sizeof(probe.expected[0])))
        goto cleanup;
    strcpy(probe.expected[2], probe.expected[0]);
    if (!AddPart((STRPTR)probe.expected[0], (CONST_STRPTR)"My App", sizeof(probe.expected[0])))
        goto cleanup;
    strcpy(probe.expected[1], "SYS:Tools/Other App");
    memset(too_long, 'x', sizeof(too_long) - 1);
    too_long[3] = ':';
    too_long[sizeof(too_long) - 1] = '\0';
    arguments[0].wa_Lock = lock;
    arguments[0].wa_Name = (BYTE *)"My App.INFO";
    arguments[1].wa_Name = (BYTE *)sender_name;
    arguments[2].wa_Lock = lock;
    arguments[2].wa_Name = (BYTE *)"";
    arguments[3].wa_Name = (BYTE *)"unresolved relative name";
    arguments[4].wa_Name = (BYTE *)too_long;
    message.am_Message.mn_ReplyPort = probe.replies;
    message.am_Message.mn_Length = sizeof(message);
    message.am_Type = AMTYPE_APPWINDOW;
    message.am_Version = AM_VERSION;
    message.am_NumArgs = 5;
    message.am_MouseX = 37;
    message.am_MouseY = -12;
    message.am_ArgList = arguments;
    probe.message = &message;
    probe.target = &target;
    probe.sender_name = sender_name;
    PutMsg(target.port, &message.am_Message);
    if (dock_drop_poll(&target, accept_path, &probe) != 3 || probe.count != 3)
        ++failures;
    failures += probe.failures;
    if (GetMsg(probe.replies) != NULL || dock_drop_poll(&target, accept_path, &probe) != 0)
        ++failures;

    message.am_Type = AMTYPE_APPMENUITEM;
    PutMsg(target.port, &message.am_Message);
    if (dock_drop_poll(&target, accept_path, &probe) != 0 ||
        GetMsg(probe.replies) != &message.am_Message)
        ++failures;

    /* Closing must reply queued drops without delivering new model changes. */
    message.am_Type = AMTYPE_APPWINDOW;
    message.am_ArgList = arguments;
    PutMsg(target.port, &message.am_Message);
    dock_drop_close(&target);
    if (target.port || target.window || GetMsg(probe.replies) != &message.am_Message || probe.count != 3)
        ++failures;
    dock_drop_close(&target);
    system_lock = Lock((CONST_STRPTR)"SYS:", ACCESS_READ);
    if (!system_lock ||
        !NameFromLock(system_lock, (STRPTR)probe.expected[0], sizeof(probe.expected[0])) ||
        !dock_same_object("SYS:", probe.expected[0]) ||
        dock_same_object("SYS:", "RAM:") ||
        !dock_same_object("SYS:Missing Object.INFO", "sys:missing object") ||
        dock_same_object(NULL, "SYS:") ||
        dock_same_object("SYS:", too_long))
        ++failures;
    fprintf(output, "%s: paths with spaces, multiple args, .INFO normalization, volume lock, invalid paths, reply before callback, copied sender data and coordinates, queued close, repeated close, DOS object identity (%d failures)\n",
            failures ? "FAIL" : "PASS", failures);
    result = failures ? 1 : 0;
cleanup:
    dock_drop_close(&target);
    if (lock)
        UnLock(lock);
    if (system_lock)
        UnLock(system_lock);
    if (probe.replies) {
        while (GetMsg(probe.replies)) { }
        DeleteMsgPort(probe.replies);
    }
    if (fclose(output) != 0)
        result = 1;
    return result;
}
