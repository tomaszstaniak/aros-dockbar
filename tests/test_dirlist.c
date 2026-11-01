#include "dirlist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char out[64];
    assert(dock_dir_listed("Calculator") && dock_dir_listed("a.info.txt") && dock_dir_listed(".infox"));
    assert(!dock_dir_listed("Calculator.info") && !dock_dir_listed("DISK.INFO") && !dock_dir_listed(".info"));
    assert(!dock_dir_listed("") && !dock_dir_listed(".") && !dock_dir_listed(".."));
    /* Drawers first, then case-insensitive, then exact bytes. */
    assert(dock_dir_compare(1, "zeta", 0, "alpha") < 0 && dock_dir_compare(0, "alpha", 1, "zeta") > 0);
    assert(dock_dir_compare(0, "apple", 0, "Banana") < 0 && dock_dir_compare(0, "Banana", 0, "apple") > 0);
    assert(dock_dir_compare(0, "ReadMe", 0, "readme") != 0 && dock_dir_compare(0, "a", 0, "a") == 0);
    assert(dock_dir_compare(0, "ab", 0, "abc") < 0);
    /* Parents stop at the volume. */
    assert(dock_dir_parent("Work:a/b", out, sizeof out) && !strcmp(out, "Work:a"));
    assert(dock_dir_parent("Work:a", out, sizeof out) && !strcmp(out, "Work:"));
    assert(!dock_dir_parent("Work:", out, sizeof out) && !dock_dir_parent("noColon", out, sizeof out));
    assert(dock_dir_join("Work:", "x", out, sizeof out) && !strcmp(out, "Work:x"));
    assert(dock_dir_join("Work:a", "x y", out, sizeof out) && !strcmp(out, "Work:a/x y"));
    assert(!dock_dir_join("Work:a", "0123456789012345678901234567890123456789012345678901234567890", out, sizeof out));
    assert(dock_dir_info_base("Calc.info", out, sizeof out) && !strcmp(out, "Calc"));
    assert(dock_dir_info_base("a b.INFO", out, sizeof out) && !strcmp(out, "a b"));
    assert(!dock_dir_info_base(".info", out, sizeof out) && !dock_dir_info_base("Calc", out, sizeof out));
    assert(!dock_dir_info_base("Calc.info", out, 4));
    assert(dock_dir_samename("ReadMe", "README") && !dock_dir_samename("ReadMe", "ReadMe2") && !dock_dir_samename("a", ""));
    puts("PASS dirlist: .info and dot entries hidden, drawers first then names without case, parent stops at the volume, joining, .info to object");
    return 0;
}
