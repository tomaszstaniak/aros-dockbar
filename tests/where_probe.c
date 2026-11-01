/* Development probe, not shipped: stands in for a widget provider and logs
 * the context it really runs in: argv, GetProgramDir(), the package root the
 * Weather provider derives from it (two levels up), whether the bundled CA
 * file is there, the current directory, CURL_CA_BUNDLE. Answers nothing.
 * Appends to RESULTS:dockbar-where.log. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void name_of(BPTR lock, char *out, LONG cap)
{
    if (!lock)
        snprintf(out, cap, "(none)");
    else if (!NameFromLock(lock, (STRPTR)out, cap))
        snprintf(out, cap, "(NameFromLock failed, IoErr %ld)", (long)IoErr());
}

int main(int argc, char **argv)
{
    FILE *f = fopen("RESULTS:dockbar-where.log", "a");
    if (!f) return 20;
    char name[600], root_name[600], cur[600], ca[700], var[600];
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR bin = GetProgramDir(), variant = bin ? ParentDir(bin) : BNULL,
         root = variant ? ParentDir(variant) : BNULL;
    name_of(bin, name, sizeof name);
    name_of(root, root_name, sizeof root_name);
    name_of(me->pr_CurrentDir, cur, sizeof cur);
    snprintf(ca, sizeof ca, "%s", root_name);
    int found = 0;
    if (root && AddPart((STRPTR)ca, (CONST_STRPTR)"certs/ca-bundle.crt", sizeof ca)) {
        BPTR l = Lock((CONST_STRPTR)ca, SHARED_LOCK);
        found = l != BNULL;
        if (l) UnLock(l);
    }
    const char *env = getenv("CURL_CA_BUNDLE");
    LONG got = GetVar((CONST_STRPTR)"CURL_CA_BUNDLE", (STRPTR)var, sizeof var, 0);
    fprintf(f, "argc=%d argv0=%s\n request=%s\n progdir=%s\n root=%s\n ca=%s found=%d\n"
               " currentdir=%s\n getenv(CURL_CA_BUNDLE)=%s GetVar=%s\n",
            argc, argv[0], argc > 1 ? argv[1] : "", name, root_name, ca, found, cur,
            env ? env : "(unset)", got >= 0 ? var : "(unset)");
    fclose(f);
    if (root) UnLock(root);
    if (variant) UnLock(variant);
    return 10;
}
