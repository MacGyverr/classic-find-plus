#include "thunar-admin.h"
#include <string.h>

char *cf_thunar_admin_uri(const char *target, GError **error)
{
    g_autofree char *path = NULL, *host = NULL;
    if (target && g_path_is_absolute(target)) path = g_strdup(target);
    else if (target && g_str_has_prefix(target, "file:")) {
        g_autoptr(GUri) parsed = g_uri_parse(target, G_URI_FLAGS_ENCODED, error);
        if (!parsed) return NULL;
        if (g_uri_get_query(parsed) || g_uri_get_fragment(parsed) ||
            g_uri_get_userinfo(parsed) || g_uri_get_port(parsed) != -1) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                "Local directory URIs cannot contain query, fragment or credentials.");
            return NULL;
        }
        path = g_filename_from_uri(target, &host, error);
        if (!path) return NULL;
        if (host && *host && g_ascii_strcasecmp(host, "localhost")) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                "Only local directories can be opened as Administrator.");
            return NULL;
        }
    } else {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "Choose a local directory; remote and admin URIs are unsupported.");
        return NULL;
    }
    g_autoptr(GFile) file = g_file_new_for_path(path);
    g_autoptr(GError) local = NULL;
    g_autoptr(GFileInfo) info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                               G_FILE_QUERY_INFO_NONE, NULL, &local);
    /* Inaccessible ancestors are precisely why an admin location may be needed. */
    if (!info && !g_error_matches(local, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
        g_propagate_error(error, g_steal_pointer(&local)); return NULL;
    }
    if (info && g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
        g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY, "Choose a directory, not a regular file.");
        return NULL;
    }
    g_autofree char *uri = g_file_get_uri(file);
    return g_strconcat("admin:", uri + strlen("file:"), NULL);
}

gboolean cf_thunar_admin_launch(const char *uri, const char *program,
                                const char *const *schemes, GError **error)
{
    if (!schemes || !g_strv_contains(schemes, "admin")) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            "GVfs administrative access is unavailable. Check your distribution's GVfs admin backend and desktop authentication agent.");
        return FALSE;
    }
    if (!program) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Thunar is unavailable. Install/configure Thunar separately.");
        return FALSE;
    }
    const char *argv[] = { program, uri, NULL };
    g_autoptr(GSubprocess) process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, error);
    if (!process) return FALSE;
    /* Thunar owns GVfs mounting, Polkit prompts and location errors. No shell. */
    return g_subprocess_wait_check(process, NULL, error);
}
