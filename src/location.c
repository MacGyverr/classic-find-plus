#include "input-limits.h"
#include "classic-find.h"
#include "settings.h"

GFile *cf_location_resolve(const char *argument, GError **error)
{
    g_autofree char *cwd = NULL;
    if (!argument) {
        cwd = g_get_current_dir();
        argument = cwd;
    }
    /* CLI filename arguments may contain non-UTF-8 Unix path bytes. GTK/config
     * text is validated separately; preserve raw file identity here. */
    if (strlen(argument) > CF_PATH_LIMIT * 4 ||
        (g_utf8_validate(argument, -1, NULL) && !cf_input_valid(argument, CF_PATH_LIMIT))) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Search path exceeds the length limit (32768 characters or 131072 raw bytes).");
        return NULL;
    }
    GFile *file = g_file_new_for_commandline_arg(argument);
    g_autoptr(GFileInfo) info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                               G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!info) {
        g_object_unref(file);
        return NULL;
    }
    if (g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY,
                            "Search location must be a directory");
        g_object_unref(file);
        return NULL;
    }
    return file;
}

GFile *cf_location_startup(const char *argument, GError **error)
{
    if (argument) return cf_location_resolve(argument, error);
    g_autoptr(CfSettings) settings = cf_settings_new();
    g_autofree char *saved = g_key_file_get_string(cf_settings_data(settings), "Search", "location", NULL);
    GFile *file = saved && *saved && g_utf8_validate(saved, -1, NULL) ? cf_location_resolve(saved, NULL) : NULL;
    return file ? file : cf_location_resolve(g_get_home_dir(), error);
}
