/* Development probe, not shipped: makes the desktop picture arrive late.
 *
 *   slowpaint_probe reset SECONDS LOG [LIFETIME]
 *     Subscribed ahead of Dockbar (higher priority). When the Workbench
 *     screen reopens, a plain grey window covers the desktop before Dockbar
 *     hears of it, and goes away SECONDS later: what a slow graphics card or
 *     a slow wallpaper load looks like to a dock.
 *   slowpaint_probe now SECONDS LOG
 *     Covers the desktop now, behind every other window, for SECONDS.
 *
 * Started from Wanderer (WBRun), the arguments come from ENV:slowpaint, so
 * that no Shell window stays open to block the screen change under test.
 *
 * The cover is smart refresh, so revealing parts of it costs no repaint
 * from this task. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *CyberGfxBase;
static FILE *logfile;
static struct Window *cover;

static void say(const char *what)
{
    struct DateStamp d;
    DateStamp(&d);
    fprintf(logfile, "%ld.%02ld %s\n", (long)(d.ds_Minute * 60 + d.ds_Tick / 50), (long)(d.ds_Tick % 50 * 2), what);
    fflush(logfile);
}

static void open_cover(int to_back)
{
    struct Screen *s = LockPubScreen(NULL);
    if (!s) { say("cover: no screen"); return; }
    int top = s->BarHeight + 1;
    cover = OpenWindowTags(NULL, WA_PubScreen, (IPTR)s, WA_Left, 0, WA_Top, top,
                           WA_Width, s->Width, WA_Height, s->Height - top,
                           WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE,
                           WA_NoCareRefresh, TRUE, TAG_DONE);
    UnlockPubScreen(NULL, s);
    if (!cover) { say("cover: open failed"); return; }
    if (to_back) WindowToBack(cover);
    FillPixelArray(cover->RPort, 0, 0, cover->Width, cover->Height, 0x9c9c9c);
    say("cover: shown");
}

static void close_cover(void)
{
    if (!cover) return;
    CloseWindow(cover);
    cover = NULL;
    say("cover: removed");
}

int main(int argc, char **argv)
{
    static char line[200], *words[6];
    if (argc == 0) {
        argc = 1;
        if (GetVar((CONST_STRPTR)"slowpaint", (STRPTR)line, sizeof line, 0) > 0)
            for (char *p = strtok(line, " \n"); p && argc < 6; p = strtok(NULL, " \n")) words[argc++] = p;
        argv = words;
    }
    if (argc < 4) {
        puts("slowpaint_probe reset|now SECONDS LOG [LIFETIME]");
        return 20;
    }
    int seconds = atoi(argv[2]), lifetime = argc > 4 ? atoi(argv[4]) : 600;
    if (!(logfile = fopen(argv[3], "w"))) return 20;
    if (!(CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0))) return 20;
    if (!strcmp(argv[1], "now")) {
        open_cover(1);
        Delay(seconds * 50);
        close_cover();
    } else {
        struct MsgPort *port = CreateMsgPort();
        APTR notify = port ? StartScreenNotifyTags(SNA_Notify, SNOTIFY_WAIT_REPLY | SNOTIFY_BEFORE_CLOSEWB | SNOTIFY_AFTER_OPENWB,
                                                   SNA_MsgPort, (IPTR)port, SNA_Priority, 10, TAG_END) : NULL;
        say(notify ? "watching" : "notify failed");
        int ticks = 0, shown_at = -1;
        while (notify && ticks < lifetime * 50 && !(SetSignal(0, 0) & SIGBREAKF_CTRL_C)) {
            struct ScreenNotifyMessage *msg;
            while ((msg = (struct ScreenNotifyMessage *)GetMsg(port))) {
                if (msg->snm_Class == SNOTIFY_BEFORE_CLOSEWB) {
                    close_cover();
                    say("before close");
                } else if (msg->snm_Class == SNOTIFY_AFTER_OPENWB) {
                    say("after open");
                    open_cover(0);
                    shown_at = ticks;
                }
                ReplyMsg((struct Message *)msg);
            }
            if (cover && shown_at >= 0 && ticks - shown_at >= seconds * 50) close_cover();
            Delay(5);
            ticks += 5;
        }
        close_cover();
        if (notify) {
            while (!EndScreenNotify(notify)) {
                struct Message *m;
                while ((m = GetMsg(port))) ReplyMsg(m);
                Delay(2);
            }
        }
        if (port) {
            struct Message *m;
            while ((m = GetMsg(port))) ReplyMsg(m);
            DeleteMsgPort(port);
        }
    }
    say("end");
    fclose(logfile);
    CloseLibrary(CyberGfxBase);
    return 0;
}
