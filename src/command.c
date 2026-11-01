#include "command.h"
#include <stdio.h>
#include <string.h>

int dock_command_console(const char *label, char *out, size_t capacity)
{
    char title[80];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)(label ? label : ""); *p && n + 1 < sizeof title; p++) {
        if (*p < 32 || *p == 127) continue;
        title[n++] = *p == '/' ? '-' : (char)*p;
    }
    title[n] = 0;
    if (!n) strcpy(title, "Command");
    int written = snprintf(out, capacity, "CON:40/40/640/200/%s/AUTO/CLOSE/WAIT", title);
    return written > 0 && (size_t)written < capacity;
}

int dock_command_quoted(const char *command, const char *path, char *out, size_t capacity)
{
    size_t n;
    if (!command || !path || !*path || capacity < strlen(command) + 5) return 0;
    n = (size_t)snprintf(out, capacity, "%s \"", command);
    for (const char *p = path; *p; p++) {
        const char *rep = *p == '"' ? "*\"" : *p == '*' ? "**" : *p == '\n' ? "*N" : NULL;
        size_t len = rep ? 2 : 1;
        if (n + len + 2 > capacity) return 0;
        if (rep) { memcpy(out + n, rep, 2); n += 2; }
        else out[n++] = *p;
    }
    out[n++] = '"';
    out[n] = 0;
    return 1;
}

int dock_command_script_line(const char *path, char *out, size_t capacity)
{
    return dock_command_quoted("Execute", path, out, capacity);
}

/* Appends " \"PATH\"" quoted the AmigaDOS way; 0 when it does not fit. */
static int append_quoted(char *out, size_t *n, size_t capacity, const char *path)
{
    if (*n + 3 >= capacity) return 0;
    out[(*n)++] = ' ';
    out[(*n)++] = '"';
    for (const char *p = path; *p; p++) {
        const char *rep = *p == '"' ? "*\"" : *p == '*' ? "**" : *p == '\n' ? "*N" : NULL;
        size_t len = rep ? 2 : 1;
        if (*n + len + 2 > capacity) return 0;
        if (rep) { memcpy(out + *n, rep, 2); *n += 2; }
        else out[(*n)++] = *p;
    }
    out[(*n)++] = '"';
    out[*n] = 0;
    return 1;
}

int dock_command_files_token(const char *command)
{
    static const char token[] = "{files}";
    const size_t length = sizeof token - 1;
    int quoted = 0, found = 0;
    if (!command) return 0;
    for (const char *p = command; *p; p++) {
        if (quoted && *p == '*' && p[1]) { p++; continue; }  /* *" and ** inside quotes */
        if (*p == '"') { quoted = !quoted; continue; }
        if (strncmp(p, token, length)) continue;
        int alone = (p == command || p[-1] == ' ' || p[-1] == '\t') &&
                    (!p[length] || p[length] == ' ' || p[length] == '\t');
        if (quoted || !alone || found) return -1;
        found = 1;
        p += length - 1;
    }
    return found;
}

int dock_command_with_files(const char *command, const char *const *files, int count,
                            char *out, size_t capacity)
{
    static const char token[] = "{files}";
    size_t n = 0;
    int valid = dock_command_files_token(command);
    if (valid == 0) return -1;
    if (valid < 0) return -2;
    if (!files || count < 1 || capacity < 1) return 0;
    out[0] = 0;
    const char *at = strstr(command, token);
    /* Outside quotes the first occurrence is the valid one: an earlier
     * "{files}" inside quotes would have made the line invalid. */
    for (const char *p = command; *p;) {
        if (p == at) {
            for (int i = 0; i < count; i++) {
                size_t before = n;
                if (!append_quoted(out, &n, capacity, files[i])) return 0;
                if (i == 0) {
                    /* The first file takes the token's place, without a
                     * leading space of its own. */
                    memmove(out + before, out + before + 1, n - before);
                    n--;
                }
            }
            p += sizeof token - 1;
        } else {
            if (n + 2 > capacity) return 0;
            out[n++] = *p++;
            out[n] = 0;
        }
    }
    return 1;
}

int dock_command_script_files(const char *script, const char *const *files, int count,
                              char *out, size_t capacity)
{
    char inner[4200];
    size_t n, m;
    if (!files || count < 1 || !dock_command_script_line(script, out, capacity)) return 0;
    n = strlen(out);
    if (n + 7 >= capacity) return 0;
    memcpy(out + n, " FILES", 7);
    n += 6;
    for (int i = 0; i < count; i++) {
        /* The value itself is "FILE", quoted and escaped; quoted once more
         * for Execute's own line. */
        m = 0;
        if (!append_quoted(inner, &m, sizeof inner, files[i])) return 0;
        if (!append_quoted(out, &n, capacity, inner + 1)) return 0;
    }
    return 1;
}

static int fold(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int word_is(const char *p, size_t length, const char *word)
{
    if (strlen(word) != length) return 0;
    for (size_t i = 0; i < length; i++) if (fold((unsigned char)p[i]) != word[i]) return 0;
    return 1;
}

int dock_command_script_takes_files(const char *text)
{
    const char *line = text;
    if (!text) return 0;
    /* Directives come first; blank lines and ; comments before them too. */
    while (*line) {
        const char *end = line, *p = line;
        while (*end && *end != '\n') end++;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p == end || *p == ';') { line = *end ? end + 1 : end; continue; }
        if (*p != '.') return 0;
        const char *word = p + 1, *w = word;
        while (w < end && *w != ' ' && *w != '\t') w++;
        if (!word_is(word, (size_t)(w - word), "key") && !word_is(word, (size_t)(w - word), "k")) {
            line = *end ? end + 1 : end;  /* .BRA, .KET, .DEF and the like */
            continue;
        }
        /* Template: comma-separated items NAME[=ALIAS...][/MOD...]. */
        for (const char *item = w; item < end;) {
            while (item < end && (*item == ' ' || *item == '\t' || *item == ',')) item++;
            const char *stop = item;
            while (stop < end && *stop != ',') stop++;
            int named = 0, multiple = 0;
            for (const char *q = item; q < stop;) {
                const char *r = q;
                while (r < stop && *r != '=' && *r != '/') r++;
                const char *e = r;
                while (e > q && (e[-1] == ' ' || e[-1] == '\t')) e--;
                if (word_is(q, (size_t)(e - q), "files")) named = 1;
                if (r < stop && *r == '/') {
                    for (q = r; q < stop && *q == '/'; q += 2)
                        if (q + 1 < stop && fold((unsigned char)q[1]) == 'm') multiple = 1;
                    break;
                }
                q = r < stop ? r + 1 : r;
            }
            if (named && multiple) return 1;
            item = stop < end ? stop + 1 : stop;
        }
        return 0;
    }
    return 0;
}

int dock_command_line_safe(const char *line)
{
    int naive = 0, real = 0;
    if (!line) return 1;
    for (const char *p = line; *p; p++) {
        if (*p == '"') naive = !naive;
        if ((*p == ';' || *p == '|') && naive != real) return 0;
        if (real && *p == '*' && p[1]) {
            /* An escape: the Shell's first pass still flips at *". */
            if (p[1] == '"') naive = !naive;
            p++;
            continue;
        }
        if (*p == '"') real = !real;
    }
    return 1;
}

int dock_command_program_files(const char *program, const char *const *files, int count,
                               char *out, size_t capacity)
{
    size_t n = 0;
    if (!program || !*program || count < 0 || (count && !files) || capacity < 1) return 0;
    out[0] = 0;
    if (!append_quoted(out, &n, capacity, program)) return 0;
    memmove(out, out + 1, n);  /* no leading space before the program */
    n--;
    for (int i = 0; i < count; i++)
        if (!append_quoted(out, &n, capacity, files[i])) return 0;
    return 1;
}
