#define _XOPEN_SOURCE 700
#include "traversal.h"
#include "criteria-private.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

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

typedef struct { const CfCriteria *criteria; GFile *root, *file; GCancellable *cancel; } ContentScope;
static gboolean content_scope(GFile *target, guint64 device, gpointer data, GError **error)
{
    ContentScope *scope=data;
    if (!scope->criteria->follow_symlinks) {
        /* The selected root itself may be a link. Below it, reject unexpected
         * intermediate aliases (including an ancestor replaced after enumeration). */
        g_autofree char *root_path=g_file_get_path(scope->root);
        g_autofree char *base=root_path ? realpath(root_path,NULL) : NULL;
        g_autofree char *relative=g_file_get_relative_path(scope->root,scope->file);
        if (!base || !relative) {
            g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Cannot verify content path beneath search root.");return FALSE;
        }
        g_autoptr(GFile) boundary=g_file_new_for_path(base);
        g_autoptr(GFile) expected=g_file_resolve_relative_path(boundary,relative);
        if (!g_file_equal(target,expected)) return FALSE;
    }
    g_autoptr(GFile) parent=g_file_get_parent(target);
    if (!parent || !cf_traversal_target_allowed(scope->criteria,scope->root,parent,scope->cancel,error)) return FALSE;
    if (scope->criteria->stay_on_filesystem) {
        g_autofree char *path=g_file_get_path(scope->root);
        struct stat root;
        if (!path || stat(path,&root)) {
            g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Cannot verify content root filesystem.");return FALSE;
        }
        if ((guint64)root.st_dev!=device) return FALSE;
    }
    return TRUE;
}
gboolean cf_traversal_content_matches(const CfCriteria *c, GFile *root,
    GFile *file, GCancellable *cancel, GError **error)
{
    ContentScope scope={c,root,file,cancel};
    return cf_content_match_checked(c->content,file,c->follow_symlinks,cancel,content_scope,&scope,error);
}
