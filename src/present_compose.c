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

/* PROTOTYPE (proto-backdrop-blur): neither tag is in mainline yet, and their
 * numbers are not reserved. An attribute given this number later would answer
 * too, so only this contract's exact answer sends the window tag; the first
 * prototype's TRUE does not. The same value as Limpet's backdrop_blur.h. */
#ifndef WA_BackdropBlur
#define WA_BackdropBlur (WA_Dummy + 107)
#endif
#ifndef SA_BackdropBlurSupport
#define SA_BackdropBlurSupport (SA_Dummy + 161)
#endif
#ifndef BACKDROPBLUR_CONTRACT_V1
#define BACKDROPBLUR_CONTRACT_V1 0x424c5201UL
#endif
int dock_present_can_blur(struct Screen *screen)
{
    IPTR blur = 0;
    if (!screen) return 0;
    if (!GetAttr(SA_BackdropBlurSupport, (Object *)screen, &blur)) return 0;
    return blur == BACKDROPBLUR_CONTRACT_V1;
}

struct Window *dock_present_open(int compose, int blur, struct TagItem *tags)
{
    struct TagItem extra[] = {
        {WA_AlphaSurface, TRUE},
        {blur > 0 ? WA_BackdropBlur : TAG_IGNORE, (IPTR)blur},
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
