#include "present.h"
#include <proto/intuition.h>

/* Built where the SDK has no compositor (ABIv11, mainline before
 * 2026-10-24): every dock is pictured. */
int dock_present_can_compose(struct Screen *screen) { (void)screen; return 0; }

int dock_present_can_blur(struct Screen *screen) { (void)screen; return 0; }

struct Window *dock_present_open(int compose, int blur, struct TagItem *tags)
{
    (void)compose;
    (void)blur;
    return OpenWindowTagList(NULL, tags);
}

int dock_present_composed(struct Window *window) { (void)window; return 0; }
int dock_present_hit(struct Window *window, int x, int y) { (void)window; (void)x; (void)y; return 0; }

const char *dock_present_build_name(void) { return "desktop picture only"; }
