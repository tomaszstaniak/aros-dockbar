/* display_bitmap_probe: can a display sized, displayable bitmap of the
 * Workbench screen's mode be allocated, the way the compositor allocates its
 * composition buffer (gfx_compositorclass.c HIDDCompositorAllocBitMap)?
 *
 *   display_bitmap_probe LOG/A
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <utility/tagitem.h>
#include <stdio.h>

static void try(FILE *f, const char *what, struct Screen *s, ULONG flags)
{
    struct TagItem tags[] = {{BMATags_DisplayID, GetVPModeID(&s->ViewPort)}, {TAG_DONE, 0}};
    struct BitMap *bm = AllocBitMap(s->Width, s->Height, GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH),
                                    flags | BMF_CHECKVALUE, (struct BitMap *)tags);
    fprintf(f, "%s: %s", what, bm ? "allocated" : "FAILED");
    if (bm)
    {
        fprintf(f, " pixfmt=%ld bppix=%ld", (long)GetCyberMapAttr(bm, CYBRMATTR_PIXFMT),
                (long)GetCyberMapAttr(bm, CYBRMATTR_BPPIX));
        FreeBitMap(bm);
    }
    fprintf(f, "\n");
}

int main(void)
{
    IPTR args[1] = {0};
    struct RDArgs *rd = ReadArgs("LOG/A", args, NULL);
    struct Screen *s;
    FILE *f;

    if (!rd) return 20;
    f = fopen((char *)args[0], "w");
    if (!f) { FreeArgs(rd); return 20; }
    s = LockPubScreen(NULL);
    if (s)
    {
        fprintf(f, "screen %dx%d modeid=%08lx avail chip/fast=%lu\n", s->Width, s->Height,
                (unsigned long)GetVPModeID(&s->ViewPort), (unsigned long)AvailMem(MEMF_ANY));
        try(f, "displayable", s, BMF_DISPLAYABLE);
        try(f, "plain", s, 0);
        try(f, "displayable again", s, BMF_DISPLAYABLE);
        UnlockPubScreen(NULL, s);
    }
    fclose(f);
    FreeArgs(rd);
    return 0;
}
