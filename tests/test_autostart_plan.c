/* Rules for Start with Wanderer (autostart_plan.h). */
#include "autostart_plan.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct AsScan scan_of(int complete, int n, const char *const *names, const int *kinds)
{
    struct AsScan s;
    memset(&s, 0, sizeof s);
    s.complete = complete;
    for (int i = 0; i < n; i++) {
        snprintf(s.item[i].base, sizeof s.item[i].base, "%s", names[i]);
        s.item[i].kind = kinds[i];
    }
    s.count = n;
    return s;
}

int main(void)
{
    struct AsPlan p;
    int failed[AS_MAX_ITEMS];

    /* Another installation's start icon: On leaves it alone and changes
       nothing; Off does not remove it; only Replace does. */
    {
        const char *names[] = {"Dockbar Test", "Dockbar"};
        int kinds[] = {AS_OURS, AS_OTHER};
        struct AsScan s = scan_of(1, 2, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p));
        CHECK(p.verdict == AS_REFUSE_NEEDS_REPLACE && p.culprit == 1 && !p.write);
        CHECK(p.remove[0] == 0 && p.remove[1] == 0);
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && !p.write);
        CHECK(p.remove[0] == AS_RM_ICON && p.remove[1] == 0);
        CHECK(as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.write);
        CHECK(p.remove[0] == AS_RM_ICON); /* ours under another name goes */
        CHECK(p.remove[1] == 0);          /* written over, not deleted */
        CHECK(p.types_from == 0);         /* ours first */
    }
    /* Off with only another installation's icon: nothing to remove. */
    {
        const char *names[] = {"Dockbar"};
        int kinds[] = {AS_OTHER};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.remove[0] == 0);
    }
    /* A copy: On refuses, Replace removes program and icon. */
    {
        const char *names[] = {"Dockbar", "Dockbar 0.5"};
        int kinds[] = {AS_COPY, AS_COPY};
        struct AsScan s = scan_of(1, 2, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_NEEDS_REPLACE);
        CHECK(as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.write);
        CHECK(p.remove[0] == AS_RM_FILE);
        CHECK(p.remove[1] == (AS_RM_FILE | AS_RM_ICON));
        CHECK(p.types_from == -1); /* a copy's tool types are not a start icon's */
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.remove[0] == 0 && p.remove[1] == 0);
    }
    /* The old program without an icon still needs Replace. */
    {
        const char *names[] = {"Dockbar"};
        int kinds[] = {AS_COPY_FILE};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p));
        CHECK(as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.remove[0] == AS_RM_FILE);
    }
    /* A start icon whose program is gone counts as another's. */
    {
        const char *names[] = {"Dockbar"};
        int kinds[] = {AS_MISSING};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_NEEDS_REPLACE);
        CHECK(as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.types_from == 0);
    }
    /* An icon that could not be read is never free space. */
    {
        const char *names[] = {"Dockbar", "Dockbar Old"};
        int kinds[] = {AS_UNREADABLE, AS_OURS};
        struct AsScan s = scan_of(1, 2, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_UNREADABLE && p.culprit == 0);
        CHECK(!p.write);
        CHECK(!as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.verdict == AS_REFUSE_UNREADABLE);
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.remove[1] == AS_RM_ICON && p.remove[0] == 0);
    }
    {
        const char *names[] = {"IntroMusic"};
        int kinds[] = {AS_UNREADABLE};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_UNREADABLE);
    }
    /* Something else of the program's name. */
    {
        const char *names[] = {"dockbar"};
        int kinds[] = {AS_FOREIGN};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(!as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.verdict == AS_REFUSE_BLOCKED && !p.write);
    }
    /* A drawer read only in part, or with more entries than fit. */
    {
        const char *names[] = {"Dockbar"};
        int kinds[] = {AS_OURS};
        struct AsScan s = scan_of(0, 1, names, kinds);
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_INCOMPLETE && !p.write);
        CHECK(!as_plan_make(&s, AS_REPLACE, "Dockbar", &p) && p.verdict == AS_REFUSE_INCOMPLETE);
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.remove[0] == AS_RM_ICON); /* recognised: still removed */
    }
    /* Off removes this Dockbar's own icon of the program's name too. */
    {
        const char *names[] = {"Dockbar"};
        int kinds[] = {AS_OURS};
        struct AsScan s = scan_of(1, 1, names, kinds);
        CHECK(as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.remove[0] == AS_RM_ICON);
        CHECK(as_plan_make(&s, AS_ON, "Dockbar", &p) && p.write && p.remove[0] == 0 && p.types_from == 0);
    }
    /* The copy of Dockbar is itself the program. */
    {
        struct AsScan s = scan_of(1, 0, NULL, NULL);
        s.self_copy = 1;
        CHECK(!as_plan_make(&s, AS_ON, "Dockbar", &p) && p.verdict == AS_REFUSE_SELF_COPY);
        CHECK(!as_plan_make(&s, AS_OFF, "Dockbar", &p) && p.verdict == AS_REFUSE_SELF_COPY);
    }
    /* Nothing there: On writes, Off has nothing to do. */
    {
        struct AsScan s = scan_of(1, 0, NULL, NULL);
        CHECK(as_plan_make(&s, AS_ON, "Dockbar", &p) && p.write && p.types_from == -1);
    }
    /* What still starts after a removal failed. */
    {
        const char *names[] = {"Dockbar", "Dockbar 0.5", "Dockbar Two", "Dockbar"};
        int kinds[] = {AS_COPY, AS_COPY, AS_OTHER, AS_COPY_FILE};
        struct AsScan s = scan_of(1, 4, names, kinds);
        CHECK(as_plan_make(&s, AS_REPLACE, "Dockbar", &p));
        memset(failed, 0, sizeof failed);
        failed[0] = AS_RM_FILE;              /* program left, icon written over */
        failed[1] = AS_RM_ICON;              /* its own icon still there */
        failed[2] = AS_RM_ICON;
        failed[3] = AS_RM_FILE;
        CHECK(!as_still_starts(&s, &p, failed, 0, "Dockbar"));
        CHECK(as_still_starts(&s, &p, failed, 1, "Dockbar"));
        CHECK(as_still_starts(&s, &p, failed, 2, "Dockbar"));
        CHECK(!as_still_starts(&s, &p, failed, 3, "Dockbar"));
        failed[1] = AS_RM_FILE;              /* program left, icon gone */
        CHECK(!as_still_starts(&s, &p, failed, 1, "Dockbar"));
    }
    if (failures)
        return 1;
    printf("autostart plan: all checks passed\n");
    return 0;
}
