/* SPDX-License-Identifier: MIT */
#include "rates.h"

#include <stdlib.h>
#include <string.h>

/* The value of attribute name ('...' or "...") in the tag starting at tag
 * and ending before end; 0 when it is not there or does not fit. */
static int attribute(const char *tag, const char *end, const char *name, char *out, size_t cap)
{
    size_t nn = strlen(name);
    const char *p;
    for (p = tag; p + nn + 2 < end; p++) {
        char quote;
        const char *v, *e;
        if (memcmp(p, name, nn) || p[nn] != '=' || (p > tag && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '\n')) continue;
        quote = p[nn + 1];
        if (quote != '\'' && quote != '"') continue;
        v = p + nn + 2;
        for (e = v; e < end && *e != quote; e++) ;
        if (e >= end || (size_t)(e - v) + 1 > cap) return 0;
        memcpy(out, v, (size_t)(e - v));
        out[e - v] = 0;
        return 1;
    }
    return 0;
}

int currency_parse_ecb_days(const char *xml, size_t n, struct currency_rates *days, int max)
{
    const char *p = xml, *end = xml + n;
    struct currency_rates *out = NULL;
    int count = 0;
    if (max < 1) return 0;
    memset(days, 0, sizeof *days * (size_t)max);
    while (p < end) {
        const char *tag = memchr(p, '<', (size_t)(end - p)), *close;
        char code[8], rate[32], date[16];
        if (!tag) break;
        close = memchr(tag, '>', (size_t)(end - tag));
        if (!close) break;
        if (close - tag > 5 && !memcmp(tag, "<Cube", 5)) {
            if (attribute(tag, close, "time", date, sizeof date) && strlen(date) == 10) {
                if (count == max) break;
                out = &days[count++];
                memcpy(out->date, date, 11);
            }
            if (out && attribute(tag, close, "currency", code, sizeof code) && strlen(code) == 3 &&
                attribute(tag, close, "rate", rate, sizeof rate) && out->n < CURRENCY_MAX) {
                char *stop;
                double v = strtod(rate, &stop);
                if (stop != rate && !*stop && v > 0 && v < 1e9) {
                    memcpy(out->code[out->n], code, 4);
                    out->rate[out->n] = v;
                    out->n++;
                }
            }
        }
        p = close + 1;
    }
    /* a day without rates ends the usable part */
    for (n = 0; (int)n < count; n++)
        if (!days[n].n) { count = (int)n; break; }
    return count;
}

int currency_parse_ecb(const char *xml, size_t n, struct currency_rates *out)
{
    return currency_parse_ecb_days(xml, n, out, 1) == 1;
}
