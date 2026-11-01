#include "frame.h"
#include <assert.h>
#include <stdio.h>

static const struct DockFrameStyle style = {0x101010, 0x101010, 0xf0f0f0, 0xf0f0f0};

static void square_corners_keep_every_pixel(void)
{
    for (int y = 0; y < 20; y++) assert(dock_row_inset(y, 60, 20, 0) == 0);
    assert(dock_frame_pixel(&style, 0, 0, 60, 20, 0) == 0xfff0f0f0UL);
    assert(dock_frame_pixel(&style, 1, 1, 60, 20, 0) == 0xff101010UL);
}
static void small_radius_trims_only_corner_rows(void)
{
    const int r = DOCK_CORNER_DEFAULT;
    assert(dock_row_inset(0, 60, 20, r) > 0);
    assert(dock_row_inset(0, 60, 20, r) == dock_row_inset(19, 60, 20, r));
    for (int y = r; y < 20 - r; y++) assert(dock_row_inset(y, 60, 20, r) == 0);
    for (int y = 1; y < r; y++)
        assert(dock_row_inset(y, 60, 20, r) <= dock_row_inset(y - 1, 60, 20, r));
}
static void border_sits_on_first_kept_pixel(void)
{
    /* A border beyond the region would be cut; one inside it would leave a
       fill-coloured fringe outside the line. */
    for (int r = 0; r <= DOCK_CORNER_MAX; r++)
        for (int y = 0; y < 20; y++) {
            int edge = dock_row_inset(y, 60, 20, r);
            unsigned long c = dock_frame_pixel(&style, edge, y, 60, 20, r);
            assert(((c >> 8) & 0xff) >= 0x80);
        }
}
static void corner_transition_is_graded(void)
{
    /* Some corner pixel must lie strictly between border and fill. */
    int graded = 0;
    for (int y = 0; y < DOCK_CORNER_MAX; y++)
        for (int x = 0; x < DOCK_CORNER_MAX; x++) {
            unsigned g = (dock_frame_pixel(&style, x, y, 60, 20, DOCK_CORNER_MAX) >> 8) & 0xff;
            graded |= g > 0x10 && g < 0xf0;
        }
    assert(graded);
}
static void coverage_is_bounded_and_symmetric(void)
{
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 30; x++) {
            float a = dock_round_coverage(x, y, 60, 20, 6);
            assert(a >= 0 && a <= 1);
            assert(a == dock_round_coverage(59 - x, y, 60, 20, 6));
        }
    assert(dock_round_coverage(0, 0, 60, 20, 6) < 0.5f);
    assert(dock_round_coverage(30, 10, 60, 20, 6) == 1);
}
int main(void)
{
    square_corners_keep_every_pixel();
    small_radius_trims_only_corner_rows();
    border_sits_on_first_kept_pixel();
    corner_transition_is_graded();
    coverage_is_bounded_and_symmetric();
    puts("PASS frame: square and rounded insets, border placement, graded corners, coverage");
    return 0;
}
