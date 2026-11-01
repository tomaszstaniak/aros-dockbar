#include "docks.h"
#include "layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct DockSet set, loaded;
static struct DockModel v1;
static char path[512], v1path[512];

static void put(const char *file, const char *text)
{
    FILE *f = fopen(file, "wb");
    assert(f && fputs(text, f) >= 0 && !fclose(f));
}
static long size_of(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}
static const char *v1_text =
    "DOCKBAR\t1\nPOSITION\t1\t200\t501\nROOT\t5\n"
    "ITEM\t1\t2\t\tApplications\t\nITEM\t2\t0\t\t\t\nITEM\t4\t0\tSYS:Tools\tTools\tRAM:t.svg\n"
    "ITEM\t2\t0\t\t\t\nITEM\t3\t0\t\tClock\t\nGROUP\t2\t3\n"
    "ITEM\t0\t0\tSYS:Prefs\tPrefs\t\nITEM\t0\t0\tSYS:System/Shell\tShell\tSYS:x.info\n"
    "ITEM\t0\t0\tSYS:Utilities/MultiView\tMultiView\t\nEND\n";

static void migrates_0_1_without_loss(void)
{
    struct DockPosition pos = {0};
    struct DockPrefs prefs = {48, 2, 1};
    put(v1path, v1_text);
    long before = size_of(v1path);
    assert(dock_config_load(&v1, &pos, v1path) == 1);
    dock_set_from_v1(&set, &v1, &pos, &prefs);
    assert(dock_set_valid(&set));
    struct DockInstance *d = &set.docks[0];
    assert(d->place.mode == DOCK_PLACE_FREE && d->place.x == 200 && d->place.y == 501);
    assert(d->look.icon == 48 && d->look.corner == 2 && d->place.locked == 1);
    assert(!memcmp(&d->model, &v1, sizeof v1));
    assert(!strcmp(d->model.items[2].icon, "RAM:t.svg"));
    assert(dock_set_save(&set, path) == 1);
    assert(dock_set_load(&loaded, path) == 1 && !memcmp(&loaded, &set, sizeof set));
    assert(size_of(v1path) == before);  /* 0.1 file kept as the way back */
}
static void docks_are_independent(void)
{
    int b = dock_set_add(&set, "Side");
    assert(b == 1 && set.docks[b].id == 2 && !strcmp(set.docks[b].name, "Side"));
    struct DockInstance *side = &set.docks[b];
    side->look.orientation = DOCK_VERTICAL;
    side->place.mode = DOCK_PLACE_LEFT;
    assert(dock_add(&side->model, 0, "SYS:Prefs", 0) == 1);  /* same program in two docks */
    assert(dock_add(&side->model, 0, "SYS:Prefs.info", 1) == 0);
    assert(dock_add_kind(&side->model, 0, DOCK_CLOCK, "clock:seconds", 1) == 1);
    assert(dock_add_kind(&side->model, 0, DOCK_SPACER, NULL, 1) == 1);
    assert(dock_set_save(&set, path) == 1 && dock_set_load(&loaded, path) == 1);
    assert(!memcmp(&loaded, &set, sizeof set));
    assert(loaded.docks[0].look.orientation == DOCK_HORIZONTAL);
    assert(!strcmp(loaded.docks[1].model.items[2].path, "clock:seconds"));
}
static void moves_between_docks(void)
{
    struct DockSet before = set;
    /* App from the first dock's group into the side dock root. */
    assert(dock_set_move(&set, 0, 2, 2, 1, 0, 0) == 1);
    assert(set.docks[0].model.groups[1].count == 2);
    assert(!strcmp(set.docks[1].model.items[0].path, "SYS:Utilities/MultiView"));
    /* Prefs already in the side dock: a move there is refused, nothing changes. */
    struct DockSet snapshot = set;
    assert(dock_set_move(&set, 0, 2, 0, 1, 0, 0) == 0 && !memcmp(&snapshot, &set, sizeof set));
    /* The whole group moves with its members. */
    assert(dock_set_move(&set, 0, 0, 0, 1, 0, 1) == 1);
    assert(set.docks[1].model.items[1].kind == DOCK_GROUP);
    int n = 0;
    struct DockItem *kids = dock_items(&set.docks[1].model, set.docks[1].model.items[1].group, &n);
    assert(kids && n == 2 && !strcmp(kids[1].path, "SYS:System/Shell"));
    assert(set.docks[0].model.count == 4 && !set.docks[0].model.groups[1].used);
    /* Invalid moves leave the set alone. */
    snapshot = set;
    assert(dock_set_move(&set, 0, 0, 99, 1, 0, 0) == -1 && !memcmp(&snapshot, &set, sizeof set));
    assert(dock_set_move(&set, 1, 0, 1, 0, 1, 0) == -1 && !memcmp(&snapshot, &set, sizeof set));
    set = before;
}
static void create_duplicate_remove(void)
{
    int c = dock_set_duplicate(&set, 1);
    assert(c == 2 && set.docks[c].id == 3 && !strcmp(set.docks[c].name, "Side copy"));
    assert(!memcmp(&set.docks[c].model, &set.docks[1].model, sizeof set.docks[1].model));
    assert(dock_set_remove(&set, 1) == 1 && set.count == 2 && set.docks[1].id == 3);
    assert(dock_set_find(&set, 3) == 1 && dock_set_find(&set, 2) == -1);
    assert(dock_set_remove(&set, 0) == 1 && !dock_set_remove(&set, 0));  /* last stays */
    while (dock_set_add(&set, NULL) >= 0) {}
    assert(set.count == DOCK_MAX_DOCKS && dock_set_valid(&set));
    assert(dock_set_duplicate(&set, 0) == -1);
}
static void damaged_or_future_files_are_kept(void)
{
    const char *bad[] = {"DOCKBAR\t3\t2\nEND\n", "DOCKBAR\t2\t2\nDOCK\t1\tA\nLOOK\t40\n", "garbage"};
    for (int i = 0; i < 3; i++) {
        put(path, bad[i]);
        long n = size_of(path);
        assert(dock_set_load(&loaded, path) == -1);
        assert(dock_set_save(&set, path) == 0 && size_of(path) == n);
    }
    remove(path);
    struct DockSet invalid = set;
    invalid.docks[0].look.icon = 99;
    assert(!dock_set_save(&invalid, path) && size_of(path) == -1);
    invalid = set;
    strcpy(invalid.docks[1].name, "a\tb");
    assert(!dock_set_valid(&invalid));
    assert(dock_set_load(&loaded, path) == 0);
}
static void placement_stays_on_screen(void)
{
    struct DockPlace p;
    int x, y;
    dock_place_default(&p);
    dock_place_resolve(&p, 1024, 768, 20, 300, 56, &x, &y);
    assert(x == 362 && y == 768 - 56 - 8);
    p.mode = DOCK_PLACE_RIGHT; p.align = DOCK_ALIGN_START; p.offset = 0;
    dock_place_resolve(&p, 1024, 768, 20, 56, 400, &x, &y);
    assert(x == 1024 - 56 && y == 28);
    p.mode = DOCK_PLACE_LEFT; p.align = DOCK_ALIGN_END;
    dock_place_resolve(&p, 1024, 768, 20, 56, 400, &x, &y);
    assert(x == 0 && y == 768 - 400 - 8);
    p.mode = DOCK_PLACE_TOP; p.align = DOCK_ALIGN_CENTER; p.offset = 4;
    dock_place_resolve(&p, 1024, 768, 20, 300, 56, &x, &y);
    assert(x == 362 && y == 24);
    p.mode = DOCK_PLACE_FREE; p.valid = 1; p.x = 5000; p.y = -50;
    dock_place_resolve(&p, 1024, 768, 20, 300, 56, &x, &y);
    assert(x == 724 && y == 20);
    dock_place_resolve(&p, 1024, 768, 20, 2000, 900, &x, &y);
    assert(x == 0 && y == 20);
}
/* A 0.2 file: transparency and glass per dock, no defaults record. */
static const char *v2_text =
    "DOCKBAR\t2\t3\nDOCK\t1\tBottom\nLOOK\t40\t5\t0\t1\t0\t60\t1\n"
    "PLACE\t2\t1\t8\t0\t0\t0\t1\nROOT\t1\nITEM\t0\t0\tSYS:Prefs\tPrefs\t\nENDDOCK\n"
    "DOCK\t2\tSide\nLOOK\t48\t3\t2\t2\t1\t0\t0\nPLACE\t3\t0\t4\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n";

static void version2_keeps_its_look(void)
{
    struct DockAppearance a;
    put(path, v2_text);
    assert(dock_set_load(&set, path) == 1 && set.count == 2);
    /* 0.2 drew dark, with a bar and a grip: those become the defaults. */
    assert(set.defaults.theme == DOCK_THEME_DARK && set.defaults.background == DOCK_BG_BAR && set.defaults.grip == 1);
    assert(set.docks[0].look.transparency == 60 && set.docks[0].look.glass == 1);
    assert(set.docks[1].look.transparency == 0 && set.docks[1].look.glass == 0);
    assert(set.docks[0].look.theme == DOCK_INHERIT && set.docks[0].look.background == DOCK_INHERIT &&
           set.docks[0].look.grip == DOCK_INHERIT);
    dock_appearance_of(&set, 0, &a);
    assert(a.theme == DOCK_THEME_DARK && a.background == DOCK_BG_BAR && a.transparency == 60 && a.glass == 1 && a.grip == 1);
    /* Saved again it is version 4 and reads back the same. */
    assert(dock_set_save(&set, path) == 1);
    FILE *f = fopen(path, "rb");
    char head[64] = "";
    assert(f && fgets(head, sizeof head, f) && !fclose(f));
    assert(!strncmp(head, "DOCKBAR\t10\t", 11));
    assert(dock_set_load(&loaded, path) == 1 && !memcmp(&loaded, &set, sizeof set));

    /* Inherit and "no background" are different values: changing the default
     * reaches only docks that inherit. */
    set.docks[1].look.background = DOCK_BG_NONE;
    set.defaults.background = DOCK_BG_BAR;
    set.defaults.theme = DOCK_THEME_LIGHT;
    set.defaults.grip = 0;
    dock_appearance_of(&set, 0, &a);
    assert(a.background == DOCK_BG_BAR && a.theme == DOCK_THEME_LIGHT && a.grip == 0);
    dock_appearance_of(&set, 1, &a);
    assert(a.background == DOCK_BG_NONE && a.theme == DOCK_THEME_LIGHT);
    set.docks[1].look.grip = 1;
    dock_appearance_of(&set, 1, &a);
    assert(a.grip == 1);
    assert(dock_set_save(&set, path) == 1 && dock_set_load(&loaded, path) == 1 && !memcmp(&loaded, &set, sizeof set));

    /* Out-of-range values are refused, in memory and in files. */
    struct DockSet bad = set;
    bad.docks[0].look.background = 2;
    assert(!dock_set_valid(&bad));
    bad = set;
    bad.defaults.theme = DOCK_INHERIT;  /* defaults cannot inherit */
    assert(!dock_set_valid(&bad));
    bad = set;
    bad.docks[0].look.transparency = -2;
    assert(!dock_set_valid(&bad));
    put(path, "DOCKBAR\t2\t2\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t0\nPLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&loaded, path) == -1);  /* version 2 had no inherit */
    put(path, "DOCKBAR\t3\t2\nDEFAULTS\t9\t0\t0\t0\t1\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&loaded, path) == -1);
    remove(path);

    /* A 0.3 file: every dock and item shows icons, as before. */
    put(path, "DOCKBAR\t3\t2\nDEFAULTS\t2\t0\t0\t0\t1\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t1\nITEM\t0\t0\tSYS:Prefs\tPrefs\t\nENDDOCK\nEND\n");
    assert(dock_set_load(&loaded, path) == 1);
    assert(loaded.docks[0].look.present == 0 && loaded.docks[0].look.buttons == 0 &&
           loaded.docks[0].model.items[0].present == DOCK_PRESENT_DOCK);
    /* Presentation per dock and per item round-trips; out of range is refused. */
    loaded.docks[0].look.present = 1;
    loaded.docks[0].look.buttons = 1;
    loaded.docks[0].model.items[0].present = DOCK_PRESENT_ICON_TEXT;
    assert(loaded.defaults.text == 0);  /* the system's text, as before */
    assert(loaded.docks[0].look.beside == 0);  /* 0.3 had no icon and text: new layout */
    loaded.docks[0].look.beside = 1;
    loaded.defaults.text = 1;
    assert(dock_set_save(&loaded, path) == 1);
    struct DockSet again;
    assert(dock_set_load(&again, path) == 1 && !memcmp(&again, &loaded, sizeof again));
    bad = loaded;
    bad.docks[0].look.present = 3;
    assert(!dock_set_valid(&bad));
    bad = loaded;
    bad.docks[0].model.items[0].present = 4;
    assert(!dock_set_valid(&bad));
    put(path, "DOCKBAR\t4\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t1\nITEM\t0\t0\tSYS:Prefs\tPrefs\t\nENDDOCK\nEND\n");
    assert(dock_set_load(&again, path) == -1);  /* version 4 items carry a presentation */
    /* A command item round-trips with its working directory and flags; a
     * command inside a group, or without a command, is refused. */
    {
        static struct DockSet c, r, bad2;
        c = loaded;
        remove(path);  /* the invalid file above is never replaced by a save */
        struct DockItem cmd;
        memset(&cmd, 0, sizeof cmd);
        cmd.kind = DOCK_COMMAND;
        strcpy(cmd.path, "C:Echo \"two words\" TO RAM:out.txt");
        strcpy(cmd.label, "Echo test");
        strcpy(cmd.dir, "RAM:");
        cmd.run = DOCK_RUN_SHELL;
        assert(dock_insert(&c.docks[0].model, 0, &cmd, 0) == 1 && dock_set_valid(&c));
        assert(dock_set_save(&c, path) == 1);
        assert(dock_set_load(&r, path) == 1 && !memcmp(&r, &c, sizeof r));
        assert(r.docks[0].model.items[0].kind == DOCK_COMMAND && !strcmp(r.docks[0].model.items[0].dir, "RAM:") &&
               r.docks[0].model.items[0].run == DOCK_RUN_SHELL);
        cmd.path[0] = 0;
        assert(dock_insert(&c.docks[0].model, 0, &cmd, 0) == -1);
        strcpy(cmd.path, "Run x");
        assert(dock_insert(&c.docks[0].model, 1, &cmd, 0) == -1 || !c.docks[0].model.groups[0].used);
        bad2 = c;
        bad2.docks[0].model.items[1].run = 9;
        assert(!dock_set_valid(&bad2));
        bad2 = c;
        strcpy(bad2.docks[0].model.items[1].dir, "RAM:");  /* only commands have a directory */
        assert(!dock_set_valid(&bad2));
    }
    /* On drop (programs, commands) and Open on click (drawers) round-trip;
     * files of 0.4-test3 and the weather builds (version 7) load with both
     * off, so existing items behave as before. Values are refused on other
     * kinds and out of range. */
    {
        static struct DockSet d, r, bad3;
        d = loaded;
        remove(path);
        struct DockItem drawer;
        memset(&drawer, 0, sizeof drawer);
        drawer.kind = DOCK_DRAWER;
        strcpy(drawer.path, "SYS:Tools");
        strcpy(drawer.label, "Tools");
        drawer.open = DOCK_OPEN_PANEL;
        assert(dock_insert(&d.docks[0].model, 0, &drawer, 0) == 1);
        d.docks[0].model.items[1].on_drop = DOCK_DROP_OPEN;  /* SYS:Prefs, a program item */
        assert(dock_set_valid(&d) && dock_set_save(&d, path) == 1);
        assert(dock_set_load(&r, path) == 1 && !memcmp(&r, &d, sizeof r));
        assert(r.docks[0].model.items[0].open == DOCK_OPEN_PANEL && r.docks[0].model.items[1].on_drop == DOCK_DROP_OPEN);
        bad3 = d;
        bad3.docks[0].model.items[0].on_drop = DOCK_DROP_OPEN;  /* a drawer opens no files */
        assert(!dock_set_valid(&bad3));
        bad3 = d;
        bad3.docks[0].model.items[1].open = DOCK_OPEN_PANEL;  /* only drawers have a panel */
        assert(!dock_set_valid(&bad3));
        bad3 = d;
        bad3.docks[0].model.items[1].on_drop = 2;
        assert(!dock_set_valid(&bad3));
        put(path, "DOCKBAR\t7\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\n"
                 "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t2\nITEM\t4\t0\tSYS:Tools\tTools\t\t0\t\t0\n"
                 "ITEM\t0\t0\tSYS:Utilities/MultiView\tMultiView\t\t0\t\t0\nENDDOCK\nEND\n");
        assert(dock_set_load(&r, path) == 1 && r.docks[0].model.items[0].open == DOCK_OPEN_WANDERER &&
               r.docks[0].model.items[1].on_drop == DOCK_DROP_ADD);
        remove(path);
    }
    /* A 0.4-test1 file keeps icon and text side by side. */
    put(path, "DOCKBAR\t4\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t2\t0\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t1\nITEM\t0\t0\tSYS:Prefs\tPrefs\t\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&again, path) == 1 && again.docks[0].look.beside == 1 && again.docks[0].look.present == 2);
    struct DockLook fresh_look;
    dock_look_default(&fresh_look);
    assert(fresh_look.beside == 0);  /* new docks: caption under the icon */
    remove(path);

    /* New docks inherit everything; 0.1 migration keeps 0.1's dark look. */
    dock_set_add(&set, "New");
    assert(set.docks[set.count - 1].look.theme == DOCK_INHERIT && set.docks[set.count - 1].look.transparency == DOCK_INHERIT);
    struct DockAppearance fresh;
    dock_appearance_default(&fresh);
    assert(fresh.theme == DOCK_THEME_AUTO && fresh.background == DOCK_BG_BAR && fresh.grip == 1 && fresh.transparency == 0);
}

/* The hover effect: Lift for new configurations, None for every file
 * written before version 9 (docks inheriting it), own values per dock
 * round-trip, out-of-range values are refused. */
static void hover_effect_setting(void)
{
    static struct DockSet s, r, bad;
    struct DockAppearance a;
    dock_appearance_default(&a);
    assert(a.hover == DOCK_HOVER_LIFT);
    dock_appearance_legacy(&a);
    assert(a.hover == DOCK_HOVER_NONE);
    put(path, "DOCKBAR\t8\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&s, path) == 1);
    assert(s.defaults.hover == DOCK_HOVER_NONE && s.docks[0].look.hover == DOCK_INHERIT);
    dock_appearance_of(&s, 0, &a);
    assert(a.hover == DOCK_HOVER_NONE);
    int b = dock_set_add(&s, "Side");
    assert(b == 1 && s.docks[b].look.hover == DOCK_INHERIT);
    s.defaults.hover = DOCK_HOVER_LIFT;
    s.docks[b].look.hover = DOCK_HOVER_MAGNIFY;
    assert(dock_set_save(&s, path) == 1);
    assert(dock_set_load(&r, path) == 1 && !memcmp(&r, &s, sizeof r));
    dock_appearance_of(&r, 0, &a);
    assert(a.hover == DOCK_HOVER_LIFT);
    dock_appearance_of(&r, 1, &a);
    assert(a.hover == DOCK_HOVER_MAGNIFY);
    /* The file carries the new fields. */
    FILE *f = fopen(path, "rb");
    char line[256];
    assert(f && fgets(line, sizeof line, f) && !strncmp(line, "DOCKBAR\t10\t", 11));
    assert(fgets(line, sizeof line, f) && !strcmp(line, "DEFAULTS\t2\t0\t0\t0\t1\t0\t1\t0\n"));
    fclose(f);
    bad = s;
    bad.defaults.hover = 3;
    assert(!dock_set_valid(&bad));
    bad = s;
    bad.docks[0].look.hover = -2;
    assert(!dock_set_valid(&bad));
    bad = s;
    bad.defaults.hover = DOCK_INHERIT;  /* the defaults have no one to inherit from */
    assert(!dock_set_valid(&bad));
    put(path, "DOCKBAR\t9\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\t7\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&r, path) == -1);
    put(path, "DOCKBAR\t11\t2\nEND\n");
    assert(dock_set_load(&r, path) == -1);  /* a later version is left alone */
    remove(path);
}

/* Running indicators: off by default and for every file written before
 * version 10 (docks inheriting it); own values per dock round-trip; only
 * 0 and 1 (and Inherit for a dock) are accepted. */
static void running_setting(void)
{
    static struct DockSet s, r, bad;
    struct DockAppearance a;
    dock_appearance_default(&a);
    assert(a.running == 0);
    put(path, "DOCKBAR\t9\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\t1\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&s, path) == 1);
    assert(s.defaults.running == 0 && s.docks[0].look.running == DOCK_INHERIT);
    int b = dock_set_add(&s, "Side");
    assert(b == 1 && s.docks[b].look.running == DOCK_INHERIT);
    s.defaults.running = 1;
    s.docks[b].look.running = 0;
    assert(dock_set_save(&s, path) == 1);
    assert(dock_set_load(&r, path) == 1 && !memcmp(&r, &s, sizeof r));
    dock_appearance_of(&r, 0, &a);
    assert(a.running == 1);
    dock_appearance_of(&r, 1, &a);
    assert(a.running == 0);
    bad = s;
    bad.defaults.running = 2;
    assert(!dock_set_valid(&bad));
    bad = s;
    bad.defaults.running = DOCK_INHERIT;
    assert(!dock_set_valid(&bad));
    bad = s;
    bad.docks[0].look.running = 2;
    assert(!dock_set_valid(&bad));
    put(path, "DOCKBAR\t10\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\t1\t3\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\t-1\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&r, path) == -1);
    /* A version 10 file without the new field is damaged, not old. */
    put(path, "DOCKBAR\t10\t2\nDEFAULTS\t2\t0\t0\t0\t1\t0\t1\nDOCK\t1\tA\nLOOK\t40\t5\t0\t1\t0\t-1\t-1\t-1\t-1\t-1\t0\t0\t0\t-1\n"
             "PLACE\t2\t1\t8\t0\t0\t0\t0\nROOT\t0\nENDDOCK\nEND\n");
    assert(dock_set_load(&r, path) == -1);
    remove(path);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    snprintf(path, sizeof path, "%s/docks.config", dir);
    snprintf(v1path, sizeof v1path, "%s/dock.config", dir);
    remove(path);
    migrates_0_1_without_loss();
    docks_are_independent();
    moves_between_docks();
    create_duplicate_remove();
    damaged_or_future_files_are_kept();
    placement_stays_on_screen();
    version2_keeps_its_look();
    hover_effect_setting();
    running_setting();
    puts("PASS docks: 0.1 migration, 0.2 files keep their look, inherit vs own values, independent docks, cross-dock moves, create/duplicate/remove, damaged files kept, placement, hover effect, running indicators");
    return 0;
}
