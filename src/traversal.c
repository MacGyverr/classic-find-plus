#define _XOPEN_SOURCE 700
#include "traversal.h"
#include "criteria-private.h"
#include <stdlib.h>
#include <errno.h>

char *cf_traversal_identity(GFileInfo *info)
{
    const char *fs=g_file_info_get_attribute_string(info,G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    const char *id=g_file_info_get_attribute_string(info,G_FILE_ATTRIBUTE_ID_FILE);
    if (!fs || !*fs || !id || !*id) return NULL;
    /* Length prefix keeps arbitrary backend identifiers unambiguous. */
    return g_strdup_printf("%" G_GSIZE_FORMAT ":%s%s",strlen(fs),fs,id);
}
gboolean cf_traversal_target_allowed(const CfCriteria *c, GFile *root, GFile *directory,
    GCancellable *cancel, GError **error)
{
    if (g_cancellable_set_error_if_cancelled(cancel,error)) return FALSE;
    if (g_file_equal(root,directory)) return TRUE;
    g_autofree char *path=g_file_get_path(directory), *root_path=g_file_get_path(root);
    if (!path || !root_path) {
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_SUPPORTED,
            "Safe symbolic-link traversal requires native local or mounted paths.");
        return FALSE;
    }
    g_autofree char *resolved=realpath(path,NULL);
    int saved=errno;
    g_autofree char *base=resolved ? realpath(root_path,NULL) : NULL;
    if (resolved && !base) saved=errno;
    if (!resolved || !base) {
        g_set_error(error,G_IO_ERROR,g_io_error_from_errno(saved),"Cannot resolve symbolic-link target: %s",g_strerror(saved));
        return FALSE;
    }
    g_autoptr(GFile) target=g_file_new_for_path(resolved), boundary=g_file_new_for_path(base);
    /* Inside the selected root, exempt its own name and ancestors. Outside it,
     * examine every resolved target ancestor, preventing aliases into pruned trees. */
    gboolean inside=g_file_equal(target,boundary) || g_file_has_prefix(target,boundary);
    while (target && !(inside && g_file_equal(target,boundary))) {
        if (g_cancellable_set_error_if_cancelled(cancel,error)) return FALSE;
        g_autofree char *name=g_file_get_basename(target);
        g_autoptr(GFileInfo) info=g_file_info_new();
        g_file_info_set_name(info,name ? name : "");
        g_file_info_set_file_type(info,G_FILE_TYPE_DIRECTORY);
        if (cf_criteria_excludes(c,info)) return FALSE;
        GFile *parent=g_file_get_parent(target);
        g_object_unref(target);target=parent;
    }
    return TRUE;
}
