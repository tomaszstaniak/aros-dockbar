/* Development probe, not shipped: runs Dockbar's config save steps one by
 * one on PATH (a copy is made first) and logs each result with errno and
 * IoErr(), for file systems where a save fails.
 *   save_probe PATH LOGFILE */
#include <proto/dos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static FILE *out;
static void step(const char *what, int failed)
{
    fprintf(out, "%-28s %s errno=%d ioerr=%ld\n", what, failed ? "FAIL" : "ok", errno, (long)IoErr());
    fflush(out);
    errno = 0;
}

int main(int argc, char **argv)
{
    char path[512], tmp[520], old[520];
    if (argc < 3 || !(out = fopen(argv[2], "w"))) return 20;
    snprintf(path, sizeof path, "%s.probe", argv[1]);
    snprintf(tmp, sizeof tmp, "%s.new", path);
    snprintf(old, sizeof old, "%s.old", path);
    FILE *f = fopen(path, "wb");
    step("create primary", !f || fputs("x\n", f) < 0 || fclose(f));
    f = fopen(path, "rb");
    step("read primary", !f);
    if (f) fclose(f);
    f = fopen(old, "rb");
    step("open .old (expect ENOENT)", f != NULL);
    if (f) fclose(f);
    f = fopen(tmp, "wb");
    step("create .new", !f);
    if (f) step("close .new", fputs("y\n", f) < 0 || fclose(f));
    step("remove .old (expect ENOENT)", remove(old) != 0);
    step("rename primary -> .old", rename(path, old) != 0);
    step("rename .new -> primary", rename(tmp, path) != 0);
    step("remove .old", remove(old) != 0);
    step("remove primary", remove(path) != 0);
    fclose(out);
    return 0;
}
