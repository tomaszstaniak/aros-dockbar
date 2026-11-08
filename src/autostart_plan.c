/* Rules for Start with Wanderer; see autostart_plan.h. */
#include "autostart_plan.h"

#include <string.h>
#include <strings.h>

static int named(const struct AsItem *item, const char *program)
{
    return !strcasecmp(item->base, program);
}

static int starts_other(int kind)
{
    return kind == AS_COPY || kind == AS_COPY_FILE || kind == AS_OTHER || kind == AS_MISSING;
}

int as_plan_make(const struct AsScan *scan, int action, const char *program, struct AsPlan *plan)
{
    int i;

    memset(plan, 0, sizeof *plan);
    plan->verdict = AS_GO;
    plan->culprit = plan->types_from = -1;
    if (scan->self_copy) {
        plan->verdict = AS_REFUSE_SELF_COPY;
        return 0;
    }
    if (action == AS_OFF) {
        for (i = 0; i < scan->count; i++)
            if (scan->item[i].kind == AS_OURS)
                plan->remove[i] = AS_RM_ICON;
        return 1;
    }
    if (!scan->complete) {
        plan->verdict = AS_REFUSE_INCOMPLETE;
        return 0;
    }
    for (i = 0; i < scan->count; i++)
        if (scan->item[i].kind == AS_UNREADABLE) {
            plan->verdict = AS_REFUSE_UNREADABLE;
            plan->culprit = i;
            return 0;
        }
    for (i = 0; i < scan->count; i++)
        if (scan->item[i].kind == AS_FOREIGN && named(&scan->item[i], program)) {
            plan->verdict = AS_REFUSE_BLOCKED;
            plan->culprit = i;
            return 0;
        }
    if (action == AS_ON)
        for (i = 0; i < scan->count; i++)
            if (starts_other(scan->item[i].kind)) {
                plan->verdict = AS_REFUSE_NEEDS_REPLACE;
                plan->culprit = i;
                return 0;
            }

    plan->write = 1;
    /* Tool types: of this Dockbar's start icon, else of the one replaced. */
    for (i = 0; i < scan->count && plan->types_from < 0; i++)
        if (scan->item[i].kind == AS_OURS && named(&scan->item[i], program))
            plan->types_from = i;
    for (i = 0; i < scan->count && plan->types_from < 0; i++)
        if (scan->item[i].kind == AS_OURS)
            plan->types_from = i;
    for (i = 0; i < scan->count && plan->types_from < 0; i++)
        if (scan->item[i].kind == AS_OTHER || scan->item[i].kind == AS_MISSING)
            plan->types_from = i;

    /* The icon of the program's name is written over; others go. */
    for (i = 0; i < scan->count; i++) {
        int kind = scan->item[i].kind, icon = named(&scan->item[i], program) ? 0 : AS_RM_ICON;
        if (kind == AS_OURS || kind == AS_OTHER || kind == AS_MISSING)
            plan->remove[i] = icon;
        else if (kind == AS_COPY)
            plan->remove[i] = AS_RM_FILE | icon;
        else if (kind == AS_COPY_FILE)
            plan->remove[i] = AS_RM_FILE;
    }
    return 1;
}

int as_still_starts(const struct AsScan *scan, const struct AsPlan *plan, const int *failed, int i,
                    const char *program)
{
    (void)plan;
    if (i < 0 || i >= scan->count || scan->item[i].kind == AS_COPY_FILE)
        return 0;
    return (failed[i] & AS_RM_ICON) && !named(&scan->item[i], program);
}
