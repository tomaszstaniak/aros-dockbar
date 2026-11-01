#ifndef DOCK_COMMAND_H
#define DOCK_COMMAND_H
#include <stddef.h>

/* Pure helpers for starting a command item (no AROS calls). */

/* Console window spec "CON:x/y/w/h/TITLE/AUTO/CLOSE/WAIT" for a command
 * named label. The title is cleaned first: a slash would end it and turn
 * the rest into options, control characters are dropped; empty -> "Command".
 * 0 when it does not fit. */
int dock_command_console(const char *label, char *out, size_t capacity);

/* The Shell line that runs a script file: Execute "PATH", with the path
 * quoted the AmigaDOS way (" as *", * as **, newline as *N). A relative
 * path stays relative: it is resolved in the command's working directory,
 * which is also where the caller checks it. 0 when it does not fit or the
 * path is empty. */
int dock_command_script_line(const char *path, char *out, size_t capacity);

/* COMMAND "PATH" with the same quoting, for any command taking one file. */
int dock_command_quoted(const char *command, const char *path, char *out, size_t capacity);

/* Where dropped files go in a command line: one {files}, as a word of its
 * own (start of line or blank before, end or blank after) and outside any
 * quotes; Dockbar quotes each file itself. 1 valid, 0 none (the command
 * takes no files), -1 inside quotes, glued to other text, or more than once. */
int dock_command_files_token(const char *command);
/* Files dropped on a command: the {files} word becomes the files, each
 * quoted as above and separated by spaces. 1 done, -1 no {files} (nothing
 * is passed), -2 {files} not valid (see above), 0 no files or it does not fit. */
int dock_command_with_files(const char *command, const char *const *files, int count,
                            char *out, size_t capacity);
/* Files dropped on a script: Execute "SCRIPT" FILES "*"FILE*"" ..., for a
 * script that starts with .KEY FILES/M and writes <FILES> without quotes.
 * Each name reaches the script as one quoted argument, so <FILES> expands to
 * the names exactly, spaces, quotes and asterisks included. Two traps of the
 * AROS Shell decide this form (both measured, ABIv11 2026.09):
 * - .KEY substitutes values as they are, without quotes, so a name with a
 *   space would split; hence each value carries its own quotes;
 * - Execute reads its arguments with /F, which unescapes the first word when
 *   it is quoted (readargs.c), so that one would lose its quotes; hence the
 *   unquoted keyword FILES first.
 * 1 done, 0 no files or it does not fit. */
int dock_command_script_files(const char *script, const char *const *files, int count,
                              char *out, size_t capacity);
/* 1 when a script's text takes dropped files: its first command line is
 * .KEY (or .K) with an argument FILES (or an alias of it) marked /M.
 * Any other script would get the word FILES as one of its arguments. */
int dock_command_script_takes_files(const char *text);
/* 0 when the AROS Shell would misread LINE: it decides where comments (;)
 * and pipes (|) start by flipping a quote flag at every ", the escaped *"
 * included, before the parser that honours escapes (shell/readLine.c). A
 * ; or | inside a real quoted argument, after an escaped quote, then ends
 * the command or splits it. 1 when every ; and | is seen as it is meant. */
int dock_command_line_safe(const char *line);

/* A program started as a Shell command, with or without dropped files:
 * "PROGRAM" "FILE" ..., all quoted and escaped. 1 done, 0 it does not fit. */
int dock_command_program_files(const char *program, const char *const *files, int count,
                               char *out, size_t capacity);

#endif
