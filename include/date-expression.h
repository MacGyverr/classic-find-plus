#pragma once
#include <glib.h>

typedef enum { CF_DATE_FIXED, CF_DATE_TODAY, CF_DATE_YESTERDAY, CF_DATE_TOMORROW,
               CF_DATE_WEEK_START, CF_DATE_MONTH_START, CF_DATE_YEAR_START } CfDateBase;
typedef enum { CF_TIME_FIXED, CF_TIME_NOW, CF_TIME_MIDNIGHT, CF_TIME_NOON } CfTimeBase;
typedef struct {
    CfDateBase base;
    int year, month, day;
    gboolean adjust_date;
    int days, weeks, months, years;
    gboolean specify_time;
    CfTimeBase time_base;
    int hour, minute, second;
    gboolean adjust_time;
    int seconds, minutes, hours;
} CfDateExpression;

gboolean cf_date_expression_parse(const char *text, CfDateExpression *out, GError **error);
char *cf_date_expression_format(const CfDateExpression *expression);
GDateTime *cf_date_expression_evaluate(const CfDateExpression *expression, GDateTime *now, GError **error);
GDateTime *cf_date_expression_resolve(const char *text, GDateTime *now, gboolean *has_time, GError **error);
