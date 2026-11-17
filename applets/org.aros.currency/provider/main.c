/* SPDX-License-Identifier: MIT */
/* CurrencyProvider: answers one request of the widget protocol.
 *
 *   CurrencyProvider "<request file>"
 *
 * Operation "rates": fetches the European Central Bank's reference rates of
 * the last days and answers with
 *   - the user's currency: the setting "currency", else the one of the
 *     system's locale, else EUR in Europe's time zones and USD elsewhere;
 *   - rows of the section "top": five well-known currencies other than the
 *     user's, each with the price of one unit in the user's currency
 *     (value), the change since the previous day of rates (detail, as a
 *     percentage) and that change as the row's plot number;
 *   - rows of the section "all": every currency of the bank (label: code,
 *     value: how much of it one euro buys), for a host that converts;
 *   - fields: "date", "base", "own", "heading", "basis", and for the first
 *     row of "top" "pair", "rate", "tile" ("EUR 4.24") and "summary", for a
 *     host that shows one number. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rates.h"
#include "http.h"
#include "applets/json.h"
#include "applets/snapshot.h"

#ifdef __AROS__
#include <dos/dos.h>
#include <libraries/locale.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/locale.h>
#endif

/* about 60 days of rates, newest first; the first two are used */
#define ECB_URL "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-hist-90d.xml"

static int str(const struct DockJson *j, int o, const char *k, char *v, size_t cap)
{
    return dock_json_string(j, dock_json_get(j, o, k), v, cap);
}

/* The bundled certificates are <package>/certs/ca-bundle.crt; the host
 * names the package directory in the request. */
static void find_package_ca(const struct DockJson *j)
{
    char root[512], path[512];
    FILE *f;
    int byte;
    weather_http_package_ca[0] = 0;
    if (!str(j, 0, "packageRoot", root, sizeof root) || !*root) return;
#ifdef __AROS__
    snprintf(path, sizeof path, "%s", root);
    if (!AddPart((STRPTR)path, (CONST_STRPTR)"certs/ca-bundle.crt", sizeof path)) return;
#else
    if (snprintf(path, sizeof path, "%s/certs/ca-bundle.crt", root) >= (int)sizeof path) return;
#endif
    f = fopen(path, "rb");
    byte = f ? fgetc(f) : EOF;
    if (f) fclose(f);
    if (byte != EOF) snprintf(weather_http_package_ca, sizeof weather_http_package_ca, "%s", path);
}

static double rate_of(const struct currency_rates *r, const char *code)
{
    int i;
    if (!strcmp(code, "EUR")) return 1;
    for (i = 0; i < r->n; i++)
        if (!strcmp(r->code[i], code)) return r->rate[i];
    return 0;
}

/* Three letters, upper case, from text; 0 when it is not a currency code. */
static int code_from(const char *text, char out[4])
{
    int i;
    while (*text == ' ') text++;
    for (i = 0; i < 3; i++) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c < 'A' || c > 'Z') return 0;
        out[i] = c;
    }
    out[3] = 0;
    for (text += 3; *text; text++) if (*text != ' ') return 0;
    return 1;
}

/* The user's currency. From the setting when it names one the bank lists
 * (its default, "auto", names none);
 * else from the system's locale (its international currency symbol); else
 * EUR where the clock is zero to three hours east of Greenwich, USD
 * elsewhere. */
static void own_currency(const struct DockJson *j, const struct currency_rates *rates, char out[4])
{
    char text[16];
    int settings = dock_json_get(j, 0, "settings"), europe = 0;
    if (settings >= 0 && dock_json_string(j, dock_json_get(j, settings, "currency"), text, sizeof text) &&
        code_from(text, out) && rate_of(rates, out) > 0) return;
#ifdef __AROS__
    {
        struct Library *LocaleBase = OpenLibrary((CONST_STRPTR)"locale.library", 38);
        if (LocaleBase) {
            struct Locale *locale = OpenLocale(NULL);
            int found = 0;
            if (locale) {
                found = locale->loc_MonIntCS && code_from((const char *)locale->loc_MonIntCS, out) && rate_of(rates, out) > 0;
                europe = locale->loc_GMTOffset <= 0 && locale->loc_GMTOffset >= -180;
                CloseLocale(locale);
            }
            CloseLibrary(LocaleBase);
            if (found) return;
        }
    }
#endif
    strcpy(out, europe ? "EUR" : "USD");
}

/* A price for people: two decimals from 100 up, four from 1, five below. */
static void price(double v, char *out, size_t cap)
{
    snprintf(out, cap, v >= 100 ? "%.2f" : v >= 1 ? "%.4f" : "%.5f", v);
}

static void answer_rates(const struct DockJson *j, const struct currency_rates *days, int day_count, struct DockAppletSnapshot *reply)
{
    static const char *const top[] = { "EUR", "USD", "GBP", "CHF", "JPY", "CAD" };
    const struct currency_rates *now = &days[0], *before = day_count > 1 ? &days[1] : NULL;
    char own[4], shown[32];
    double mine, mine_before;
    int i, n = 0, f = 0;

    own_currency(j, now, own);
    mine = rate_of(now, own);
    mine_before = before ? rate_of(before, own) : 0;

    strcpy(reply->fields[f].key, "date");
    snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "%s", now->date);
    strcpy(reply->fields[f].key, "base");
    strcpy(reply->fields[f++].text, "EUR");
    strcpy(reply->fields[f].key, "own");
    strcpy(reply->fields[f++].text, own);
    strcpy(reply->fields[f].key, "heading");
    snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "Exchange rates in %s", own);
    strcpy(reply->fields[f].key, "basis");
    if (before) snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "Reference rates of %s, change since %s", now->date, before->date);
    else snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "Reference rates of %s", now->date);

    for (i = 0; i < (int)(sizeof top / sizeof *top) && n < 5; i++) {
        struct DockAppletRow *row = &reply->rows[n];
        double theirs = rate_of(now, top[i]), theirs_before = before ? rate_of(before, top[i]) : 0, v;
        if (!strcmp(top[i], own) || theirs <= 0) continue;
        v = mine / theirs;
        snprintf(row->label, sizeof row->label, "%s", top[i]);
        price(v, shown, sizeof shown);
        snprintf(row->value, sizeof row->value, "%s", shown);
        strcpy(row->section, "top");
        if (mine_before > 0 && theirs_before > 0) {
            double change = (v / (mine_before / theirs_before) - 1) * 100;
            if (change > -0.005 && change < 0.005) change = 0;
            snprintf(row->detail, sizeof row->detail, "%s%.2f%%", change > 0 ? "+" : "", change);
            row->plot = change;
            row->has_plot = 1;
        }
        if (!n) {
            strcpy(reply->fields[f].key, "pair");
            snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "%s/%s", top[i], own);
            /* as text, four significant digits: short enough for a dock tile */
            strcpy(reply->fields[f].key, "rate");
            snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "%.4g", v);
            /* a text-only tile: the currency and its price */
            strcpy(reply->fields[f].key, "tile");
            snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, v >= 1000 ? "%s %.0f" : v >= 100 ? "%s %.1f" : v >= 10 ? "%s %.2f" : v >= 1 ? "%s %.3f" : "%s %.4f", top[i], v);
            strcpy(reply->fields[f].key, "summary");
            snprintf(reply->fields[f++].text, sizeof reply->fields[0].text, "1 %s = %s %s", top[i], shown, own);
        }
        n++;
    }
    reply->field_count = f;
    for (i = 0; i < now->n && n < DA_ROWS; i++) {
        snprintf(reply->rows[n].label, sizeof reply->rows[n].label, "%s", now->code[i]);
        snprintf(reply->rows[n].value, sizeof reply->rows[n].value, "%.6g", now->rate[i]);
        strcpy(reply->rows[n].section, "all");
        n++;
    }
    reply->row_count = n;
}

int main(int argc, char **argv)
{
    size_t length;
    char *input, *body = NULL;
    struct DockJson *j = malloc(sizeof *j);
    struct DockAppletSnapshot *reply = calloc(1, sizeof *reply);
    struct currency_rates *rates = malloc(2 * sizeof *rates);
    char operation[32], temporary[DA_PATH], completed[DA_PATH];
    const char *why = NULL;
    size_t size = 0;
    int result = 5, g = 0, api = 0, a, b, ok, days;
    FILE *f;

    if (argc != 2) return 5;
    input = dock_applet_read(argv[1], 65536, &length);
    if (!input) why = "cannot read the request file";
    else if (!j || !reply || !rates) why = "out of memory";
    else if (!dock_json_parse(j, input, length)) why = "request is not valid JSON";
    else if (!dock_json_int(j, dock_json_get(j, 0, "api"), &api) || api != 1) why = "unsupported api";
    else if (!str(j, 0, "package", reply->package, sizeof reply->package) || strcmp(reply->package, "org.aros.currency")) why = "wrong package";
    else if (!str(j, 0, "instance", reply->instance, sizeof reply->instance) || !dock_applet_id_valid(reply->instance)) why = "bad instance";
    else if (!str(j, 0, "session", reply->session, sizeof reply->session) || !dock_applet_id_valid(reply->session)) why = "bad session";
    else if (!dock_json_int(j, dock_json_get(j, 0, "generation"), &g) || g < 1) why = "bad generation";
    if (why) { fprintf(stderr, "CurrencyProvider: %s\n", why); goto done; }

    find_package_ca(j);
    snprintf(weather_http_user_agent, sizeof weather_http_user_agent, "AROS-Currency/0.2.0");
    reply->generation = (unsigned long)g;
    reply->status = DA_ERROR;
    reply->updated = reply->source_time = (double)time(NULL);
    strcpy(reply->title, "Exchange rates");
    if (!str(j, 0, "operation", operation, sizeof operation)) {
        strcpy(reply->message, "Missing operation");
    } else if (strcmp(operation, "rates")) {
        strcpy(reply->message, "Unsupported operation");
    } else if (weather_http_get(ECB_URL, 512u * 1024u, &body, &size, reply->message, sizeof reply->message)) {
        if ((days = currency_parse_ecb_days(body, size, rates, 2)) > 0) {
            reply->status = DA_READY;
            strcpy(reply->attribution, "European Central Bank");
            strcpy(reply->url, "https://www.ecb.europa.eu/stats/policy_and_exchange_rates/euro_reference_exchange_rates/");
            answer_rates(j, rates, days, reply);
        } else {
            strcpy(reply->message, "The bank's file has no rates in the expected form");
        }
    }
    free(body);

    a = snprintf(temporary, sizeof temporary, "%s.tmp", argv[1]);
    b = snprintf(completed, sizeof completed, "%s.done", argv[1]);
    if (a < 0 || b < 0 || (size_t)a >= sizeof temporary || (size_t)b >= sizeof completed) {
        fprintf(stderr, "CurrencyProvider: request path too long\n");
        goto done;
    }
    /* the host gives each request a destination that does not exist yet;
       an answer never replaces a file */
    if ((f = fopen(completed, "rb"))) { fclose(f); goto done; }
    if (!(f = fopen(temporary, "wb"))) { fprintf(stderr, "CurrencyProvider: cannot write the reply\n"); goto done; }
    ok = dock_applet_snapshot_write(f, reply);
    if (fclose(f)) ok = 0;
    if (ok && !rename(temporary, completed)) result = 0;
    else { fprintf(stderr, "CurrencyProvider: cannot publish the reply\n"); remove(temporary); }
done:
    free(input); free(j); free(reply); free(rates);
    return result;
}
