#include "column-preferences.h"

const char *const cf_column_ids[] = { "name", "folder", "size", "type", "modified", "created", "accessed", "permissions", "path", "mime", "detailed-type", "group", "owner", "bytes", "bits" };
static int index_of(const char *id)
{
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) if (!g_strcmp0(id, cf_column_ids[i])) return i;
    return -1;
}
void cf_column_preferences_read(CfSettings *settings, CfColumnPreferences *p)
{
    *p = (CfColumnPreferences){ .sort = -1, .direction = GTK_SORT_ASCENDING };
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        p->order[i] = i; p->visible[i] = i < 5; p->widths[i] = -1;
    }
    GKeyFile *file = cf_settings_data(settings);
    gsize count = 0;
    g_auto(GStrv) ids = g_key_file_get_string_list(file, "Columns", "Order", &count, NULL);
    int order[CF_RESULT_COLUMN_COUNT];
    gboolean seen[CF_RESULT_COLUMN_COUNT] = { FALSE };
    gboolean valid = ids && (count == CF_RESULT_COLUMN_COUNT || count == 13 || count == 9);
    for (gsize i = 0; valid && i < count; i++) {
        int index = index_of(ids[i]);
        /* Backward compatibility with the original stable numeric IDs. */
        if (index < 0 && strlen(ids[i]) == 1 && ids[i][0] >= '0' && ids[i][0] <= '8') index = ids[i][0] - '0';
        if (index < 0 || ((gsize)index >= count) || seen[index]) valid = FALSE;
        else { seen[index] = TRUE; order[i] = index; }
    }
    if (valid) for (gsize i = 0; i < count; i++) p->order[i] = order[i];
    g_autofree gboolean *legacy = g_key_file_get_boolean_list(file, "Columns", "Visible", &count, NULL);
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        if (legacy && (count == CF_RESULT_COLUMN_COUNT || count == 13 || count == 9) && (gsize)i < count) p->visible[i] = legacy[i];
        g_autofree char *visible = g_strdup_printf("Visible_%s", cf_column_ids[i]);
        p->visible[i] = cf_settings_bool(settings, "Columns", visible, p->visible[i]);
        g_autofree char *width = g_strdup_printf("Width_%s", cf_column_ids[i]);
        p->widths[i] = cf_settings_int(settings, "Columns", width, -1, 40, 4096);
    }
    p->visible[0] = TRUE;
    g_autofree char *sort = g_key_file_get_string(file, "Columns", "Sort", NULL);
    p->sort = index_of(sort);
    if (p->sort < 0) p->sort = cf_settings_int(settings, "Columns", "Sort", -1, -1, CF_RESULT_COLUMN_COUNT - 1);
    p->direction = cf_settings_int(settings, "Columns", "Direction", GTK_SORT_ASCENDING, GTK_SORT_ASCENDING, GTK_SORT_DESCENDING);
}
void cf_column_preferences_load(CfColumnPreferences *p)
{
    g_autoptr(CfSettings) settings = cf_settings_new(); cf_column_preferences_read(settings, p);
}
void cf_column_preferences_save(const CfColumnPreferences *p, GError **error)
{
    g_autoptr(CfSettings) settings = cf_settings_new();
    const char *order[CF_RESULT_COLUMN_COUNT];
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        order[i] = cf_column_ids[p->order[i]];
        g_autofree char *visible = g_strdup_printf("Visible_%s", cf_column_ids[i]);
        g_autofree char *width = g_strdup_printf("Width_%s", cf_column_ids[i]);
        cf_settings_set_bool(settings, "Columns", visible, i == 0 || p->visible[i]);
        cf_settings_set_int(settings, "Columns", width, p->widths[i]);
    }
    cf_settings_set_strings(settings, "Columns", "Order", order, CF_RESULT_COLUMN_COUNT);
    cf_settings_set_string(settings, "Columns", "Sort", p->sort >= 0 ? cf_column_ids[p->sort] : "none");
    cf_settings_set_int(settings, "Columns", "Direction", p->direction);
    cf_settings_flush(settings, error);
}
