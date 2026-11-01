#include "command.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char out[300];
    assert(dock_command_console("Echo test", out, sizeof out) && !strcmp(out, "CON:40/40/640/200/Echo test/AUTO/CLOSE/WAIT"));
    /* Slashes anywhere, the first character included, cannot reach the spec. */
    assert(dock_command_console("/AUTO/x", out, sizeof out) && !strcmp(out, "CON:40/40/640/200/-AUTO-x/AUTO/CLOSE/WAIT"));
    assert(dock_command_console("a/b", out, sizeof out) && !strcmp(out, "CON:40/40/640/200/a-b/AUTO/CLOSE/WAIT"));
    assert(dock_command_console("", out, sizeof out) && !strcmp(out, "CON:40/40/640/200/Command/AUTO/CLOSE/WAIT"));
    assert(dock_command_console("\t\n", out, sizeof out) && !strcmp(out, "CON:40/40/640/200/Command/AUTO/CLOSE/WAIT"));
    assert(!dock_command_console("Name", out, 20));
    /* Exactly five slashes: the title never adds a field. */
    for (const char *t = "/////"; *t; t++) {
        dock_command_console(t, out, sizeof out);
        int slashes = 0;
        for (char *c = out; *c; c++) slashes += *c == '/';
        assert(slashes == 7);
    }

    assert(dock_command_script_line("S:hello.script", out, sizeof out) && !strcmp(out, "Execute \"S:hello.script\""));
    assert(dock_command_script_line("RAM:my dir/run me", out, sizeof out) && !strcmp(out, "Execute \"RAM:my dir/run me\""));
    assert(dock_command_script_line("a\"b*c", out, sizeof out) && !strcmp(out, "Execute \"a*\"b**c\""));
    assert(dock_command_script_line("rel.script", out, sizeof out) && !strcmp(out, "Execute \"rel.script\""));
    assert(!dock_command_script_line("", out, sizeof out) && !dock_command_script_line("abc", out, 12));
    assert(dock_command_quoted("SYS:Utilities/MultiView", "RAM Disk:my \"dock\"/ReadMe", out, sizeof out) &&
           !strcmp(out, "SYS:Utilities/MultiView \"RAM Disk:my *\"dock*\"/ReadMe\""));
    assert(!dock_command_quoted("MultiView", "", out, sizeof out));
    /* Dropped files: {files} becomes the quoted list; scripts get them as
     * Execute arguments; no {files} means no files are passed at all. */
    {
        const char *one[] = { "RAM Disk:a b.txt" };
        const char *three[] = { "Work:x.iff", "RAM:q\"uote*star", "DH0:Assign Name/y" };
        assert(dock_command_with_files("MultiView {files}", one, 1, out, sizeof out) == 1 &&
               !strcmp(out, "MultiView \"RAM Disk:a b.txt\""));
        assert(dock_command_with_files("Copy {files} TO RAM:", three, 3, out, sizeof out) == 1 &&
               !strcmp(out, "Copy \"Work:x.iff\" \"RAM:q*\"uote**star\" \"DH0:Assign Name/y\" TO RAM:"));
        assert(dock_command_with_files("List", one, 1, out, sizeof out) == -1);
        assert(dock_command_with_files("Echo {files}", one, 0, out, sizeof out) == 0);
        assert(dock_command_with_files("Echo {files}", three, 3, out, 30) == 0);  /* does not fit */
        assert(dock_command_script_files("S:view.script", three, 2, out, sizeof out) == 1 &&
               !strcmp(out, "Execute \"S:view.script\" FILES \"*\"Work:x.iff*\"\" \"*\"RAM:q***\"uote****star*\"\""));
        assert(dock_command_script_files("s", one, 1, out, 12) == 0);
        /* {files} must stand alone, outside quotes, once. */
        assert(dock_command_files_token("List {files}") == 1);
        assert(dock_command_files_token("{files} TO RAM:") == 1);
        assert(dock_command_files_token("Copy\t{files}\tTO RAM:") == 1);
        assert(dock_command_files_token("List") == 0);
        assert(dock_command_files_token("Echo \"{files}\"") == -1);
        assert(dock_command_files_token("Echo \"a {files} b\"") == -1);
        assert(dock_command_files_token("Echo x{files}") == -1);
        assert(dock_command_files_token("Echo {files}.bak") == -1);
        assert(dock_command_files_token("Echo {files} {files}") == -1);
        assert(dock_command_files_token("Echo \"q*\"uote\" {files}") == 1);  /* *" stays inside */
        assert(dock_command_with_files("Echo \"{files}\"", one, 1, out, sizeof out) == -2);
        assert(dock_command_with_files("Echo x{files}", one, 1, out, sizeof out) == -2);
        /* Script with several names: each its own quoted argument. */
        const char *odd[] = { "RAM:has space", "RAM:q\"x", "RAM:st*r", "RAM:end.info" };
        /* The form measured on ABIv11 2026.09: <FILES> unquoted in the
         * script gave these four names exactly (g1e-sG.log). */
        assert(dock_command_script_files("RAM:s", odd, 4, out, sizeof out) == 1 &&
               !strcmp(out, "Execute \"RAM:s\" FILES \"*\"RAM:has space*\"\" \"*\"RAM:q***\"x*\"\" \"*\"RAM:st****r*\"\" \"*\"RAM:end.info*\"\""));
        /* Which scripts take files. */
        assert(dock_command_script_takes_files(".KEY FILES/M\nList <FILES>\n"));
        assert(dock_command_script_takes_files("; viewer\n\n.bra {\n.ket }\n.key FILES/M/A,QUIET/S\nx {FILES}\n"));
        assert(dock_command_script_takes_files(".K F=FILES/M\n"));
        assert(dock_command_script_takes_files(".KEY QUIET/S, files/m\n"));
        assert(!dock_command_script_takes_files(".KEY F/M\nList <F>\n"));
        assert(!dock_command_script_takes_files(".KEY FILES\n"));
        assert(!dock_command_script_takes_files(".KEY FILE/M\n"));
        assert(!dock_command_script_takes_files("Echo hello\n.KEY FILES/M\n"));
        assert(!dock_command_script_takes_files(""));
        /* Lines the Shell reads as meant, and lines it would cut or split. */
        assert(dock_command_line_safe("RAM:p/x \"RAM:semi;colon\" \"RAM:q*\"uote\""));
        assert(dock_command_line_safe("Echo a ; a comment the user wrote"));
        assert(dock_command_line_safe("RAM:p/x \"RAM:pi|pe\""));
        assert(!dock_command_line_safe("RAM:p/x \"RAM:q*\"uote\" \"RAM:semi;colon\""));
        assert(!dock_command_line_safe("RAM:p/x \"RAM:x*\"y;z\""));
        assert(!dock_command_line_safe("Execute \"s\" FILES \"*\"RAM:semi;colon*\"\""));
        assert(!dock_command_line_safe("Execute \"s\" FILES \"*\"RAM:pi|pe*\"\""));
        assert(dock_command_line_safe("Execute \"s\" FILES \"*\"RAM:a b*\"\""));
    }
    /* A Shell program started with dropped files: program and every file
     * quoted and escaped (Workbench's own Shell start does not escape). */
    {
        const char *odd[] = { "RAM Disk:dt2/a b.txt", "RAM:q\"x", "RAM:st*r" };
        assert(dock_command_program_files("RAM Disk:p/prog", odd, 3, out, sizeof out) == 1 &&
               !strcmp(out, "\"RAM Disk:p/prog\" \"RAM Disk:dt2/a b.txt\" \"RAM:q*\"x\" \"RAM:st**r\""));
        assert(dock_command_program_files("RAM:p/x", NULL, 0, out, sizeof out) == 1 && !strcmp(out, "\"RAM:p/x\""));
        assert(dock_command_program_files("p", odd, 3, out, 20) == 0);
    }
    puts("PASS command: console title cleaned (slashes, control characters, empty), AmigaDOS quoting of script paths, script FILES form and .KEY check, Shell misreading ; and | after escaped quotes");
    return 0;
}
