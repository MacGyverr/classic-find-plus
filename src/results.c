#include "results.h"
#include "metadata.h"
#include "file-time.h"
#include "selection.h"
#include "result-actions.h"
#include "result-columns.h"
#include "results-sync.h"
#include "result-names.h"

GtkListStore *cf_results_new(void)
{
    return gtk_list_store_new(CF_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INT64, G_TYPE_FILE,
        G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_INT64, G_TYPE_INT64, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

GtkWidget *cf_results_view_new(GtkListStore *store)
{
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    cf_result_columns_attach(GTK_TREE_VIEW(view));
    gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), CF_COL_PATH);
    cf_results_sync_attach(GTK_TREE_VIEW(view));
    /* Rename observes pointer events before selection delays/captures them for DnD. */
    cf_result_actions_attach(GTK_TREE_VIEW(view));
    cf_selection_attach(GTK_TREE_VIEW(view));
    return view;
}

static char *timestamp(GFileInfo *info, const char *attribute, gint64 *stamp)
{
    *stamp = G_MININT64;
    gint64 value;
    if (!cf_file_time(info, attribute, &value)) return g_strdup("");
    g_autoptr(GDateTime) utc = g_date_time_new_from_unix_utc(cf_file_time_seconds(value));
    g_autoptr(GDateTime) local = utc ? g_date_time_to_local(utc) : NULL;
    if (!local) return g_strdup("");
    *stamp = value;
    return g_date_time_format(local, "%Y-%m-%d %H:%M:%S");
}

char *cf_result_exact_size(guint64 bytes, gboolean bits)
{
    /* uint64 has at most 20 decimal digits; eightfold needs at most 21.
     * Grouping adds at most six commas. Bounds include the NUL terminator. */
    char digits[22], grouped[28];
    g_snprintf(digits, sizeof digits, "%" G_GUINT64_FORMAT, bytes);
    gsize length = strlen(digits);
    if (bits) {
        unsigned carry = 0;
        for (gsize i = length; i > 0; i--) {
            unsigned value = (unsigned)(digits[i - 1] - '0') * 8 + carry;
            digits[i - 1] = '0' + value % 10; carry = value / 10;
        }
        if (carry) {
            memmove(digits + 1, digits, length + 1);
            digits[0] = '0' + carry; length++;
        }
    }
    gsize out = 0;
    for (gsize i = 0; i < length; i++) {
        if (i && (length - i) % 3 == 0) grouped[out++] = ',';
        grouped[out++] = digits[i];
    }
    grouped[out] = 0;
    return g_strdup(grouped);
}

static void set_result(GtkListStore *store, GtkTreeIter *iter, const CfResult *result, gboolean insert)
{
    GFileInfo *info = result->info;
    g_autoptr(GFile) parent = g_file_get_parent(result->file);
    g_autofree char *folder = parent ? cf_result_path(parent, NULL) : g_strdup("");
    if (!folder) folder = g_file_get_uri(parent);
    g_autofree char *path = cf_result_path(result->file, NULL);
    if (!path) path = g_file_get_uri(result->file);
    g_autofree char *name = g_filename_display_name(g_file_info_get_name(info));
    goffset signed_size = g_file_info_get_size(info);
    guint64 size = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    gboolean directory = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY;
    g_autofree char *size_text = directory ? g_strdup("") : g_format_size_full(signed_size >= 0 ? (guint64)signed_size : 0, G_FORMAT_SIZE_IEC_UNITS);
    gboolean sized = !directory && g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    g_autofree char *bytes = sized ? cf_result_exact_size(size, FALSE) : g_strdup("");
    g_autofree char *bits = sized ? cf_result_exact_size(size, TRUE) : g_strdup("");
    const char *content = g_file_info_get_content_type(info);
    g_autofree char *type = directory ? g_strdup("Folder") :
        g_file_info_get_is_symlink(info) ? g_strdup("Symbolic link") :
        g_strdup(g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR ? "Regular file" : "File");
    g_autofree char *mime = content ? g_content_type_get_mime_type(content) : g_strdup("");
    g_autofree char *detail = content ? g_content_type_get_description(content) : g_strdup("");
    g_autofree char *owner = cf_metadata_owner(info, FALSE);
    g_autofree char *group = cf_metadata_owner(info, TRUE);
    gint64 stamp, created_stamp, accessed_stamp;
    g_autofree char *modified = timestamp(info, G_FILE_ATTRIBUTE_TIME_MODIFIED, &stamp);
    g_autofree char *created = timestamp(info, G_FILE_ATTRIBUTE_TIME_CREATED, &created_stamp);
    g_autofree char *accessed = timestamp(info, G_FILE_ATTRIBUTE_TIME_ACCESS, &accessed_stamp);
    gint64 mode = g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_MODE) ?
                  (gint64)(g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE) & 07777) : -1;
    g_autofree char *permissions = mode >= 0 ? g_strdup_printf("%04o", (guint)mode) : g_strdup("");
    if (insert) gtk_list_store_insert_with_values(store, iter, -1,
        CF_COL_NAME, name, CF_COL_FOLDER, folder, CF_COL_SIZE_TEXT, size_text,
        CF_COL_TYPE, type, CF_COL_MODIFIED_TEXT, modified, CF_COL_SIZE, size,
        CF_COL_MODIFIED, stamp, CF_COL_FILE, result->file,
        CF_COL_ICON, g_file_info_get_icon(info), CF_COL_PATH, path,
        CF_COL_ID, g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE),
        CF_COL_CREATED_TEXT, created, CF_COL_ACCESSED_TEXT, accessed, CF_COL_PERMISSIONS_TEXT, permissions,
        CF_COL_CREATED, created_stamp, CF_COL_ACCESSED, accessed_stamp, CF_COL_PERMISSIONS, mode,
        CF_COL_MIME, mime, CF_COL_DETAIL, detail, CF_COL_GROUP, group, CF_COL_OWNER, owner, CF_COL_BYTES_TEXT, bytes, CF_COL_BITS_TEXT, bits, -1);
    else gtk_list_store_set(store, iter,
        CF_COL_NAME, name, CF_COL_FOLDER, folder, CF_COL_SIZE_TEXT, size_text,
        CF_COL_TYPE, type, CF_COL_MODIFIED_TEXT, modified, CF_COL_SIZE, size,
        CF_COL_MODIFIED, stamp, CF_COL_FILE, result->file,
        CF_COL_ICON, g_file_info_get_icon(info), CF_COL_PATH, path,
        CF_COL_ID, g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE),
        CF_COL_CREATED_TEXT, created, CF_COL_ACCESSED_TEXT, accessed, CF_COL_PERMISSIONS_TEXT, permissions,
        CF_COL_CREATED, created_stamp, CF_COL_ACCESSED, accessed_stamp, CF_COL_PERMISSIONS, mode,
        CF_COL_MIME, mime, CF_COL_DETAIL, detail, CF_COL_GROUP, group, CF_COL_OWNER, owner, CF_COL_BYTES_TEXT, bytes, CF_COL_BITS_TEXT, bits, -1);
}
void cf_results_append(GtkListStore *store, const CfResult *result)
{
    GtkTreeIter iter;
    set_result(store, &iter, result, TRUE);
}
void cf_results_replace(GtkListStore *store, GFile *previous, const CfResult *result)
{
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        g_autoptr(GFile) file = NULL;
        gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, -1);
        if (file && g_file_equal(file, previous)) { set_result(store, &iter, result, FALSE); return; }
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

void cf_results_update(GtkListStore *store, GtkTreeIter *iter, const CfResult *result)
{
    set_result(store, iter, result, FALSE);
}

void cf_results_remove(GtkListStore *store, GFile *file)
{
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        g_autoptr(GFile) current = NULL;
        gtk_tree_model_get(model, &iter, CF_COL_FILE, &current, -1);
        if (current && g_file_equal(current, file)) valid = gtk_list_store_remove(store, &iter);
        else valid = gtk_tree_model_iter_next(model, &iter);
    }
}
