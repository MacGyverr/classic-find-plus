#pragma once
#include <gio/gio.h>
/* Worker task revalidates the source without following symlinks. No overwrite or delete fallback. */
void cf_file_operation_async(GFile *file, const char *expected_id, const char *new_name, GCancellable *cancel,
                             GAsyncReadyCallback callback, gpointer data);
GFile *cf_file_operation_finish(GAsyncResult *result, GError **error);
gboolean cf_file_name_valid(const char *name);
/* Plain-language guidance for access failures; NULL for unrelated errors. */
const char *cf_file_error_hint(const GError *error);
