#define _GNU_SOURCE
#include "input-limits.h"
#include "file-operations.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#endif

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
/* No check/rename fallback: destination non-overwrite is an atomic kernel property.
 * Source descriptors pin identity, but Linux has no conditional-inode rename. The
 * owner accepted the residual source-name replacement race before this syscall. */
static GFile *rename_local(GFile *file, GFileInfo *info, const char *name,
                          GCancellable *cancel, GError **error)
{
#ifndef __linux__
    (void)file; (void)info; (void)name; (void)cancel;
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Safe rename requires Linux atomic no-overwrite support.");
    return NULL;
#else
    g_autofree char *path = g_file_get_path(file);
    g_autoptr(GFile) parent = g_file_get_parent(file);
    g_autofree char *parent_path = parent ? g_file_get_path(parent) : NULL;
    g_autofree char *old = g_file_get_basename(file);
    g_autofree char *raw = g_filename_from_utf8(name, -1, NULL, NULL, error);
    if (!raw) return NULL;
    if (!path || !parent_path || !old || !strcmp(old, ".") || !strcmp(old, "..")) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Only local filesystem entries can be safely renamed.");
        return NULL;
    }
    int dir = open(parent_path, O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) goto failed;
    int source = openat(dir, old, O_PATH | O_NOFOLLOW | O_CLOEXEC);
    if (source < 0) { int saved = errno; close(dir); errno = saved; goto failed; }
    struct stat pinned, current, parent_pinned, parent_current;
    gboolean checked = fstat(source, &pinned) == 0 && fstat(dir, &parent_pinned) == 0 &&
        stat(parent_path, &parent_current) == 0 && fstatat(dir, old, &current, AT_SYMLINK_NOFOLLOW) == 0;
    gboolean identity = checked && pinned.st_dev == current.st_dev && pinned.st_ino == current.st_ino &&
        parent_pinned.st_dev == parent_current.st_dev && parent_pinned.st_ino == parent_current.st_ino &&
        g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_DEVICE) &&
        g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_INODE) &&
        pinned.st_dev == g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_DEVICE) &&
        pinned.st_ino == g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_UNIX_INODE);
    if (!identity) {
        close(source); close(dir);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "The file or its folder changed since verification. Search again before renaming.");
        return NULL;
    }
    if (g_cancellable_set_error_if_cancelled(cancel, error)) { close(source); close(dir); return NULL; }
    int result = !strcmp(old, raw) ? 0 : syscall(SYS_renameat2, dir, old, dir, raw, RENAME_NOREPLACE);
    int saved = errno;
    close(source); close(dir);
    if (result == 0) return g_file_get_child(parent, raw);
    errno = saved;
failed:;
    int code = errno;
    if (code == ENOSYS || code == EINVAL || code == EOPNOTSUPP)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "This filesystem does not support atomic no-overwrite rename.");
    else g_set_error(error, G_IO_ERROR, g_io_error_from_errno(code), "Cannot rename this item: %s", g_strerror(code));
    return NULL;
#endif
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
    g_autoptr(GFileInfo) info = g_file_query_info(file, "standard::type,id::file,unix::device,unix::inode", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, &error);
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
        destination = rename_local(file, info, name, cancel, &error);
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
