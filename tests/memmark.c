/* Development probe, not shipped: appends one line with free memory (after
 * a flush) to a log, numbered. Started from a dock command item, so marks
 * can be taken between actions without typing into a Shell (typed lines
 * stay in the console history and cost memory themselves).
 *   memmark LOGFILE */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 20;
    /* Asking for far too much makes exec expunge unused libraries and
     * fonts first, as Avail FLUSH does. */
    for (int i = 0; i < 3; i++) { APTR p = AllocMem(0x7ffffff0, MEMF_ANY); if (p) FreeMem(p, 0x7ffffff0); }
    ULONG mem = AvailMem(MEMF_ANY);
    int n = 0;
    FILE *in = fopen(argv[1], "r");
    if (in) { char line[160]; while (fgets(line, sizeof line, in)) n++; fclose(in); }
    FILE *out = fopen(argv[1], "a");
    if (!out) return 20;
    fprintf(out, "%d free=%lu\n", n + 1, (unsigned long)mem);
    fclose(out);
    return 0;
}
