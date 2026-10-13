#pragma once
#include <gio/gio.h>
/* Lossless filesystem basename converted to UTF-8; NULL for unsupported encoding. */
char *cf_result_name(GFile *file, GError **error);
/* UTF-8 character offset of the final-extension boundary, or the full name. */
gint cf_result_stem_length(const char *name, gboolean directory);
