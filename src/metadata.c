#define _POSIX_C_SOURCE 200809L
#include "metadata.h"
#include <pwd.h>
#include <grp.h>
#include <errno.h>

struct CfMetadataCache { GHashTable *users, *groups; };
CfMetadataCache *cf_metadata_cache_new(void)
{
    CfMetadataCache *c = g_new0(CfMetadataCache, 1);
    c->users = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    c->groups = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    return c;
}
void cf_metadata_cache_free(CfMetadataCache *c)
{
    if (!c) return;
    g_hash_table_unref(c->users); g_hash_table_unref(c->groups); g_free(c);
}
static char *resolve(guint32 id, gboolean group)
{
    for (gsize size = 1024; size <= 65536; size *= 2) {
        g_autofree char *buffer = g_malloc(size);
        struct passwd pw = { 0 }, *user = NULL;
        struct group gr = { 0 }, *grp = NULL;
        int status = group ? getgrgid_r((gid_t)id, &gr, buffer, size, &grp) :
                             getpwuid_r((uid_t)id, &pw, buffer, size, &user);
        if (status == ERANGE) continue;
        if (status) break;
        const char *name = group ? (grp ? grp->gr_name : NULL) : (user ? user->pw_name : NULL);
        if (!status && name && *name) return g_utf8_make_valid(name, -1);
        break;
    }
    return g_strdup_printf("%u", id);
}
void cf_metadata_prepare(CfMetadataCache *c, GFileInfo *info)
{
    for (int group = 0; group <= 1; group++) {
        const char *number = group ? G_FILE_ATTRIBUTE_UNIX_GID : G_FILE_ATTRIBUTE_UNIX_UID;
        const char *attribute = group ? G_FILE_ATTRIBUTE_OWNER_GROUP : G_FILE_ATTRIBUTE_OWNER_USER;
        if (g_file_info_has_attribute(info, attribute) || !g_file_info_has_attribute(info, number)) continue;
        guint32 id = g_file_info_get_attribute_uint32(info, number);
        GHashTable *table = group ? c->groups : c->users;
        const char *name = g_hash_table_lookup(table, GUINT_TO_POINTER(id));
        /* After the bound, use numeric IDs without further NSS calls. */
        if (!name && g_hash_table_size(table) < 4096) {
            g_hash_table_insert(table, GUINT_TO_POINTER(id), resolve(id, group));
            name = g_hash_table_lookup(table, GUINT_TO_POINTER(id));
        }
        g_autofree char *fallback = name ? NULL : g_strdup_printf("%u", id);
        g_file_info_set_attribute_string(info, attribute, name ? name : fallback);
    }
}
char *cf_metadata_owner(GFileInfo *info, gboolean group)
{
    const char *attribute = group ? G_FILE_ATTRIBUTE_OWNER_GROUP : G_FILE_ATTRIBUTE_OWNER_USER;
    const char *number = group ? G_FILE_ATTRIBUTE_UNIX_GID : G_FILE_ATTRIBUTE_UNIX_UID;
    const char *name = g_file_info_get_attribute_string(info, attribute);
    if (name && *name) return g_utf8_make_valid(name, -1);
    if (g_file_info_has_attribute(info, number))
        return g_strdup_printf("%u", g_file_info_get_attribute_uint32(info, number));
    return g_strdup("");
}

void cf_metadata_prepare_type(GFileInfo *info, gboolean backend_type)
{
    /* A stat-denied backend may return only a name without a GError. Callers
     * retain such partial rows; do not invent type data or trip strict getters. */
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_NAME)) return;
    GFileType kind = g_file_info_get_file_type(info);
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK))
        g_file_info_set_is_symlink(info, kind == G_FILE_TYPE_SYMBOLIC_LINK);
    g_autofree char *guess = NULL;
    const char *type;
    if (backend_type) {
        /* A backend lacking this attribute must still fail the explicit filter;
         * do not silently substitute a filename estimate for its classification. */
        type = g_file_info_get_content_type(info);
        if (!type) return;
    } else {
        type = g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
        if (!type || !*type) {
            if (kind == G_FILE_TYPE_DIRECTORY) type = "inode/directory";
            else if (kind == G_FILE_TYPE_SYMBOLIC_LINK) type = "inode/symlink";
            else if (kind == G_FILE_TYPE_SPECIAL) type = "application/octet-stream";
            else {
                /* NULL data is essential: unknown names stay generic. */
                guess = g_content_type_guess(g_file_info_get_name(info), NULL, 0, NULL);
                type = guess ? guess : "application/octet-stream";
            }
        }
        g_file_info_set_content_type(info, type);
    }
    g_file_info_set_attribute_string(info, CF_METADATA_MIME_SOURCE, backend_type ? "backend" : "filename");
    g_autoptr(GIcon) icon = g_content_type_get_icon(type);
    if (icon) g_file_info_set_icon(info, icon);
}
