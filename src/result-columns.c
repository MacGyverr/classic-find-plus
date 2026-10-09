#include "result-columns.h"
#include "column-preferences.h"
#include "results.h"

typedef struct {
    GtkTreeView *view;
    GtkTreeViewColumn *columns[CF_RESULT_COLUMN_COUNT];
    CfColumnPreferences preferences;
    CfSettings *settings;
    gboolean destroying;
} Columns;

static const char *const names[] = { "Name", "In Folder", "Size", "Type", "Modified", "Created", "Accessed", "Permissions", "Full Path", "MIME Type", "Detailed Type", "Group", "Owner", "Bytes", "Bits" };
static const int text_ids[] = { CF_COL_NAME, CF_COL_FOLDER, CF_COL_SIZE_TEXT, CF_COL_TYPE, CF_COL_MODIFIED_TEXT,
    CF_COL_CREATED_TEXT, CF_COL_ACCESSED_TEXT, CF_COL_PERMISSIONS_TEXT, CF_COL_PATH, CF_COL_MIME, CF_COL_DETAIL, CF_COL_GROUP, CF_COL_OWNER, CF_COL_BYTES_TEXT, CF_COL_BITS_TEXT };
static const int sort_ids[] = { CF_COL_NAME, CF_COL_FOLDER, CF_COL_SIZE, CF_COL_TYPE, CF_COL_MODIFIED,
    CF_COL_CREATED, CF_COL_ACCESSED, CF_COL_PERMISSIONS, CF_COL_PATH, CF_COL_MIME, CF_COL_DETAIL, CF_COL_GROUP, CF_COL_OWNER, CF_COL_BYTES_TEXT, CF_COL_BITS_TEXT };

static gint exact_size_sort(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer unused)
{
    (void)unused;
    guint64 x, y;
    gtk_tree_model_get(model, a, CF_COL_SIZE, &x, -1);
    gtk_tree_model_get(model, b, CF_COL_SIZE, &y, -1);
    return (x > y) - (x < y);
}

/* Owner/group names collate naturally; two numeric fallbacks sort numerically. */
static gint ownership_sort(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
    g_autofree char *left = NULL, *right = NULL;
    int column = GPOINTER_TO_INT(data);
    gtk_tree_model_get(model, a, column, &left, -1);
    gtk_tree_model_get(model, b, column, &right, -1);
    if (!left) left = g_strdup("");
    if (!right) right = g_strdup("");
    if (*left && *right && strspn(left, "0123456789") == strlen(left) &&
        strspn(right, "0123456789") == strlen(right)) {
        guint64 x = g_ascii_strtoull(left, NULL, 10), y = g_ascii_strtoull(right, NULL, 10);
        return (x > y) - (x < y);
    }
    return g_utf8_collate(left, right);
}

static void reordered(GtkTreeView *view, gpointer data)
{
    Columns *c = data;
    GList *columns = gtk_tree_view_get_columns(view);
    int position = 0;
    for (GList *p = columns; p; p = p->next) for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++)
        if (p->data == c->columns[i]) c->preferences.order[position++] = i;
    g_list_free(columns);
    if (!c->destroying) {
        const char *order[CF_RESULT_COLUMN_COUNT];
        for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) order[i] = cf_column_ids[c->preferences.order[i]];
        cf_settings_set_strings(c->settings, "Columns", "Order", order, CF_RESULT_COLUMN_COUNT);
    }
}

static void sorted(GtkTreeSortable *model, gpointer data)
{
    Columns *c = data;
    int column;
    GtkSortType direction;
    c->preferences.sort = -1;
    if (gtk_tree_sortable_get_sort_column_id(model, &column, &direction)) {
        for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) if (column == sort_ids[i]) c->preferences.sort = i;
        c->preferences.direction = direction;
    }
    if (!c->destroying) {
        cf_settings_set_string(c->settings, "Columns", "Sort", c->preferences.sort >= 0 ? cf_column_ids[c->preferences.sort] : "none");
        cf_settings_set_int(c->settings, "Columns", "Direction", c->preferences.direction);
    }
}

static void visibility(GObject *column, GParamSpec *spec, gpointer data)
{
    (void)spec;
    Columns *c = data;
    if (c->destroying) return;
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) if (G_OBJECT(c->columns[i]) == column) {
        gboolean visible = gtk_tree_view_column_get_visible(c->columns[i]);
        if (!i && !visible) { gtk_tree_view_column_set_visible(c->columns[i], TRUE); return; }
        if (visible == c->preferences.visible[i]) return;
        c->preferences.visible[i] = visible;
        g_autofree char *key = g_strdup_printf("Visible_%s", cf_column_ids[i]);
        cf_settings_set_bool(c->settings, "Columns", key, visible);
    }
}
static void resized(GObject *column, GParamSpec *spec, gpointer data)
{
    (void)spec;
    Columns *c = data;
    if (c->destroying) return;
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) if (G_OBJECT(c->columns[i]) == column) {
        int width = gtk_tree_view_column_get_fixed_width(c->columns[i]);
        if (width < 40 || width > 4096 || width == c->preferences.widths[i]) return;
        c->preferences.widths[i] = width;
        gtk_tree_view_column_set_expand(c->columns[i], FALSE);
        gtk_tree_view_column_set_sizing(c->columns[i], GTK_TREE_VIEW_COLUMN_FIXED);
        g_autofree char *key = g_strdup_printf("Width_%s", cf_column_ids[i]);
        cf_settings_set_int(c->settings, "Columns", key, width);
    }
}
static void toggled(GtkCheckMenuItem *item, gpointer data)
{
    Columns *c = data;
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "column-index"));
    if (!index) { gtk_check_menu_item_set_active(item, TRUE); return; }
    gtk_tree_view_column_set_visible(c->columns[index], gtk_check_menu_item_get_active(item));
}

GtkWidget *cf_result_columns_menu(GtkTreeView *view)
{
    Columns *c = g_object_get_data(G_OBJECT(view), "result-columns");
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(view), NULL);
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        GtkWidget *item = gtk_check_menu_item_new_with_label(names[i]);
        g_object_set_data(G_OBJECT(item), "column-index", GINT_TO_POINTER(i));
        gboolean active = gtk_tree_view_column_get_visible(c->columns[i]);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
        gtk_widget_set_sensitive(item, i != 0);
        g_signal_connect(item, "toggled", G_CALLBACK(toggled), c);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
    gtk_widget_show_all(menu);
    return menu;
}

static gboolean header_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    (void)widget;
    Columns *c = data;
    if (event->button != GDK_BUTTON_SECONDARY) return FALSE;
    GtkWidget *menu = cf_result_columns_menu(c->view);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

static void destroy(GtkWidget *view, gpointer data)
{
    Columns *c = data;
    if (c->destroying) return;
    c->destroying = TRUE;
    g_signal_handlers_disconnect_by_data(view, c);
    cf_settings_free(c->settings); c->settings = NULL;
    g_signal_handlers_disconnect_by_data(gtk_tree_view_get_model(c->view), c);
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        g_signal_handlers_disconnect_by_data(c->columns[i], c);
        g_signal_handlers_disconnect_by_data(gtk_tree_view_column_get_button(c->columns[i]), c);
    }
    /* Menus and callbacks cannot outlive their view/controller. */
    GList *menus = g_list_copy(gtk_menu_get_for_attach_widget(view));
    for (GList *p = menus; p; p = p->next) gtk_widget_destroy(p->data);
    g_list_free(menus);
}

void cf_result_columns_attach(GtkTreeView *view)
{
    Columns *c = g_new0(Columns, 1);
    c->view = view;
    c->settings = cf_settings_new();
    cf_column_preferences_read(c->settings, &c->preferences);
    g_object_set_data_full(G_OBJECT(view), "result-columns", c, g_free);
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        GtkTreeViewColumn *column = gtk_tree_view_column_new();
        c->columns[i] = column;
        g_object_set_data(G_OBJECT(column), "cf-column-index", GINT_TO_POINTER(i + 1));
        gtk_tree_view_column_set_title(column, names[i]);
        if (i == 0) {
            GtkCellRenderer *icon = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start(column, icon, FALSE);
            gtk_tree_view_column_add_attribute(column, icon, "gicon", CF_COL_ICON);
        }
        GtkCellRenderer *text = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, text, TRUE);
        gtk_tree_view_column_add_attribute(column, text, "text", text_ids[i]);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_reorderable(column, TRUE);
        gtk_tree_view_column_set_expand(column, c->preferences.widths[i] < 0 && (i == 0 || i == 1));
        if (c->preferences.widths[i] >= 40) {
            gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_fixed_width(column, c->preferences.widths[i]);
        }
        gtk_tree_view_column_set_sort_column_id(column, sort_ids[i]);
        gtk_tree_view_column_set_visible(column, c->preferences.visible[i]);
    }
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) gtk_tree_view_append_column(view, c->columns[c->preferences.order[i]]);
    GtkTreeSortable *model = GTK_TREE_SORTABLE(gtk_tree_view_get_model(view));
    gtk_tree_sortable_set_sort_func(model, CF_COL_BYTES_TEXT, exact_size_sort, NULL, NULL);
    gtk_tree_sortable_set_sort_func(model, CF_COL_BITS_TEXT, exact_size_sort, NULL, NULL);
    gtk_tree_sortable_set_sort_func(model, CF_COL_OWNER, ownership_sort, GINT_TO_POINTER(CF_COL_OWNER), NULL);
    gtk_tree_sortable_set_sort_func(model, CF_COL_GROUP, ownership_sort, GINT_TO_POINTER(CF_COL_GROUP), NULL);
    if (c->preferences.sort >= 0) gtk_tree_sortable_set_sort_column_id(model,
        sort_ids[c->preferences.sort], c->preferences.direction);
    for (int i = 0; i < CF_RESULT_COLUMN_COUNT; i++) {
        g_signal_connect(c->columns[i], "notify::visible", G_CALLBACK(visibility), c);
        g_signal_connect(c->columns[i], "notify::fixed-width", G_CALLBACK(resized), c);
        GtkWidget *button = gtk_tree_view_column_get_button(c->columns[i]);
        g_signal_connect(button, "button-press-event", G_CALLBACK(header_press), c);
    }
    g_signal_connect(view, "columns-changed", G_CALLBACK(reordered), c);
    g_signal_connect(model, "sort-column-changed", G_CALLBACK(sorted), c);
    /* Model can outlive the view, so disconnect its controller callback on destruction. */
    g_signal_connect(view, "destroy", G_CALLBACK(destroy), c);
}

const char *cf_result_column_display_id(GtkTreeViewColumn *column, int *model_id)
{
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(column), "cf-column-index")) - 1;
    if (index < 0 || index >= CF_RESULT_COLUMN_COUNT) return NULL;
    if (model_id) *model_id = text_ids[index];
    return cf_column_ids[index];
}
