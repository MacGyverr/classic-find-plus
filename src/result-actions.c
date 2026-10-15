#include "input-limits.h"
#include "file-time.h"
#include "result-actions.h"
#include "file-operations.h"
#include "results.h"
#include "selection.h"
#include "results-sync.h"
#include "result-names.h"
#include "result-rename.h"

#define INFO "standard::name,standard::type,standard::is-symlink,standard::size,standard::content-type,standard::icon,time::created,time::created-usec,time::access,time::access-usec,unix::mode,unix::uid,unix::gid,owner::user,owner::group,time::modified,time::modified-usec,access::can-read,access::can-write,id::file"
typedef enum { OPEN, OPEN_WITH, FOLDER, COPY, CUT, RENAME, TRASH, PROPERTIES } Action;
typedef struct {
    GWeakRef view;
    GPtrArray *files;
    GCancellable *cancel;
    GAppInfo *app;
    GHashTable *ids;
    Action action;
    guint index, generation;
    const char *access_hint;
    const char *readonly_hint;
    char *name;
    GString *errors;
} Request;

static void request_free(Request *r)
{
    g_weak_ref_clear(&r->view);
    g_ptr_array_unref(r->files);
    g_object_unref(r->cancel);
    g_clear_object(&r->app);
    g_hash_table_unref(r->ids);
    g_free(r->name);
    g_string_free(r->errors, TRUE);
    g_free(r);
}
static Request *request_new(GtkTreeView *view, Action action)
{
    Request *r = g_new0(Request, 1);
    g_weak_ref_init(&r->view, view);
    r->files = cf_selection_files(view);
    r->ids = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, g_free);
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GList *rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(view), NULL);
    for (GList *p = rows; p; p = p->next) {
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, p->data)) {
            GFile *file = NULL; char *id = NULL;
            gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, CF_COL_ID, &id, -1);
            if (file) g_hash_table_insert(r->ids, file, id);
            else g_free(id);
        }
    }
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
    r->cancel = g_object_ref(g_object_get_data(G_OBJECT(view), "cf-actions-cancel"));
    r->action = action;
    r->generation = cf_results_sync_generation(view);
    r->errors = g_string_new(NULL);
    return r;
}
static GtkWindow *parent_for(GtkWidget *view)
{
    GtkWidget *top = gtk_widget_get_toplevel(view);
    return GTK_IS_WINDOW(top) ? GTK_WINDOW(top) : NULL;
}
static void dialog_close(GtkDialog *dialog, int response, gpointer unused)
{
    (void)response; (void)unused;
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void finish(Request *r)
{
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&r->view);
    if (view && r->action == RENAME) {
        g_object_set_data(G_OBJECT(view), "cf-rename-busy", NULL);
        gtk_widget_set_sensitive(GTK_WIDGET(view), TRUE);
        GtkWindow *parent = parent_for(GTK_WIDGET(view));
        if (parent && (!gtk_window_get_focus(parent) || gtk_window_get_focus(parent) == GTK_WIDGET(view)))
            gtk_widget_grab_focus(GTK_WIDGET(view));
    }
    if (view && !g_cancellable_is_cancelled(r->cancel) && r->errors->len) {
        if (r->access_hint) g_string_append_printf(r->errors, "\n%s\n", r->access_hint);
        if (r->readonly_hint) g_string_append_printf(r->errors, "\n%s\n", r->readonly_hint);
        GtkWidget *dialog = gtk_message_dialog_new(parent_for(GTK_WIDGET(view)),
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", "File operation failed");
        gtk_window_set_title(GTK_WINDOW(dialog), "File operation failed");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", r->errors->str);
        g_signal_connect(dialog, "response", G_CALLBACK(dialog_close), NULL);
        gtk_widget_show(dialog);
    }
    request_free(r);
}
static void record_error(Request *r, GError *error)
{
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) return;
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        r->access_hint = cf_file_error_hint(error);
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_READ_ONLY))
        r->readonly_hint = cf_file_error_hint(error);
    if (r->errors->len > 4096) return;
    g_autofree char *path = g_file_get_parse_name(g_ptr_array_index(r->files, r->index));
    g_string_append_printf(r->errors, "%s: %s\n", path, error->message);
}
static void next(Request *r);
static void refreshed(GObject *source, GAsyncResult *result, gpointer data)
{
    Request *r = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFileInfo) info = g_file_query_info_finish(G_FILE(source), result, &error);
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&r->view);
    gboolean current = view && r->generation == cf_results_sync_generation(view) && !g_cancellable_is_cancelled(r->cancel);
    if (current && info) {
        const char *expected = g_hash_table_lookup(r->ids, g_ptr_array_index(r->files, r->index));
        if (g_strcmp0(expected, g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE))) {
            cf_results_remove(GTK_LIST_STORE(gtk_tree_view_get_model(view)), g_ptr_array_index(r->files, r->index));
            g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Rename completed, but the destination changed before it could be refreshed. Search again.");
            record_error(r, error);
        } else {
            CfResult row = { G_FILE(source), info };
            cf_results_sync_renamed(view, g_ptr_array_index(r->files, r->index), &row);
        }
    } else if (!info) {
        /* The rename succeeded: never retain the old pathname after a refresh error. */
        if (current) cf_results_remove(GTK_LIST_STORE(gtk_tree_view_get_model(view)), g_ptr_array_index(r->files, r->index));
        record_error(r, error);
    }
    r->index++;
    next(r);
}
static void mutated(GObject *source, GAsyncResult *result, gpointer data)
{
    Request *r = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = cf_file_operation_finish(result, &error);
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&r->view);
    gboolean current = view && r->generation == cf_results_sync_generation(view) && !g_cancellable_is_cancelled(r->cancel);
    if (file && current && r->action == TRASH)
        cf_results_remove(GTK_LIST_STORE(gtk_tree_view_get_model(view)), G_FILE(source));
    if (file && current && r->action == RENAME) {
        if (!g_file_equal(file, G_FILE(source))) cf_results_sync_invalidate_descendants(view, G_FILE(source));
        g_file_query_info_async(file, INFO, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                               G_PRIORITY_DEFAULT, r->cancel, refreshed, r);
        return;
    }
    if (error) record_error(r, error);
    r->index++;
    next(r);
}
static void launched(GObject *source, GAsyncResult *result, gpointer data)
{
    Request *r = data;
    g_autoptr(GError) error = NULL;
    if (r->app) g_app_info_launch_uris_finish(G_APP_INFO(source), result, &error);
    else g_app_info_launch_default_for_uri_finish(result, &error);
    if (error) record_error(r, error);
    r->index++;
    next(r);
}
static void chooser_response(GtkDialog *dialog, int response, gpointer data)
{
    Request *r = data;
    if (response == GTK_RESPONSE_OK) r->app = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dialog));
    g_object_steal_data(G_OBJECT(dialog), "cf-request");
    gtk_widget_destroy(GTK_WIDGET(dialog));
    if (r->app && !g_cancellable_is_cancelled(r->cancel)) next(r);
    else finish(r);
}
static void properties(GtkTreeView *view, GFile *file, GFileInfo *info)
{
    g_autofree char *path = g_file_get_parse_name(file);
    g_autofree char *size = g_format_size_full(g_file_info_get_size(info), G_FORMAT_SIZE_IEC_UNITS);
    const char *content = g_file_info_get_content_type(info);
    g_autofree char *type = g_file_info_get_is_symlink(info) ? g_strdup("Symbolic link") :
        g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY ? g_strdup("Folder") :
        content ? g_content_type_get_description(content) : g_strdup("File");
    gint64 timestamp;
    g_autoptr(GDateTime) time = cf_file_time(info, G_FILE_ATTRIBUTE_TIME_MODIFIED, &timestamp) ?
        g_date_time_new_from_unix_utc(cf_file_time_seconds(timestamp)) : NULL;
    g_autoptr(GDateTime) local = time ? g_date_time_to_local(time) : NULL;
    g_autofree char *date = local ? g_date_time_format(local, "%Y-%m-%d %H:%M:%S %Z") : g_strdup("Unknown");
    g_autofree char *mode = g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_MODE) ?
        g_strdup_printf("%04o", g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE) & 07777) : g_strdup("Unknown");
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Properties", parent_for(GTK_WIDGET(view)),
        GTK_DIALOG_DESTROY_WITH_PARENT, "_Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    const char *labels[] = { "Location", "Type", "Size", "Modified", "Permissions" };
    const char *values[] = { path, type, size, date, mode };
    for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
        GtkWidget *label = gtk_label_new(labels[i]);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
        label = gtk_label_new(values[i]);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 70);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);
    g_signal_connect(dialog, "response", G_CALLBACK(dialog_close), NULL);
    gtk_widget_show_all(dialog);
}
static void queried(GObject *source, GAsyncResult *result, gpointer data)
{
    Request *r = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFileInfo) info = g_file_query_info_finish(G_FILE(source), result, &error);
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&r->view);
    if (!info) { record_error(r, error); r->index++; next(r); return; }
    if (!view || g_cancellable_is_cancelled(r->cancel)) { finish(r); return; }
    const char *expected = g_hash_table_lookup(r->ids, source);
    if (expected && g_strcmp0(expected, g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILE))) {
        g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "The file has been replaced since the search. Search again.");
        record_error(r, error); r->index++; next(r); return;
    }
    if (r->action == PROPERTIES) {
        properties(view, G_FILE(source), info);
        finish(r);
    } else if (r->action == OPEN_WITH && !r->app) {
        GtkWidget *dialog = gtk_app_chooser_dialog_new(parent_for(GTK_WIDGET(view)),
            GTK_DIALOG_DESTROY_WITH_PARENT, G_FILE(source));
        /* The dialog owns the request until response or parent destruction. */
        g_object_set_data_full(G_OBJECT(dialog), "cf-request", r, (GDestroyNotify)request_free);
        g_signal_connect(dialog, "response", G_CALLBACK(chooser_response), r);
        gtk_widget_show(dialog);
    } else {
        g_autofree char *uri = g_file_get_uri(G_FILE(source));
        g_autoptr(GdkAppLaunchContext) context = gdk_display_get_app_launch_context(gtk_widget_get_display(GTK_WIDGET(view)));
        gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());
        if (r->app) {
            GList uris = { uri, NULL, NULL };
            g_app_info_launch_uris_async(r->app, &uris, G_APP_LAUNCH_CONTEXT(context), r->cancel, launched, r);
        } else g_app_info_launch_default_for_uri_async(uri, G_APP_LAUNCH_CONTEXT(context), r->cancel, launched, r);
    }
}
static void next(Request *r)
{
    if (r->index >= r->files->len || g_cancellable_is_cancelled(r->cancel)) { finish(r); return; }
    GFile *file = g_ptr_array_index(r->files, r->index);
    if (r->action == RENAME || r->action == TRASH)
        cf_file_operation_async(file, g_hash_table_lookup(r->ids, file), r->name, r->cancel, mutated, r);
    else g_file_query_info_async(file, INFO, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                G_PRIORITY_DEFAULT, r->cancel, queried, r);
}
static void mutation_response(GtkDialog *dialog, int response, gpointer data)
{
    Request *r = data;
    g_object_steal_data(G_OBJECT(dialog), "cf-request");
    gtk_widget_destroy(GTK_WIDGET(dialog));
    if (response == GTK_RESPONSE_OK) next(r);
    else finish(r);
}
static void perform(GtkTreeView *view, Action action)
{
    if (action == COPY || action == CUT) { cf_selection_copy(view, action == CUT); return; }
    if (action == RENAME) {
        GList *rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(view), NULL);
        if (rows && !rows->next) cf_result_rename_start(view, rows->data);
        g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
        return;
    }
    Request *r = request_new(view, action);
    if (!r->files->len) { finish(r); return; }
    if ((action == OPEN_WITH || action == PROPERTIES) && r->files->len != 1) { finish(r); return; }
    if (action == FOLDER) {
        GPtrArray *parents = g_ptr_array_new_with_free_func(g_object_unref);
        g_autoptr(GHashTable) seen = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
        for (guint i = 0; i < r->files->len; i++) {
            GFile *parent = g_file_get_parent(g_ptr_array_index(r->files, i));
            if (parent && !g_hash_table_contains(seen, parent)) {
                g_hash_table_add(seen, g_object_ref(parent));
                g_ptr_array_add(parents, parent);
            } else g_clear_object(&parent);
        }
        g_ptr_array_unref(r->files);
        r->files = parents;
    }
    if (action == TRASH) {
        GtkWidget *dialog;
        dialog = gtk_message_dialog_new(parent_for(GTK_WIDGET(view)), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "Move %u selected item%s to Trash?", r->files->len, r->files->len == 1 ? "" : "s");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", "If Trash is unavailable, the item will be left in place. No permanent deletion is performed.");
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL, "Move to _Trash", GTK_RESPONSE_OK, NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
        g_object_set_data_full(G_OBJECT(dialog), "cf-request", r, (GDestroyNotify)request_free);
        g_signal_connect(dialog, "response", G_CALLBACK(mutation_response), r);
        gtk_widget_show_all(dialog);
    } else next(r);
}
static void activate(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer unused)
{
    (void)column; (void)unused;
    /* Activation targets the activated row, irrespective of other selected rows. */
    Request *r = request_new(view, OPEN);
    g_ptr_array_set_size(r->files, 0);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(gtk_tree_view_get_model(view), &iter, path)) {
        GFile *file; char *id;
        gtk_tree_model_get(gtk_tree_view_get_model(view), &iter, CF_COL_FILE, &file, CF_COL_ID, &id, -1);
        if (file) {
            g_hash_table_replace(r->ids, g_object_ref(file), id);
            g_ptr_array_add(r->files, file);
        } else g_free(id);
    }
    next(r);
}
static void menu_activate(GtkMenuItem *item, gpointer data)
{
    GtkTreeView *view = data;
    Action action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "cf-action"));
    if (action == RENAME) {
        GtkTreeRowReference *row = g_object_get_data(G_OBJECT(item), "cf-target-row");
        g_autoptr(GtkTreePath) path = row ? gtk_tree_row_reference_get_path(row) : NULL;
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(view);
        if (path && gtk_tree_row_reference_get_model(row) == model && gtk_tree_model_get_iter(model, &iter, path)) {
            g_autoptr(GFile) file = NULL; g_autofree char *id = NULL;
            gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, CF_COL_ID, &id, -1);
            GFile *expected = g_object_get_data(G_OBJECT(item), "cf-target-file");
            const char *identity = g_object_get_data(G_OBJECT(item), "cf-target-id");
            if (file && expected && g_file_equal(file, expected) && !g_strcmp0(id, identity))
                cf_result_rename_start(view, path);
        }
    } else perform(view, action);
}
static void menu_detach(GtkWidget *view, GtkMenu *menu)
{
    (void)view;
    gtk_widget_destroy(GTK_WIDGET(menu));
}
static void copy_name(GtkMenuItem *item, gpointer data)
{
    const char *text = g_object_get_data(G_OBJECT(item), "cf-name-text");
    if (!text) return;
    GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(data), GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    gtk_clipboard_set_can_store(clipboard, NULL, 0);
}
static char *selected_text(GtkTreeView *view, GList *rows, guint mode, GError **error)
{
    g_autoptr(GString) text = g_string_new(NULL);
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    for (GList *p = rows; p; p = p->next) {
        GtkTreeIter iter;
        g_autoptr(GFile) file = NULL;
        g_autofree char *type = NULL, *value = NULL;
        if (gtk_tree_model_get_iter(model, &iter, p->data))
            gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, CF_COL_TYPE, &type, -1);
        if (!file) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "A selected result is no longer available.");
            return NULL;
        }
        if (mode == 2 && !g_file_is_native(file)) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "A selected result has no native filesystem path.");
            return NULL;
        }
        value = mode == 2 ? cf_result_path(file, error) : cf_result_name(file, error);
        if (!value) return NULL; /* Never copy a partial or lossy selection. */
        if (mode == 0) value[g_utf8_offset_to_pointer(value,
            cf_result_stem_length(value, !g_strcmp0(type, "Folder"))) - value] = 0;
        if (p != rows) g_string_append_c(text, '\n');
        g_string_append(text, value);
    }
    return rows ? g_string_free(g_steal_pointer(&text), FALSE) : NULL;
}
static void name_items(GtkWidget *menu, GtkTreeView *view, gboolean has_target)
{
    /* GTK returns selected paths in current model/display order, including wrappers.
     * Own the text at menu invocation, independent of later selection/model changes. */
    GList *rows = has_target ? gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(view), NULL) : NULL;
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    const char *labels[] = { "Copy Name to Clipboard", "Copy Name.EXT to Clipboard", "Copy Full Path to Clipboard" };
    for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
        GtkWidget *item = gtk_menu_item_new_with_label(labels[i]);
        g_autoptr(GError) error = NULL;
        char *text = selected_text(view, rows, i, &error);
        g_object_set_data_full(G_OBJECT(item), "cf-name-text", text, g_free);
        gtk_widget_set_sensitive(item, text != NULL);
        if (error) gtk_widget_set_tooltip_text(item, error->message);
        g_signal_connect(item, "activate", G_CALLBACK(copy_name), view);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
}
static void popup(GtkTreeView *view, GdkEvent *event, GtkTreePath *target)
{
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(view), menu_detach);
    const char *names[] = { "_Open", "Open _With…", "Open Containing _Folder", "_Copy", "Cu_t", "_Rename…", "Move to _Trash…", "_Properties" };
    gint count = gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(view));
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        GtkWidget *item = gtk_menu_item_new_with_mnemonic(names[i]);
        gtk_widget_set_sensitive(item, count > 0 && ((i != OPEN_WITH && i != RENAME && i != PROPERTIES) || count == 1));
        if (i == RENAME) {
            GtkTreeModel *model = gtk_tree_view_get_model(view);
            GtkTreeIter iter;
            gboolean valid = target && gtk_tree_model_get_iter(model, &iter, target);
            gtk_widget_set_sensitive(item, valid && !g_object_get_data(G_OBJECT(view), "cf-search-active") &&
                !g_object_get_data(G_OBJECT(view), "cf-rename-busy"));
            if (valid) {
                GFile *file = NULL; char *id = NULL;
                gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, CF_COL_ID, &id, -1);
                g_object_set_data_full(G_OBJECT(item), "cf-target-file", file, g_object_unref);
                g_object_set_data_full(G_OBJECT(item), "cf-target-id", id, g_free);
                g_object_set_data_full(G_OBJECT(item), "cf-target-row", gtk_tree_row_reference_new(model, target),
                                       (GDestroyNotify)gtk_tree_row_reference_free);
            }
        }
        g_object_set_data(G_OBJECT(item), "cf-action", GINT_TO_POINTER(i));
        g_signal_connect(item, "activate", G_CALLBACK(menu_activate), view);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    name_items(menu, view, target != NULL || event == NULL);
    g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
    gtk_widget_show_all(menu);
    if (event) gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
    else gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(view), GDK_GRAVITY_CENTER, GDK_GRAVITY_NORTH_WEST, NULL);
}
static gboolean button(GtkWidget *widget, GdkEventButton *event, gpointer unused)
{
    (void)unused;
    if (event->button != 3 || event->type != GDK_BUTTON_PRESS) return FALSE;
    GtkTreeView *view = GTK_TREE_VIEW(widget);
    GtkTreePath *path = NULL;
    gtk_tree_view_get_path_at_pos(view, event->x, event->y, &path, NULL, NULL, NULL);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    if (path && !gtk_tree_selection_path_is_selected(selection, path)) {
        gtk_tree_selection_unselect_all(selection);
        gtk_tree_selection_select_path(selection, path);
    }
    gtk_widget_grab_focus(widget);
    popup(view, (GdkEvent *)event, path);
    if (path) gtk_tree_path_free(path);
    return TRUE;
}
static gboolean keyboard_popup(GtkWidget *view, gpointer unused)
{
    (void)unused;
    GList *rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), NULL);
    popup(GTK_TREE_VIEW(view), NULL, rows && !rows->next ? rows->data : NULL);
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
    return TRUE;
}
static gboolean key(GtkWidget *widget, GdkEventKey *event, gpointer unused)
{
    (void)unused;
    guint keyval = gdk_keyval_to_lower(event->keyval);
    GdkModifierType modifiers = event->state & gtk_accelerator_get_default_mod_mask();
    GtkTreeView *view = GTK_TREE_VIEW(widget);
    if (modifiers == GDK_CONTROL_MASK && keyval == GDK_KEY_a) {
        gtk_tree_selection_select_all(gtk_tree_view_get_selection(view)); return TRUE;
    }
    if (modifiers == GDK_CONTROL_MASK && (keyval == GDK_KEY_c || keyval == GDK_KEY_x)) {
        perform(view, keyval == GDK_KEY_c ? COPY : CUT); return TRUE;
    }
    if (!modifiers && (keyval == GDK_KEY_F2 || keyval == GDK_KEY_Delete || keyval == GDK_KEY_KP_Delete)) {
        perform(view, keyval == GDK_KEY_F2 ? RENAME : TRASH); return TRUE;
    }
    return FALSE;
}
static void rename_commit(GtkTreeView *view, GFile *file, const char *id, const char *name)
{
    Request *r = request_new(view, RENAME);
    g_ptr_array_set_size(r->files, 0);
    g_hash_table_remove_all(r->ids);
    g_ptr_array_add(r->files, g_object_ref(file));
    g_hash_table_insert(r->ids, g_object_ref(file), g_strdup(id));
    r->name = g_strdup(name);
    g_object_set_data(G_OBJECT(view), "cf-rename-busy", GINT_TO_POINTER(1));
    gtk_widget_set_sensitive(GTK_WIDGET(view), FALSE);
    next(r);
}
static void cancel(GtkWidget *view, gpointer data)
{
    (void)view;
    g_cancellable_cancel(data);
}
void cf_result_actions_attach(GtkTreeView *view)
{
    GCancellable *cancellable = g_cancellable_new();
    g_object_set_data_full(G_OBJECT(view), "cf-actions-cancel", cancellable, g_object_unref);
    g_signal_connect(view, "destroy", G_CALLBACK(cancel), cancellable);
    cf_result_rename_attach(view, rename_commit);
    g_signal_connect(view, "row-activated", G_CALLBACK(activate), NULL);
    g_signal_connect(view, "button-press-event", G_CALLBACK(button), NULL);
    g_signal_connect(view, "popup-menu", G_CALLBACK(keyboard_popup), NULL);
    g_signal_connect(view, "key-press-event", G_CALLBACK(key), NULL);
}
