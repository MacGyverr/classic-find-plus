#include "result-export.h"
#include "result-columns.h"

struct _CfExportSnapshot {
    gint refs;
    GPtrArray *records;
    guint columns;
};

CfExportSnapshot *cf_export_snapshot_ref(CfExportSnapshot *s)
{
    g_atomic_int_inc(&s->refs);
    return s;
}
void cf_export_snapshot_unref(CfExportSnapshot *s)
{
    if (s && g_atomic_int_dec_and_test(&s->refs)) {
        g_ptr_array_unref(s->records);
        g_free(s);
    }
}
guint cf_export_snapshot_rows(const CfExportSnapshot *s) { return s->records->len - 1; }
guint cf_export_snapshot_columns(const CfExportSnapshot *s) { return s->columns; }

CfExportSnapshot *cf_export_snapshot_new(GtkTreeView *view)
{
    CfExportSnapshot *s = g_new0(CfExportSnapshot, 1);
    s->refs = 1;
    s->records = g_ptr_array_new_with_free_func((GDestroyNotify)g_ptr_array_unref);
    g_autoptr(GArray) ids = g_array_new(FALSE, FALSE, sizeof(int));
    GPtrArray *headings = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(s->records, headings);
    GList *columns = gtk_tree_view_get_columns(view);
    for (GList *p = columns; p; p = p->next) {
        int id;
        if (!gtk_tree_view_column_get_visible(p->data) || !cf_result_column_display_id(p->data, &id)) continue;
        g_array_append_val(ids, id);
        const char *title = gtk_tree_view_column_get_title(p->data);
        g_ptr_array_add(headings, g_utf8_make_valid(title ? title : "", -1));
    }
    g_list_free(columns);
    s->columns = ids->len;
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    gboolean valid = model && gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        GPtrArray *row = g_ptr_array_new_with_free_func(g_free);
        for (guint i = 0; i < ids->len; i++) {
            char *value = NULL;
            gtk_tree_model_get(model, &iter, g_array_index(ids, int, i), &value, -1);
            if (!value) value = g_strdup("");
            else if (!g_utf8_validate(value, -1, NULL)) {
                char *fixed = g_utf8_make_valid(value, -1);
                g_free(value); value = fixed;
            }
            g_ptr_array_add(row, value);
        }
        g_ptr_array_add(s->records, row);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    return s;
}

/* Apostrophe escapes spreadsheet formulas, including whitespace/control/BOM prefixes.
 * Leading tab/CR/LF is also escaped (spreadsheet command prefixes). */
static gboolean formula(const char *value)
{
    if (*value == '\t' || *value == '\r' || *value == '\n') return TRUE;
    const char *p = value;
    while (*p) {
        gunichar c = g_utf8_get_char(p);
        if (!g_unichar_isspace(c) && !g_unichar_iscntrl(c) && c != 0xfeff) break;
        p = g_utf8_next_char(p);
    }
    return *p && strchr("=+-@", *p);
}

static void field(GString *record, const char *value, CfExportFormat format)
{
    if (format == CF_EXPORT_CSV) g_string_append_c(record, '"');
    if (formula(value)) g_string_append_c(record, '\'');
    for (const char *p = value; *p; p++) {
        if (format == CF_EXPORT_TEXT && (*p == '\t' || *p == '\r' || *p == '\n')) g_string_append_c(record, ' ');
        else {
            g_string_append_c(record, *p);
            if (format == CF_EXPORT_CSV && *p == '"') g_string_append_c(record, '"');
        }
    }
    if (format == CF_EXPORT_CSV) g_string_append_c(record, '"');
}

gboolean cf_export_serialize(const CfExportSnapshot *s, CfExportFormat format,
                             GOutputStream *output, GCancellable *cancel, GError **error)
{
    g_autoptr(GString) record = g_string_new(NULL);
    for (guint r = 0; r < s->records->len; r++) {
        if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
        GPtrArray *row = g_ptr_array_index(s->records, r);
        g_string_truncate(record, 0);
        for (guint c = 0; c < row->len; c++) {
            if (c) g_string_append_c(record, format == CF_EXPORT_CSV ? ',' : '\t');
            field(record, g_ptr_array_index(row, c), format);
        }
        g_string_append(record, "\r\n");
        if (!g_output_stream_write_all(output, record->str, record->len, NULL, cancel, error)) return FALSE;
    }
    return TRUE;
}

char *cf_export_filename(const char *name, CfExportFormat format)
{
    if (!name || !*name) name = "Classic Find Plus Results";
    gsize length = strlen(name);
    /* Strip recognized suffixes repeatedly to avoid .csv.txt chains. */
    while (length >= 4 && (!g_ascii_strncasecmp(name + length - 4, ".csv", 4) ||
                           !g_ascii_strncasecmp(name + length - 4, ".txt", 4))) length -= 4;
    g_autofree char *base = g_strndup(name, length);
    return g_strconcat(base, format == CF_EXPORT_CSV ? ".csv" : ".txt", NULL);
}
