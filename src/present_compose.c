#include "present.h"
#include <proto/intuition.h>
#include <proto/layers.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <utility/tagitem.h>

/* SA_SourceAlphaSupport is the public question (screenclass.c); the call
 * behind it, CompositeLayersSupported(), is private to layers.library and
 * may move, so it is not called here. */
int dock_present_can_compose(struct Screen *screen)
{
    IPTR alpha = 0;
    if (!screen) return 0;
    if (!GetAttr(SA_SourceAlphaSupport, (Object *)screen, &alpha)) return 0;
    return alpha != 0;
}

struct Window *dock_present_open(int compose, struct TagItem *tags)
{
    struct TagItem extra[] = {
        {WA_AlphaSurface, TRUE},
        {TAG_MORE, (IPTR)tags},
    };
    return OpenWindowTagList(NULL, compose ? extra : tags);
}

/* Development build dev/Dockbar-refuse: every window counts as refused a
 * surface, so that the fallback to the desktop picture can be tested on a
 * screen that composes. */
#ifndef DOCK_TEST_COMPOSE_REFUSE
#define DOCK_TEST_COMPOSE_REFUSE 0
#endif
int dock_present_composed(struct Window *window)
{
    if (DOCK_TEST_COMPOSE_REFUSE) return 0;
    return window && window->WLayer && (window->WLayer->Flags & LAYERCOMPOSITED) != 0;
}

int dock_present_hit(struct Window *window, int x, int y)
{
    struct Layer *l = window ? window->WLayer : NULL;
    BOOL hit;

    if (!l) return 0;
    /* IsLayerHitAt() needs the Layer_Info lock, as WhichLayer() does */
    LockLayerInfo(l->LayerInfo);
    hit = IsLayerHitAt(l, x, y);
    UnlockLayerInfo(l->LayerInfo);
    return hit != 0;
}

const char *dock_present_build_name(void) { return "composed windows"; }
