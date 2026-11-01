Dockbar @VERSION@ - test release
================================

An Amiga-style dock for AROS: application launchers, icon groups, drawers,
separators, spaces and clocks in one or more movable bars, horizontal or
vertical.

This is a test release for testers. It is not a final version. Please report
what works and what does not.

Requirements
------------
@REQUIREMENTS@

Installation
------------
Copy or unpack the Dockbar drawer to a disk that keeps files after a
restart, for example Work: or SYS:. Do not run it from RAM: if you want to
keep your settings. Double-click the Dockbar icon to start it. Only one
Dockbar runs at a time; it shows all your docks.

Starting Dockbar with the system: add this line to S:User-Startup
(create the file with a text editor if it does not exist), with the
place where you put Dockbar instead of Work:Dockbar:

  Run <NIL: >NIL: QUIET Work:Dockbar/Dockbar

If that place has a space in its name, give it a name of its own first:
Run does not keep quotes around the program's name.

  Assign Dockbar: "Work:My Programs/Dockbar"
  Run <NIL: >NIL: QUIET Dockbar:Dockbar

Keep the Run. S:User-Startup is carried out before Wanderer starts, and
without Run the system waits for Dockbar to end, so Wanderer and the
desktop never appear. Started this way, Dockbar shows its docks before
the desktop icons appear; drawer items and dropped icons work once
Wanderer is up. To log such a start, add --log RAM:dockbar.log at the
end of the Run line.

Using a dock
------------
- Click an icon to open the program or drawer. A quick second click is
  ignored, so a double-click opens it once.
- Names of icons appear when the pointer rests on them for half a
  second. While you drag an icon from Wanderer no names are shown:
  Wanderer does not repaint its desktop during its own drag, so they
  would leave traces. A bar still marks the icon under the pointer.
- Add a program: drag its icon from a Wanderer window onto a dock,
  between two icons (or onto a group, to put it in the group). A drawer
  or disk dropped on a dock becomes a drawer item with its own icon.
  Several icons dragged together are all added, in their order.
  The same program cannot be added twice to one place, also when it is
  reached by another name (for example SYS: and the disk's name).
  The same program may be in several docks.
- Make a group: drag a program onto another program on the dock, wait
  until it is highlighted (about half a second), then release.
  Click a group to open it. Hold a dragged program over a group and the
  group opens, so you can drop it at a chosen place inside.
- Reorder: drag an icon between other icons, in a dock or in a group.
  Drag an icon to another dock to move it there; a group moves with its
  programs. Drag an icon out of a group onto a dock to take it out.
- Edit or remove: right-click an icon (or Shift-click it). Change the name
  or the icon (.info or .svg file); for a clock, choose seconds and date.
  "Show as" chooses icon, text or icon and text for this item alone
  ("As the dock" follows the dock).
  Remove takes away only the shortcut; your programs and files are not
  touched.
  Separators and spaces work the same way: right-click or Shift-click
  one to remove it, drag it to move it. A plain click on them does
  nothing.
- Edit items...: the grip menu lists every item of that dock in order,
  also spaces and, on a dock without a bar, items that are hard to
  click. Select one, then Edit..., Remove, Move up or Move down.
- Add a command: choose "Add command..." in the grip menu. Give it a
  name and either an AmigaDOS command line (for example
  Dir "SYS:Prefs" ALL) or a script file, which is run with Execute.
  Working directory is where it runs (blank: SYS:); a script file named
  without a disk or drawer is looked for there. With "Open a Shell window"
  it gets a console window that stays open until you close it, and
  errors show there; without it, nothing is shown, not even an error.
  A command is shown as an icon (the Shell's, unless you choose one),
  text, or both, like other items. Dropping a program from Wanderer
  always adds it as a program, never as a command.
- Open files with a program or command: right-click the item, set
  "On drop" to "Open the files" (programs) or "Run with the files"
  (commands), and Save. Files dropped on the middle of that item are
  then opened with it; dropped between icons they still become
  shortcuts. A program gets the files as Workbench arguments, as when
  files are dropped on its icon in Wanderer; the program decides what
  it does with several (MultiView, for example, shows only the first).
  A command line gets them where it says {files}, for example
  List {files}. Write {files} once, as a word of its own and not inside
  quotes: Dockbar puts each file in quotes itself, so names with spaces,
  quotes or * are passed as they are. A command without {files} does not
  take files, and Save refuses {files} in quotes or glued to other text.
  A script file takes the files when its first line is
      .KEY FILES/M
  and it writes <FILES> where they go, without quotes around it, for
  example  List <FILES>  (Examples/ListFiles.script is such a script).
  Dockbar hands each name over already in quotes, so names with spaces,
  quotes or * arrive as they are. A script without FILES/M in its .KEY
  line is not run with files ("The script needs .KEY FILES/M"): the
  Shell would otherwise pass the names without quotes and split them.
  The AROS Shell misreads a ; or | in a name that follows a name with
  a double quote on the same line; Dockbar refuses such a drop instead
  of running the command on cut names.
  "On drop" is off for every existing item: nothing runs because a file
  was dropped until you choose it. Opening files also works on a locked
  dock; adding shortcuts there does not.
  Dropped on other items (a drawer, a clock, a separator) files are
  refused and nothing changes; files are never copied or moved.
- Weather (x86_64 package only): open the dock menu (click the grip at
  the dock's end), choose "Add widget...", select Weather and Add. Dockbar
  never adds it by itself, and an existing dock is not changed. In its
  settings search for your city, pick it with "Use selected city" (or
  type latitude and longitude), choose the units and Save. It needs a
  running TCP/IP stack (Prefs/Network); without one it shows "Offline"
  and keeps the last data it had. "Secure connection refused" usually
  means a wrong system date: set it in Prefs/Time. Details of every
  request are in WidgetState/weather.log. "Remove widget" in its
  settings removes it with its settings and saved forecast. Click the weather item for
  the hourly and seven-day forecast. Weather lives in the Widgets drawer next to
  the program: install and move Dockbar as the whole drawer, never the
  program file alone.
- Drawers: right-click a drawer item and set "Open on click" to
  "Panel" to see the drawer's contents beside the dock instead of a
  Wanderer window. The panel shows one drawer at a time, drawers first,
  then files by name, each with its own icon (or the system's).
  Click a file to open it, a drawer to go into it, and Back to return.
  Scroll with the wheel, the cursor keys or the bar on the right.
  "Open in Wanderer" opens the shown drawer in Wanderer, and Refresh
  reads it again. The panel follows changes on disks that report them
  (for example RAM Disk); on others, use Refresh. A drawer with very
  many entries is read in steps, so the dock stays usable meanwhile.
  While a drawer is read again, the panel keeps showing what it had.
  Icons once shown are kept in memory (at most about 3 MB) so that the
  panel opens at once next time; that memory is given back two minutes
  after the panel closes. An icon is loaded again when its .info file
  changes.
  The panel only shows files: it does not take drops. Existing drawer
  items keep opening in Wanderer until you change them.
- Move a dock: drag the dotted grip at its start (left end, or top end of
  a vertical dock). Moving it by hand makes it free-standing.
- A dock longer than the screen shows arrows at its ends; click an arrow,
  or hold a dragged icon on it, to scroll. Icons are made smaller first.

The grip menu
-------------
Click the dotted grip without dragging (or right-click it when the dock is
the active window) to open its menu. Ctrl-click anywhere on a dock opens
the same menu, also when the grip is hidden. Starting Dockbar a second
time (double-clicking its icon while it runs) opens Preferences.

- Preferences...   all docks and their settings
- Add separator, Add space, Add clock
- Edit items...    every item of the dock in a list (see above)
- Refresh icons    reload icons after you changed a program's .info
- About...         version, author and the licences of this release;
                   Copy version info puts the version, target and Weather
                   version on the clipboard (nothing about your settings)
- Quit             close Dockbar (all docks)

Escape also closes Dockbar while a dock is the active window.

Preferences
-----------
The list on the left starts with "Defaults for all docks", followed by
your docks. New adds a dock; Duplicate and Delete work on the selected
dock (they are not available while the defaults are selected). Delete
removes that dock and its shortcuts only. The line above the buttons
says what the shown page is for.

Defaults for all docks: Colours (Dark, Light, Automatic), Background
(Bar, None), Transparency %, Frosted glass, Grip (Shown, Hidden), Text,
Hover effect and Running indicators (Off, On). Every dock whose setting
is "Inherit" uses these values.

A dock has four tabs:

- Placement: Name, Orientation (horizontal or vertical), Position (free,
  or at the top, bottom, left or right edge), and for an edge Alignment
  and Distance from the edge.
- Content:
  - Show items as: Icons, Text (buttons with the name) or Icon and text
    (icon and name together as one button). Programs, drawers and groups
    follow it unless set otherwise in their editor; clocks, separators
    and spaces are not affected. A text button is as wide as its name,
    at most 180 pixels; a longer name ends in "..." and shows in full
    under the pointer. In a vertical dock all buttons are as wide as the
    widest one.
  - Text position (only for Icon and text): Under icon (the default for
    new docks) or Beside icon. Docks made with 0.4-test1 keep Beside
    icon. Icon and name are one button: they scroll, highlight, take
    clicks and take drops together.
  - Text buttons (not for Icons): Plain (the name on the bar, a plate
    only under the pointer) or Plates (every button on its own plate).
  - Icon size, Spacing.
  - Icon labels (None, On hover, Always) are for items shown as icons
    only. An item that already shows its name (Text, Icon and text) gets
    no label under it and no name tooltip, unless its name was shortened.
- Appearance: Colours, Background, Transparency % and Frosted glass,
  each either "Inherit" (the value from the defaults, shown in brackets,
  for example "Inherit (Bar)") or this dock's own value; Corner radius;
  Hover effect (Inherit, None, Lift, Magnify, see "Hover effect" below);
  Running indicators (Inherit, Off, On, see "Running indicators" below).
  Transparency, Frosted glass and Hover effect are not available while
  the dock has no background. See "Colours and backgrounds" below.
- Behaviour:
  - Lock position and contents: the dock cannot be moved, and nothing
    can be added, removed, reordered, grouped or edited in it. Opening
    programs and groups still works. Unlock it in Preferences.
  - Grip: Inherit, Shown or Hidden.

Settings that do not apply (for example Alignment for a free dock) are
shown greyed; their values are kept.

Hover effect: what the icon under the pointer does.
- None: nothing moves; the highlight shows which icon is under the
  pointer, as in earlier versions.
- Lift: the icon moves a few pixels (at most 4) towards the middle of
  the screen: up in a dock at the bottom, down at the top, right at the
  left edge, left at the right edge; a free dock lifts up (horizontal)
  or to the right (vertical). It keeps its size; its neighbours, its
  label and the highlight stay where they are. Where the bar has less
  room (labels always shown under the icons of a dock at the top) it
  moves less.
- Magnify: the icon under the pointer grows within its own place on the
  bar, and its neighbours grow a little less. Neighbours are not pushed
  aside and the dock does not change size, so the growth is small
  (about 10 pixels).
Both come in and go out within about an eighth of a second. Only icons
take part: text buttons, clocks, widgets, separators and spaces stay as
they are. While an item is dragged, or a menu, panel or settings window
of Dockbar is open, icons stay at rest. Docks without a bar show no hover
effect for now; the setting is kept for when the dock gets its bar back.
New configurations start with Lift; settings from earlier versions keep
None until you choose otherwise.

Running indicators (off unless you turn them on): a small dot under the
icon of each program in the dock that is running, whether it was started
from the dock, from Wanderer or from a Shell. In a vertical dock the dot
is beside the icon, on the side nearer the screen edge. It appears within
about two seconds of the start and goes within about four seconds of the
end. Clicking works as before.
- Only programs placed in the dock itself are marked, not those in its
  groups, and only when Dockbar can tell that it is exactly that program
  file: a copy with the same name in another drawer gets its own dot.
- Programs on FAT volumes get no dot (that file system does not let
  Dockbar tell drawers apart), and neither do commands built into the
  Shell or programs that hand over to another program and end.
- On a dock without a bar (not with real transparency), the place of each
  dot belongs to the dock even while no dot is shown: it shows the desktop
  as Dockbar last saw it, and a click there starts the program.

Text (one setting for all docks, under Defaults for all docks):
- System (the default): the screen font, drawn exactly as the system
  draws it. Docks of earlier versions look the same as before.
- Smooth: anti-aliased text from the TrueType fonts installed in the
  system (no fonts come with Dockbar). It uses the screen font's own
  file if the screen font is an outline font, otherwise Vera Sans, and
  Open Sans for any letter the first font lacks; the size matches the
  screen font's height. Letters are those of the system's code page,
  read from ENV:CHARSET: ISO-8859-1 and ISO-8859-2 are supported, so
  Polish names show correctly when ENV:CHARSET is ISO-8859-2.
- Smooth applies to: names under icons, text buttons, clocks and the
  tooltips of the docks. It does not apply to the grip menu, the group
  panel, the item editor or Preferences, which keep the screen font.
- If the font files cannot be read, Dockbar uses System and says so in
  its log.

The buttons at the bottom:
- Apply shows the changes on the docks at once, without saving them;
  Preferences stays open.
- Save saves the changes and closes Preferences. If they cannot be
  saved, Preferences stays open and says so.
- Cancel (also the close gadget or Escape) closes Preferences without
  saving. Changes shown with Apply are taken back, and the docks look
  as they did before Preferences was opened. A screen mode change while
  Preferences is open does the same.
- Help shows this ReadMe.

While Preferences is open, the docks do not take clicks or drops, and
nothing can be added to them.

Colours and backgrounds
-----------------------
Colours: Dark and Light are fixed. Automatic chooses Dark or Light from
the brightness of what is behind the dock: the desktop picture for a
see-through or bar-less dock, otherwise Dark. It is checked again when the
dock's picture of the desktop is taken (see below), not while the pointer
moves. A dock over a desktop that is half light and half dark gets one of
the two for all of it.

Background "None": the dock has no bar, frame or shadow; only
the icons, the labels, the clocks and the grip (if shown) are drawn, and
clicks between them reach the desktop or the window behind. Because there
is no bar:
- there are no highlight boxes; the name of the icon under the pointer is
  shown instead (unless labels are always shown). During a drag from
  Wanderer such a dock marks nothing;
- to add or move an icon, drop it on an icon of the dock; a drop in a
  gap goes to what is behind the dock;
- each clock has its own small plate so that it stays readable;
- a dock too long for the screen shows its bar again, with the arrows
  ("Too long to show without a bar");
- the outline of the dock is fixed when it opens. Dockbar does not change
  it while you use the dock, because AROS did not repaint the desktop
  reliably under a shrinking outline in our tests;
- some icons look different when clicked (a drawer opens). The outline
  covers both looks; where only the other look draws, the dock shows the
  desktop as it was at its last picture. A dock with such icons
  disappears for a moment when that picture is taken, like a see-through
  dock (see below).
- text buttons keep a fixed area: their plate (Plates, or under the
  pointer with Plain) appears inside it. With Plain the rest of that
  area shows the desktop picture, like the clicked look of icons above;
  a window moved behind the dock may then show through as an old
  picture until the next one (Refresh icons takes one at once). Plates
  avoid this. Clicks anywhere in a button's area go to the button.
A group still opens in its normal panel.

With the grip hidden, use Ctrl-click or start Dockbar again to reach
Preferences, for example to unlock a dock.

See-through backgrounds
-----------------------
On mainline AROS (the x86_64-v1 package) with a 32-bit screen whose
display composes windows, see-through docks are truly transparent: the
desktop and any window behind a dock show through it as they change, and
the dock never disappears to take a picture. Icons and text stay fully
opaque, and the rounded corners and the edges of docks without a bar are
smooth. Frosted glass needs a picture of the desktop, so it cannot be
changed for such a dock (Preferences says so). Automatic colours look at
the desktop behind the dock when it opens, after it is moved and after a
new wallpaper. The rest of this section applies to the other systems.

Everywhere else (ABIv11, older mainline, screens that do not compose), a
see-through dock shows a picture of the desktop behind it, not a live
view. The picture is taken again by itself when the dock opens or is
moved, after a screen mode change, and when the Wanderer wallpaper
changes. For a moment the dock disappears while it does so.

After a screen mode change, or a new wallpaper, the desktop picture can
appear some seconds later, especially on slower graphics cards. Dockbar
watches the desktop around each see-through dock for about half a minute
after such a change and takes a new picture once it has stopped
changing. Until then a dock may show the plain grey desktop. If the
desktop cannot be read at all, the dock shows its normal background.

Windows that open, close or move behind a dock later are not shown. If a
picture is still wrong, choose Refresh icons in the grip menu.

Reporting a background problem: select the Dockbar icon, choose
Information in Wanderer's Icon menu and add the Tool Type
  LOG=RAM:dockbar.log
then Save, start Dockbar and repeat what went wrong (for example change
the screen mode or the wallpaper) and wait half a minute. Copy
RAM:dockbar.log somewhere safe before restarting, because RAM: is
emptied. Please send the log, a screenshot, your graphics card and AROS
version, and say whether the dock visibly flickered or disappeared.
Remove the Tool Type afterwards: the log keeps growing while Dockbar
runs. Results on real hardware are welcome; this release was tested
only under QEMU.

Screen mode changes
-------------------
Dockbar lets Workbench change its screen mode: it closes its docks while
the screen closes and opens them again on the new screen. Anything open
at that moment closes too (menus, group panels, the item editor and the
Preferences window); unsaved changes in an open Preferences window or
item editor are lost.

Files
-----
docks.config   all docks, their contents, placement and appearance
Written next to the program when you change something.

Which version reads which docks.config:
- This version reads the files of 0.2, 0.3, 0.4-test1 to 0.4-test3,
  0.5-test1 and its own, and keeps their look: 0.2 and 0.3 docks show
  icons as before, 0.4-test1 docks keep icon and text side by side.
- 0.5-test1 writes the same format as this version, so the two can be
  swapped freely.
- Its first save writes its own format. Earlier versions cannot read
  that format: they show their default dock, refuse changes
  ("Editing unavailable") and leave the file untouched.
- So keep a copy of docks.config if you may want to go back to an
  older version.

Updating from 0.1: on the first start Dockbar reads 0.1's dock.config and
dock.prefs and makes one dock from them. It never changes those two files,
so 0.1 still finds its own settings.

A new installation starts with one dock: Shell, MultiView, Prefs, Tools
and a clock, with automatic colours.

To start again with the default dock, quit Dockbar and delete
docks.config (and, if they are there, dock.config and dock.prefs).

Known limitations of this test release
--------------------------------------
- Up to 8 docks. Groups cannot contain other groups; up to 8 groups per
  dock, 24 icons in a dock and in each group.
- The dock does not show running programs.
- Where transparency is a picture of the desktop (see above), rounded
  corners have small steps and desktop icons behind a dock may be missing
  from the picture.
- On mainline AROS with gfx.hidd older than 45.32 (sources up to 26
  October 2026), a see-through dock can blink for a moment. With
  gfx.hidd older than 45.31 it blinks when the pointer moves over it or
  something behind it changes; with 45.31 when the whole display is
  redrawn, for example when a screen behind the Workbench screen moves.
  The cause is in the system's display composition, which is fixed from
  gfx.hidd 45.32. When memory is short the system composes the old way,
  and the blinking can come back.
- The wallpaper is followed only for Wanderer (its prefs file
  ENV:SYS/Wanderer/global.prefs).
- Automatic colours and docks without a bar look at the desktop behind
  them; a window opened or closed there later is noticed only at the next
  picture (Refresh icons takes one at once).
- Docks without a bar: see "Colours and backgrounds" for what is
  different.
- On a screen smaller than the dock was made for, icons get smaller and
  the dock scrolls.
- Settings in RAM: are lost at restart.
@TESTED@
@SVGNOTE@

Licence
-------
The licence of Dockbar itself is not decided yet (MIT is proposed). This is
a test build for evaluation. NOTICES lists the parts it contains and their
origin.
