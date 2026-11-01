#ifndef DOCK_PRESENT_H
#define DOCK_PRESENT_H

/* How a dock window's pixels reach the screen. Two ways, chosen per dock
 * when it opens:
 *
 * - composed: the window has a surface with per pixel alpha of its own and
 *   the system's compositor lays it over the desktop (mainline AROS from
 *   2026-10-24). A see-through bar is see-through by itself: nothing of the
 *   desktop is read, and the dock never has to hide for it.
 * - pictured: an ordinary window; a see-through bar shows a picture of the
 *   desktop taken while the dock was out of the way (snapshot.h). ABIv11,
 *   older mainline and screens that cannot compose.
 *
 * present_compose.c is built where the SDK has the compositor's window tags,
 * present_compose_none.c elsewhere. The pixels are ARGB with straight
 * (not premultiplied) alpha either way; see alpha.h. */

struct Screen;
struct Window;
struct TagItem;

/* The screen composes windows with per pixel alpha: a 32 bit screen whose
 * display can compose (SA_SourceAlphaSupport). 0 where this build has no
 * compositor support. */
int dock_present_can_compose(struct Screen *screen);

/* The screen's compositor can blur what lies behind a composed window
 * (frosted glass without a desktop picture): SA_BackdropBlurSupport.
 * PROTOTYPE: mainline has no such attribute yet; a system without it
 * answers no. 0 where this build has no compositor support. Even where
 * the answer is yes, a window can show plain transparency for a moment
 * when the system is short of memory. */
int dock_present_can_blur(struct Screen *screen);

/* OpenWindowTagList(NULL, tags), asking for an alpha surface when compose
 * is set, and for the area behind it blurred by 'blur' pixels (0: none)
 * when the compositor can. The system may still give an ordinary window
 * (no memory for the surface, budget of the screen used up);
 * dock_present_composed() tells. A blur it cannot do leaves plain
 * transparency. */
struct Window *dock_present_open(int compose, int blur, struct TagItem *tags);

/* The window has its own alpha surface now. Asked after opening and after
 * every size change: a surface that cannot grow is turned into an ordinary
 * window without notice. */
int dock_present_composed(struct Window *window);

/* A point of the screen (screen coordinates) takes input in a composed
 * window: the system's own hit test, which is what decides where clicks and
 * drops go (alpha not 0, within the window's input shape). Only for windows
 * dock_present_composed() said yes to. */
int dock_present_hit(struct Window *window, int x, int y);

/* What this build can do, for the diagnostic copy ("composed windows" or
 * "desktop picture only"). */
const char *dock_present_build_name(void);

#endif
