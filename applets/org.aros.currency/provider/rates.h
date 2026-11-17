/* SPDX-License-Identifier: MIT */
/* The European Central Bank's daily reference rates, from its XML file
 * (https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml):
 *
 *   <Cube time='2026-11-16'>
 *     <Cube currency='USD' rate='1.0845'/> ...
 *
 * Each rate is the amount of that currency one euro buys. Pure. */
#ifndef CURRENCY_RATES_H
#define CURRENCY_RATES_H

#include <stddef.h>

#define CURRENCY_MAX 60

struct currency_rates {
    char date[12];                  /* YYYY-MM-DD */
    int n;
    char code[CURRENCY_MAX][4];
    double rate[CURRENCY_MAX];
};

/* The bank's history file (eurofxref-hist-90d.xml) has the same form with
 * one <Cube time=...> per day, newest first. Fills up to max days in the
 * file's order; the number of days that have a date and a rate. */
int currency_parse_ecb_days(const char *xml, size_t n, struct currency_rates *days, int max);

/* 1 when the text holds a date and at least one rate. */
int currency_parse_ecb(const char *xml, size_t n, struct currency_rates *out);

#endif
