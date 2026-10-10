#pragma once
#include <glib.h>
#include <string.h>
#define CF_TEXT_LIMIT 8192
#define CF_PATH_LIMIT 32768
#define CF_DATE_LIMIT 1024
#define CF_NUMBER_LIMIT 32
#define CF_SETTINGS_LIMIT (1024 * 1024)
/* GTK limits count Unicode characters. Bound bytes first before decoding. */
static inline gboolean cf_input_valid(const char *text, gsize characters)
{
    return text && strlen(text) <= characters * 4 && g_utf8_validate(text, -1, NULL) &&
           (gsize)g_utf8_strlen(text, -1) <= characters;
}
static inline int cf_input_limit(const char *property)
{
    if (!strcmp(property, "containing-text")) return 256 * 1024;
    if (!strcmp(property, "location")) return CF_PATH_LIMIT;
    if (!strcmp(property, "date-start") || !strcmp(property, "date-end")) return CF_DATE_LIMIT;
    if (!strcmp(property, "size-start") || !strcmp(property, "size-end") || !strcmp(property, "recent")) return CF_NUMBER_LIMIT;
    return CF_TEXT_LIMIT;
}
