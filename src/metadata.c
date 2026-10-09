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
