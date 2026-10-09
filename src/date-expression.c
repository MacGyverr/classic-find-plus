#include "input-limits.h"
#include "date-expression.h"
#include <gio/gio.h>
#include <string.h>

static const char *const bases[] = { NULL, "Today", "Yesterday", "Tomorrow", "StartOfWeek", "StartOfMonth", "StartOfYear" };
static const char *const times[] = { NULL, "Now", "Midnight", "Noon" };

static gboolean invalid(GError **error)
{
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
        "Invalid date/time expression. Use YYYY/MM/DD or a relative date, signed adjustments, and optional @ HH:mm:ss.");
    return FALSE;
}

static gboolean digits(const char *text, guint count)
{
    for (guint i = 0; i < count; i++) if (!g_ascii_isdigit(text[i])) return FALSE;
    return TRUE;
}

static int decimal(const char *text, guint count)
{
    int value = 0;
    for (guint i = 0; i < count; i++) value = value * 10 + text[i] - '0';
    return value;
}

gboolean cf_date_expression_parse(const char *text, CfDateExpression *out, GError **error)
{
    CfDateExpression e = { 0 };
    if (!cf_input_valid(text, CF_DATE_LIMIT) || !*text) return invalid(error);
    g_auto(GStrv) words = g_strsplit(text, " ", -1);
    const char *base = words[0];
    for (guint i = 1; i < G_N_ELEMENTS(bases); i++) if (!strcmp(base, bases[i])) e.base = i;
    if (e.base == CF_DATE_FIXED) {
        if (strlen(base) != 10 || (base[4] != '/' && base[4] != '-') || base[7] != base[4] ||
            !digits(base, 4) || !digits(base + 5, 2) || !digits(base + 8, 2)) return invalid(error);
        e.year = decimal(base, 4); e.month = decimal(base + 5, 2); e.day = decimal(base + 8, 2);
        if (!g_date_valid_dmy(e.day, e.month, e.year)) return invalid(error);
    }
    for (guint i = 1; words[i]; i++) {
        if (!strcmp(words[i], "@")) {
            if (e.specify_time || !words[++i]) return invalid(error);
            e.specify_time = TRUE;
            for (guint n = 1; n < G_N_ELEMENTS(times); n++) if (!strcmp(words[i], times[n])) e.time_base = n;
            if (e.time_base == CF_TIME_FIXED) {
                const char *t = words[i];
                if (strlen(t) != 8 || t[2] != ':' || t[5] != ':' ||
                    !digits(t, 2) || !digits(t + 3, 2) || !digits(t + 6, 2)) return invalid(error);
                e.hour = decimal(t, 2); e.minute = decimal(t + 3, 2); e.second = decimal(t + 6, 2);
                if (e.hour > 23 || e.minute > 59 || e.second > 59) return invalid(error);
            }
            continue;
        }
        const char *value = words[i];
        if ((*value != '+' && *value != '-') || !value[1]) return invalid(error);
        for (const char *p = value + 1; *p; p++) if (!g_ascii_isdigit(*p)) return invalid(error);
        char *end = NULL;
        gint64 parsed = g_ascii_strtoll(value, &end, 10);
        if (*end || parsed < -1000000 || parsed > 1000000 || !words[++i]) return invalid(error);
        const char *unit = words[i];
        int *target = NULL;
        if (!e.specify_time) {
            if (!strcmp(unit, "days")) target = &e.days;
            else if (!strcmp(unit, "weeks")) target = &e.weeks;
            else if (!strcmp(unit, "months")) target = &e.months;
            else if (!strcmp(unit, "years")) target = &e.years;
            e.adjust_date = TRUE;
        } else {
            if (!strcmp(unit, "seconds")) target = &e.seconds;
            else if (!strcmp(unit, "minutes")) target = &e.minutes;
            else if (!strcmp(unit, "hours")) target = &e.hours;
            e.adjust_time = TRUE;
        }
        if (!target || *target) return invalid(error);
        *target = parsed;
    }
    *out = e;
    return TRUE;
}

char *cf_date_expression_format(const CfDateExpression *e)
{
    GString *s = g_string_new(NULL);
    if (e->base == CF_DATE_FIXED) g_string_append_printf(s, "%04d/%02d/%02d", e->year, e->month, e->day);
    else g_string_append(s, bases[e->base]);
    if (e->adjust_date) g_string_append_printf(s, " %+d years %+d months %+d weeks %+d days", e->years, e->months, e->weeks, e->days);
    if (e->specify_time) {
        g_string_append(s, " @ ");
        if (e->time_base == CF_TIME_FIXED) g_string_append_printf(s, "%02d:%02d:%02d", e->hour, e->minute, e->second);
        else g_string_append(s, times[e->time_base]);
        if (e->adjust_time) g_string_append_printf(s, " %+d hours %+d minutes %+d seconds", e->hours, e->minutes, e->seconds);
    }
    return g_string_free(s, FALSE);
}

GDateTime *cf_date_expression_evaluate(const CfDateExpression *e, GDateTime *now, GError **error)
{
    g_autoptr(GDateTime) clock = now ? g_date_time_ref(now) : g_date_time_new_now_local();
    int year = e->base == CF_DATE_FIXED ? e->year : g_date_time_get_year(clock);
    int month = e->base == CF_DATE_FIXED ? e->month : e->base == CF_DATE_YEAR_START ? 1 : g_date_time_get_month(clock);
    int day = e->base == CF_DATE_FIXED ? e->day :
              e->base == CF_DATE_MONTH_START || e->base == CF_DATE_YEAR_START ? 1 : g_date_time_get_day_of_month(clock);
    GDateTime *date = g_date_time_new(g_date_time_get_timezone(clock), year, month, day, 0, 0, 0);
    if (!date) { invalid(error); return NULL; }
    int delta = e->base == CF_DATE_YESTERDAY ? -1 : e->base == CF_DATE_TOMORROW ? 1 :
                e->base == CF_DATE_WEEK_START ? 1 - g_date_time_get_day_of_week(date) : 0;
    GDateTime *next = g_date_time_add_days(date, delta);
    g_date_time_unref(date); date = next;
    if (e->adjust_date && date) {
        next = g_date_time_add_years(date, e->years); g_date_time_unref(date); date = next;
        if (date) { next = g_date_time_add_months(date, e->months); g_date_time_unref(date); date = next; }
        if (date) { next = g_date_time_add_weeks(date, e->weeks); g_date_time_unref(date); date = next; }
        if (date) { next = g_date_time_add_days(date, e->days); g_date_time_unref(date); date = next; }
    }
    if (date && e->specify_time) {
        int hour = e->time_base == CF_TIME_NOW ? g_date_time_get_hour(clock) :
                   e->time_base == CF_TIME_NOON ? 12 : e->time_base == CF_TIME_MIDNIGHT ? 0 : e->hour;
        int minute = e->time_base == CF_TIME_NOW ? g_date_time_get_minute(clock) : e->time_base == CF_TIME_FIXED ? e->minute : 0;
        double second = e->time_base == CF_TIME_NOW ? g_date_time_get_seconds(clock) : e->time_base == CF_TIME_FIXED ? e->second : 0;
        next = g_date_time_new(g_date_time_get_timezone(clock), g_date_time_get_year(date),
            g_date_time_get_month(date), g_date_time_get_day_of_month(date), hour, minute, second);
        g_date_time_unref(date); date = next;
        /* Reject DST gaps rather than silently advancing the selected time. */
        if (date && (g_date_time_get_hour(date) != hour || g_date_time_get_minute(date) != minute)) {
            g_date_time_unref(date); date = NULL;
        }
        if (date && e->adjust_time) {
            next = g_date_time_add_seconds(date, (double)e->hours * 3600 + (double)e->minutes * 60 + e->seconds);
            g_date_time_unref(date); date = next;
        }
    }
    if (!date) invalid(error);
    return date;
}

GDateTime *cf_date_expression_resolve(const char *text, GDateTime *now, gboolean *has_time, GError **error)
{
    CfDateExpression e;
    if (!cf_date_expression_parse(text, &e, error)) return NULL;
    if (has_time) *has_time = e.specify_time;
    return cf_date_expression_evaluate(&e, now, error);
}
