#include "input-limits.h"
#include "file-operations.h"

const char *cf_file_error_hint(const GError *error)
{
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        return "Your account does not have access to this item or its folder. Ask its owner for access, or choose an accessible location.";
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_READ_ONLY))
        return "This filesystem is read-only. Choose a writable location.";
    return NULL;
}

gboolean cf_file_name_valid(const char *name)
{
    return cf_input_valid(name, CF_TEXT_LIMIT) && *name && !strchr(name, '/') &&
           strcmp(name, ".") && strcmp(name, "..");
}
typedef struct { char *name, *id; } Operation;
static void operation_free(gpointer data)
{
    Operation *op = data;
    g_free(op->name); g_free(op->id); g_free(op);
}
static void operation(GTask *task, gpointer source, gpointer task_data, GCancellable *cancel)
{
    GFile *file = source;
    Operation *op = task_data;
    const char *name = op->name;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFileInfo) info = g_file_query_info(file, "standard::type,id::file", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, &error);
    if (!info) { g_task_return_error(task, g_steal_pointer(&error)); return; }
    if (op->id && g_strcmp0(op->id, g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE))) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "The file has been replaced since the search. Search again before changing it.");
        return;
    }
    GFile *destination = NULL;
    if (name) {
        if (!cf_file_name_valid(name)) {
            g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME, "Enter a single valid filename.");
            return;
        }
        destination = g_file_set_display_name(file, name, cancel, &error);
    } else if (g_file_trash(file, cancel, &error)) destination = g_object_ref(file);
    if (destination) g_task_return_pointer(task, destination, g_object_unref);
    else g_task_return_error(task, g_steal_pointer(&error));
}
void cf_file_operation_async(GFile *file, const char *expected_id, const char *new_name, GCancellable *cancel,
                             GAsyncReadyCallback callback, gpointer data)
{
    GTask *task = g_task_new(file, cancel, callback, data);
    Operation *op = g_new0(Operation, 1);
    op->name = g_strdup(new_name); op->id = g_strdup(expected_id);
    g_task_set_task_data(task, op, operation_free);
    /* Once a mutation succeeds, report that success even if cancellation arrived concurrently. */
    g_task_set_check_cancellable(task, FALSE);
    g_task_run_in_thread(task, operation);
    g_object_unref(task);
}
GFile *cf_file_operation_finish(GAsyncResult *result, GError **error)
{
    g_return_val_if_fail(G_IS_TASK(result), NULL);
    return g_task_propagate_pointer(G_TASK(result), error);
}
