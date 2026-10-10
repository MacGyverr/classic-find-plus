#define _XOPEN_SOURCE 700
#include "root-mode.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static gboolean safe_directory(const char *path, uid_t owner, GError **error)
{
    struct stat st;
    if (lstat(path, &st) < 0) {
        if (errno == ENOENT) return TRUE; /* Created later by its account owner. */
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno), "Cannot inspect administrator profile: %s", g_strerror(errno));
        return FALSE;
    }
    if (!S_ISDIR(st.st_mode) || st.st_uid != owner || (st.st_mode & 0022)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "Unsafe administrator profile directory: %s (must be owned by the account, private and not a symlink)", path);
        return FALSE;
    }
    return TRUE;
}
gboolean cf_root_environment(const char *home, uid_t owner, GError **error)
{
    g_autofree char *canonical = home ? realpath(home, NULL) : NULL;
    if (!canonical || !g_path_is_absolute(home) || strcmp(home, canonical) ||
        !safe_directory(home, owner, error)) {
        if (!error || !*error) g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Cannot use trusted administrator home directory.");
        return FALSE;
    }
    const char *dirs[] = { ".config", ".cache", ".local", ".local/share" };
    for (guint i = 0; i < G_N_ELEMENTS(dirs); i++) {
        g_autofree char *directory = g_build_filename(home, dirs[i], NULL);
        if (!safe_directory(directory, owner, error)) return FALSE;
        g_autofree char *profile = g_build_filename(directory, "classic-find-plus-administrator", NULL);
        if (!safe_directory(profile, owner, error)) return FALSE;
    }
    g_autofree char *config = g_build_filename(home, ".config", "classic-find-plus-administrator", NULL);
    g_autofree char *cache = g_build_filename(home, ".cache", "classic-find-plus-administrator", NULL);
    g_autofree char *data = g_build_filename(home, ".local/share", "classic-find-plus-administrator", NULL);
    /* Must precede any GLib user-directory cache lookup. */
    g_setenv("HOME", home, TRUE);
    g_setenv("XDG_CONFIG_HOME", config, TRUE); g_setenv("XDG_CACHE_HOME", cache, TRUE);
    g_setenv("XDG_DATA_HOME", data, TRUE);
    return TRUE;
}
