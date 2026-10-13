#include "result-names.h"
#include <string.h>

char *cf_result_name(GFile *file, GError **error)
{
    g_autofree char *raw = g_file_get_basename(file);
    if (!raw) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME, "This location has no editable filename.");
        return NULL;
    }
    char *text = g_filename_to_utf8(raw, -1, NULL, NULL, error);
    if (!text) return NULL;
    g_autofree char *roundtrip = g_filename_from_utf8(text, -1, NULL, NULL, NULL);
    if (!roundtrip || strcmp(raw, roundtrip)) {
        g_free(text);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                            "The filename cannot be represented as text without changing it.");
        return NULL;
    }
    return text;
}
gint cf_result_stem_length(const char *name, gboolean directory)
{
    const char *dot = directory ? NULL : strrchr(name, '.');
    return g_utf8_strlen(name, dot && dot != name && dot[1] ? dot - name : -1);
}
