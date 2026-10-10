#include "admin-protocol.h"
#include "criteria-private.h"
#include <string.h>

static gboolean invalid(GError **error)
{
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid administrator search message.");
    return FALSE;
}
static gboolean bounded(GVariant *value, const char *type, gsize limit)
{
    return value && g_variant_is_of_type(value, G_VARIANT_TYPE(type)) &&
        g_variant_get_size(value) <= limit && g_variant_is_normal_form(value);
}
GVariant *cf_admin_criteria_encode(const CfCriteria *c)
{
    return g_variant_new(CF_ADMIN_CRITERIA_TYPE,
        CF_ADMIN_PROTOCOL_VERSION, c->input_name, c->input_exclude, c->input_extension,
        c->name_mode, c->type, c->date_field, c->size_mode,
        c->size_start, c->size_end, c->date_start, c->date_end,
        c->has_start, c->has_end, c->case_sensitive, c->extension_only,
        c->recursive, c->stay_on_filesystem, c->has_uid, c->has_gid, c->has_permissions,
        c->uid, c->gid, c->permissions, c->no_hidden, c->follow_symlinks, c->input_content, c->content_mode);
}
CfCriteria *cf_admin_criteria_decode(GVariant *value, GError **error)
{
    if (!bounded(value, CF_ADMIN_CRITERIA_TYPE, 450 * 1024)) { invalid(error); return NULL; }
    const char *name, *exclude, *extension, *content;
    guint version, mode, type, field, size_mode, uid, gid, permissions, content_mode;
    gboolean has_uid, has_gid, has_permissions, hidden, follow;
    guint64 lower, upper;
    gint64 start, end;
    gboolean has_start, has_end, sensitive, extension_only, recursive, stay;
    g_variant_get(value, "(u&s&s&suuuuttxxbbbbbbbbbuuubb&su)", &version, &name, &exclude, &extension,
        &mode, &type, &field, &size_mode, &lower, &upper, &start, &end,
        &has_start, &has_end, &sensitive, &extension_only, &recursive, &stay,
        &has_uid, &has_gid, &has_permissions, &uid, &gid, &permissions, &hidden, &follow, &content, &content_mode);
    if (version != CF_ADMIN_PROTOCOL_VERSION) {
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,
            "Administrator search protocol version mismatch; restart CFP after updating its package.");
        return NULL;
    }
    if ((has_uid && uid == G_MAXUINT32) ||
        (has_gid && gid == G_MAXUINT32) || permissions > 07777 ||
        (!has_uid && uid) || (!has_gid && gid) || (!has_permissions && permissions) || mode > CF_NAME_REGEX || type > CF_TYPE_TEXT || field > CF_DATE_ACCESSED || size_mode > CF_SIZE_EQUAL ||
        start < G_GINT64_CONSTANT(-62135596800000000) || end < G_GINT64_CONSTANT(-62135596800000000) ||
        start > G_GINT64_CONSTANT(253402300800000000) || end > G_GINT64_CONSTANT(253402300800000000) ||
        (has_start && has_end && start >= end)) { invalid(error); return NULL; }
    g_autofree char *size_start = g_strdup_printf("%" G_GUINT64_FORMAT, lower);
    g_autofree char *size_end = g_strdup_printf("%" G_GUINT64_FORMAT, upper);
    CfCriteriaInput input = { .containing = content, .content_mode = content_mode, .name = name, .exclude = exclude, .extension = extension,
        .name_mode = mode, .type = type, .date_field = field, .size_mode = size_mode,
        .size_start = size_start, .size_end = size_end, .case_sensitive = sensitive,
        .extension_only = extension_only, .recursive = recursive, .stay_on_filesystem = stay };
    CfCriteria *c = cf_criteria_new(&input, NULL, error);
    if (c) {
        c->has_uid = has_uid; c->has_gid = has_gid; c->has_permissions = has_permissions;
        c->uid = uid; c->gid = gid; c->permissions = permissions; c->no_hidden = hidden; c->follow_symlinks = follow;
        c->date_start = start; c->date_end = end; c->has_start = has_start; c->has_end = has_end; }
    return c;
}
GFile *cf_admin_root(const char *uri, GError **error)
{
    if (!uri || strlen(uri) > 32768 || !g_utf8_validate(uri, -1, NULL) || !g_str_has_prefix(uri, "file:///")) {
        invalid(error); return NULL;
    }
    g_autoptr(GError) local_error = NULL;
    g_autofree char *host = NULL;
    g_autofree char *path = g_filename_from_uri(uri, &host, &local_error);
    if (!path || (host && *host) || !g_path_is_absolute(path)) { invalid(error); return NULL; }
    return g_file_new_for_path(path);
}

typedef struct { const char *name; GFileAttributeType type; } Attribute;
static const Attribute attributes[] = {
    { "standard::name", G_FILE_ATTRIBUTE_TYPE_BYTE_STRING },
    { "standard::display-name", G_FILE_ATTRIBUTE_TYPE_STRING },
    { "standard::type", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "standard::is-symlink", G_FILE_ATTRIBUTE_TYPE_BOOLEAN },
    { "standard::size", G_FILE_ATTRIBUTE_TYPE_UINT64 },
    { "standard::icon", G_FILE_ATTRIBUTE_TYPE_OBJECT },
    { "standard::content-type", G_FILE_ATTRIBUTE_TYPE_STRING },
    { "time::created", G_FILE_ATTRIBUTE_TYPE_UINT64 }, { "time::created-usec", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "time::access", G_FILE_ATTRIBUTE_TYPE_UINT64 }, { "time::access-usec", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "time::modified", G_FILE_ATTRIBUTE_TYPE_UINT64 }, { "time::modified-usec", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "unix::mode", G_FILE_ATTRIBUTE_TYPE_UINT32 }, { "unix::uid", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "unix::gid", G_FILE_ATTRIBUTE_TYPE_UINT32 },
    { "id::filesystem", G_FILE_ATTRIBUTE_TYPE_STRING }, { "id::file", G_FILE_ATTRIBUTE_TYPE_STRING },
    { "owner::user", G_FILE_ATTRIBUTE_TYPE_STRING }, { "owner::group", G_FILE_ATTRIBUTE_TYPE_STRING }
};
static const char *variant_type(GFileAttributeType type)
{
    switch (type) {
    case G_FILE_ATTRIBUTE_TYPE_UINT32: return "u";
    case G_FILE_ATTRIBUTE_TYPE_UINT64: return "t";
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN: return "b";
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING: return "ay";
    default: return "s";
    }
}
GVariant *cf_admin_result_encode(CfResult *r, GError **error)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
    for (guint i = 0; i < G_N_ELEMENTS(attributes); i++) {
        const Attribute *a = &attributes[i];
        if (!g_file_info_has_attribute(r->info, a->name)) continue;
        GVariant *v = NULL;
        switch (a->type) {
        case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
            v = g_variant_new_bytestring(g_file_info_get_attribute_byte_string(r->info, a->name)); break;
        case G_FILE_ATTRIBUTE_TYPE_STRING:
            v = g_variant_new_string(g_file_info_get_attribute_string(r->info, a->name)); break;
        case G_FILE_ATTRIBUTE_TYPE_UINT32:
            v = g_variant_new_uint32(g_file_info_get_attribute_uint32(r->info, a->name)); break;
        case G_FILE_ATTRIBUTE_TYPE_UINT64:
            v = g_variant_new_uint64(g_file_info_get_attribute_uint64(r->info, a->name)); break;
        case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
            v = g_variant_new_boolean(g_file_info_get_attribute_boolean(r->info, a->name)); break;
        case G_FILE_ATTRIBUTE_TYPE_OBJECT: {
            /* Only theme icons cross IPC. Never deserialize arbitrary GIcon classes. */
            GIcon *icon = g_file_info_get_icon(r->info);
            if (G_IS_THEMED_ICON(icon)) {
                const char * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
                if (names && names[0]) v = g_variant_new_string(names[0]);
            }
            break;
        }
        default: break;
        }
        if (v) g_variant_builder_add(&b, "{sv}", a->name, v);
    }
    g_autofree char *uri = g_file_get_uri(r->file);
    GVariant *value = g_variant_ref_sink(g_variant_new("(s@a{sv})", uri, g_variant_builder_end(&b)));
    if (!bounded(value, "(sa{sv})", 64 * 1024)) { g_variant_unref(value); invalid(error); return NULL; }
    return value;
}
CfResult *cf_admin_result_decode(GVariant *value, GError **error)
{
    if (!bounded(value, "(sa{sv})", 64 * 1024)) { invalid(error); return NULL; }
    const char *uri;
    g_autoptr(GVariant) dict = NULL;
    g_variant_get(value, "(&s@a{sv})", &uri, &dict);
    g_autoptr(GFile) file = cf_admin_root(uri, error);
    if (!file) return NULL;
    g_autoptr(GFileInfo) info = g_file_info_new();
    if (g_variant_n_children(dict) > G_N_ELEMENTS(attributes)) { invalid(error); return NULL; }
    GVariantIter iter;
    const char *key;
    GVariant *v;
    g_variant_iter_init(&iter, dict);
    while (g_variant_iter_next(&iter, "{&sv}", &key, &v)) {
        g_autoptr(GVariant) owned = v;
        const Attribute *a = NULL;
        for (guint i = 0; i < G_N_ELEMENTS(attributes); i++)
            if (!strcmp(key, attributes[i].name)) { a = &attributes[i]; break; }
        if (!a || g_file_info_has_attribute(info, key) ||
            !bounded(v, variant_type(a->type), 32768)) { invalid(error); return NULL; }
        switch (a->type) {
        case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING: {
            gsize len;
            const guint8 *bytes = g_variant_get_fixed_array(v, &len, 1);
            if (!len || bytes[len-1] || memchr(bytes, 0, len-1) || memchr(bytes, '/', len-1)) {
                invalid(error); return NULL;
            }
            g_file_info_set_attribute_byte_string(info, key, (const char *)bytes); break;
        }
        case G_FILE_ATTRIBUTE_TYPE_STRING:
            g_file_info_set_attribute_string(info, key, g_variant_get_string(v, NULL)); break;
        case G_FILE_ATTRIBUTE_TYPE_UINT32:
            g_file_info_set_attribute_uint32(info, key, g_variant_get_uint32(v)); break;
        case G_FILE_ATTRIBUTE_TYPE_UINT64:
            g_file_info_set_attribute_uint64(info, key, g_variant_get_uint64(v)); break;
        case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
            g_file_info_set_attribute_boolean(info, key, g_variant_get_boolean(v)); break;
        case G_FILE_ATTRIBUTE_TYPE_OBJECT: {
            g_autoptr(GIcon) icon = g_themed_icon_new(g_variant_get_string(v, NULL));
            g_file_info_set_icon(info, icon); break;
        }
        default: g_assert_not_reached();
        }
    }
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_NAME) ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) { invalid(error); return NULL; }
    const char *name = g_file_info_get_name(info);
    g_autofree char *basename = g_file_get_basename(file);
    if (!name || !*name || !strcmp(name, ".") || !strcmp(name, "..") || g_strcmp0(name, basename) ||
        g_file_info_get_file_type(info) > G_FILE_TYPE_MOUNTABLE) { invalid(error); return NULL; }
    /* Required by existing renderers; optional metadata has safe inert defaults. */
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK)) g_file_info_set_is_symlink(info, FALSE);
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
        g_file_info_set_content_type(info, "application/octet-stream");
    if (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_ICON)) {
        g_autoptr(GIcon) icon = g_themed_icon_new("text-x-generic"); g_file_info_set_icon(info, icon);
    }
    CfResult *r = g_new0(CfResult, 1);
    r->file = g_steal_pointer(&file); r->info = g_steal_pointer(&info);
    return r;
}
GVariant *cf_admin_stats_encode(const CfSearchStats *s)
{
    g_autofree char *message = g_utf8_make_valid(s->first_error ? s->first_error : "", -1);
    if (strlen(message) > 4096) *g_utf8_find_prev_char(message, message + 4096) = 0;
    return g_variant_new(CF_ADMIN_STATS_TYPE, s->scanned, s->matched, s->errors,
        s->directories_enumerated, s->directories_pruned, s->finished, s->cancelled, message);
}
gboolean cf_admin_stats_decode(GVariant *v, CfSearchStats *s, GError **error)
{
    if (!bounded(v, CF_ADMIN_STATS_TYPE, 8192)) return invalid(error);
    const char *message;
    *s = (CfSearchStats){0};
    g_variant_get(v, "(tttttbb&s)", &s->scanned, &s->matched, &s->errors,
        &s->directories_enumerated, &s->directories_pruned, &s->finished, &s->cancelled, &message);
    s->first_error = *message ? g_strdup(message) : NULL;
    return TRUE;
}
