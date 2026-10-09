#pragma once
#include <gio/gio.h>
#include <string.h>
/* GIO stores seconds as uint64, including signed Unix seconds cast to uint64.
 * Validate representability before conversion to microseconds or GLib calendar APIs. */
static inline gboolean cf_file_time(GFileInfo *info, const char *attribute, gint64 *stamp)
{
    if (g_file_info_get_attribute_type(info, attribute) != G_FILE_ATTRIBUTE_TYPE_UINT64) return FALSE;
    guint64 raw = g_file_info_get_attribute_uint64(info, attribute);
    gint64 seconds = raw <= G_MAXINT64 ? (gint64)raw : -1 - (gint64)(G_MAXUINT64 - raw);
    /* GLib's representable UTC calendar is years 1 through 9999. */
    if (seconds < G_GINT64_CONSTANT(-62135596800) || seconds > G_GINT64_CONSTANT(253402300799)) return FALSE;
    const char *usec = !strcmp(attribute, G_FILE_ATTRIBUTE_TIME_CREATED) ? G_FILE_ATTRIBUTE_TIME_CREATED_USEC :
        !strcmp(attribute, G_FILE_ATTRIBUTE_TIME_ACCESS) ? G_FILE_ATTRIBUTE_TIME_ACCESS_USEC : G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC;
    guint32 fraction = 0;
    if (g_file_info_has_attribute(info, usec)) {
        if (g_file_info_get_attribute_type(info, usec) != G_FILE_ATTRIBUTE_TYPE_UINT32) return FALSE;
        fraction = g_file_info_get_attribute_uint32(info, usec);
        if (fraction >= G_USEC_PER_SEC) return FALSE;
    }
    *stamp = seconds * G_USEC_PER_SEC + fraction;
    return TRUE;
}

static inline gint64 cf_file_time_seconds(gint64 microseconds)
{
    return microseconds / G_USEC_PER_SEC - (microseconds % G_USEC_PER_SEC < 0);
}
