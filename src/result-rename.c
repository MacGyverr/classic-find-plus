#include "result-rename.h"
#include "result-names.h"
#include "results.h"
#include "results-sync.h"
#include "file-operations.h"
#include "input-limits.h"

#define KEY "cf-result-rename"
typedef struct {
    GtkTreeView *view;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    CfRenameCommit commit;
    GtkTreeModel *model;
    GtkTreeRowReference *row, *click, *pressed;
    GFile *file;
    char *id, *name;
    GtkWidget *entry;
    guint generation, idle;
    gint64 clicked_at, pressed_at;
    double x, y;
    gboolean accept, slow, closed;
} Rename;
static void clear_ref(GtkTreeRowReference **ref)
{
    if (*ref) gtk_tree_row_reference_free(*ref);
    *ref = NULL;
}
static void gesture_clear(Rename *r)
{
    clear_ref(&r->click); clear_ref(&r->pressed);
    r->slow = FALSE;
    if (r->idle) { g_source_remove(r->idle); r->idle = 0; }
}
static void editor_clear(Rename *r)
{
    if (r->entry) {
        g_signal_handlers_disconnect_by_data(r->entry, r);
        g_object_remove_weak_pointer(G_OBJECT(r->entry), (gpointer *)&r->entry);
    }
    r->entry = NULL; r->accept = FALSE;
    clear_ref(&r->row); g_clear_object(&r->file);
    g_clear_pointer(&r->id, g_free); g_clear_pointer(&r->name, g_free);
    g_object_set(r->renderer, "editable", FALSE, NULL);
}
void cf_result_rename_cancel(GtkTreeView *view)
{
    Rename *r = g_object_get_data(G_OBJECT(view), KEY);
    if (!r) return;
    gesture_clear(r);
    g_autoptr(GtkWidget) entry = r->entry ? g_object_ref(r->entry) : NULL;
    gtk_cell_renderer_stop_editing(r->renderer, TRUE);
    if (entry) gtk_cell_editable_remove_widget(GTK_CELL_EDITABLE(entry));
    editor_clear(r);
}
static void close_error(GtkDialog *dialog, int response, gpointer data)
{
    (void)response; (void)data; gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void explain(Rename *r, const char *message)
{
    GtkWidget *top = gtk_widget_get_toplevel(GTK_WIDGET(r->view));
    GtkWidget *dialog = gtk_message_dialog_new(GTK_IS_WINDOW(top) ? GTK_WINDOW(top) : NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", "Cannot rename this result");
    gtk_window_set_title(GTK_WINDOW(dialog), "Cannot rename this result");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
    g_signal_connect(dialog, "response", G_CALLBACK(close_error), NULL);
    gtk_widget_show(dialog);
}
static gboolean entry_key(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
    (void)entry;
    Rename *r = data;
    r->accept = event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter;
    return FALSE;
}
static gboolean focus_out(GtkWidget *entry, GdkEventFocus *event, gpointer data)
{
    (void)entry; (void)event;
    ((Rename *)data)->accept = FALSE;
    return FALSE;
}
static void editing_started(GtkCellRenderer *renderer, GtkCellEditable *editable, const char *path, gpointer data)
{
    (void)renderer; (void)path;
    Rename *r = data;
    if (!r->row || !GTK_IS_ENTRY(editable)) return;
    r->entry = GTK_WIDGET(editable);
    g_object_add_weak_pointer(G_OBJECT(editable), (gpointer *)&r->entry);
    gtk_entry_set_text(GTK_ENTRY(editable), r->name);
    gtk_entry_set_max_length(GTK_ENTRY(editable), CF_TEXT_LIMIT);
    g_signal_connect(editable, "key-press-event", G_CALLBACK(entry_key), r);
    g_signal_connect(editable, "focus-out-event", G_CALLBACK(focus_out), r);
}
static void editing_cancelled(GtkCellRenderer *renderer, gpointer data)
{
    (void)renderer;
    editor_clear(data);
}
static void edited(GtkCellRendererText *renderer, const char *path, const char *name, gpointer data)
{
    (void)renderer; (void)path;
    Rename *r = data;
    /* Opening/copying/cancelling an editor and accepting unchanged text never dispatch
     * a filesystem task, even when the original entry is inaccessible or missing. */
    if (!r->accept || !r->name || !strcmp(name, r->name)) { editor_clear(r); return; }
    GtkTreeIter iter;
    g_autoptr(GtkTreePath) current = r->row ? gtk_tree_row_reference_get_path(r->row) : NULL;
    gboolean valid = r->accept && current && !r->closed &&
        r->generation == cf_results_sync_generation(r->view) &&
        !g_object_get_data(G_OBJECT(r->view), "cf-search-active") &&
        !g_object_get_data(G_OBJECT(r->view), "cf-rename-busy") &&
        gtk_tree_view_get_model(r->view) == r->model && gtk_tree_model_get_iter(r->model, &iter, current);
    g_autoptr(GFile) file = NULL;
    g_autofree char *id = NULL;
    if (valid) {
        gtk_tree_model_get(r->model, &iter, CF_COL_FILE, &file, CF_COL_ID, &id, -1);
        valid = file && g_file_equal(file, r->file) && !g_strcmp0(id, r->id);
    }
    if (valid && !id) {
        explain(r, "A stable file identity is unavailable. Search again before changing this item.");
        editor_clear(r); return;
    }
    if (valid && cf_file_name_valid(name)) {
        /* Own strings across editor teardown and potential synchronous signals. */
        g_autofree char *new_name = g_strdup(name);
        editor_clear(r);
        r->commit(r->view, file, id, new_name);
    } else {
        if (valid) explain(r, "Enter a nonempty filename that is not . or .. and contains no slash. The filename must be valid text.");
        editor_clear(r);
    }
}
void cf_result_rename_start(GtkTreeView *view, GtkTreePath *path)
{
    Rename *r = g_object_get_data(G_OBJECT(view), KEY);
    if (!r || r->closed || r->row || !path ||
        g_object_get_data(G_OBJECT(view), "cf-search-active") ||
        g_object_get_data(G_OBJECT(view), "cf-rename-busy")) return;
    gesture_clear(r);
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    if (model != r->model || !gtk_tree_model_get_iter(model, &iter, path)) return;
    g_autofree char *type = NULL;
    gtk_tree_model_get(model, &iter, CF_COL_FILE, &r->file, CF_COL_ID, &r->id, CF_COL_TYPE, &type, -1);
    g_autoptr(GError) error = NULL;
    r->name = r->file ? cf_result_name(r->file, &error) : NULL;
    if (!r->name) {
        explain(r, error ? error->message : "A stable file identity is unavailable. Search again before renaming this item.");
        editor_clear(r); return;
    }
    r->row = gtk_tree_row_reference_new(model, path);
    r->generation = cf_results_sync_generation(view);
    gtk_tree_view_scroll_to_cell(view, path, r->column, FALSE, 0, 0);
    gtk_widget_grab_focus(GTK_WIDGET(view));
    g_object_set(r->renderer, "editable", TRUE, NULL);
    gtk_tree_view_set_cursor_on_cell(view, path, r->column, r->renderer, TRUE);
    /* GTK's initial editing setup selects all; override after set_cursor returns. */
    if (r->entry) gtk_editable_select_region(GTK_EDITABLE(r->entry), 0,
        cf_result_stem_length(r->name, !g_strcmp0(type, "Folder")));
    else editor_clear(r);
    g_object_set(r->renderer, "editable", FALSE, NULL);
}
static gboolean press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    Rename *r = data;
    GtkTreePath *path = NULL; GtkTreeViewColumn *column = NULL;
    if (r->row || event->button != 1 || event->type != GDK_BUTTON_PRESS ||
        event->state & gtk_accelerator_get_default_mod_mask() ||
        !gtk_tree_view_get_path_at_pos(r->view, event->x, event->y, &path, &column, NULL, NULL) || column != r->column) {
        if (path) gtk_tree_path_free(path);
        gesture_clear(r); return FALSE;
    }
    clear_ref(&r->pressed);
    gint threshold = 250;
    g_object_get(gtk_widget_get_settings(widget), "gtk-double-click-time", &threshold, NULL);
    gint64 now = g_get_monotonic_time(), elapsed = now - r->clicked_at;
    g_autoptr(GtkTreePath) previous = r->click ? gtk_tree_row_reference_get_path(r->click) : NULL;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(r->view);
    r->slow = previous && !gtk_tree_path_compare(previous, path) && threshold < 500 &&
        elapsed > (gint64)threshold * 1000 && elapsed <= 500000 &&
        gtk_tree_selection_count_selected_rows(selection) == 1 &&
        gtk_tree_selection_path_is_selected(selection, path) &&
        !gtk_drag_check_threshold(widget, r->x, r->y, event->x, event->y);
    r->pressed = gtk_tree_row_reference_new(r->model, path);
    r->pressed_at = now; r->x = event->x; r->y = event->y;
    gtk_tree_path_free(path);
    return FALSE;
}
static gboolean released_idle(gpointer data)
{
    Rename *r = data; r->idle = 0;
    g_autoptr(GtkTreePath) path = r->pressed ? gtk_tree_row_reference_get_path(r->pressed) : NULL;
    gboolean slow = r->slow;
    clear_ref(&r->click);
    if (path && gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(r->view)) == 1 &&
        gtk_tree_selection_path_is_selected(gtk_tree_view_get_selection(r->view), path)) {
        r->click = gtk_tree_row_reference_new(r->model, path);
        r->clicked_at = r->pressed_at;
    }
    clear_ref(&r->pressed); r->slow = FALSE;
    if (slow && r->click && path) cf_result_rename_start(r->view, path);
    return G_SOURCE_REMOVE;
}
static gboolean release(GtkWidget *view, GdkEventButton *event, gpointer data)
{
    (void)view;
    Rename *r = data;
    if (event->button == 1 && r->pressed && !r->idle) r->idle = g_idle_add(released_idle, r);
    return FALSE;
}
static gboolean motion(GtkWidget *view, GdkEventMotion *event, gpointer data)
{
    Rename *r = data;
    if (r->pressed && gtk_drag_check_threshold(view, r->x, r->y, event->x, event->y)) gesture_clear(r);
    return FALSE;
}
static void drag(GtkWidget *view, GdkDragContext *context, gpointer data)
{
    (void)view; (void)context; gesture_clear(data);
}
static void selection_changed(GtkTreeSelection *selection, gpointer data)
{
    Rename *r = data;
    if (r->pressed) return; /* The current pointer gesture is still selecting. */
    g_autoptr(GtkTreePath) path = r->click ? gtk_tree_row_reference_get_path(r->click) : NULL;
    if (!path || gtk_tree_selection_count_selected_rows(selection) != 1 ||
        !gtk_tree_selection_path_is_selected(selection, path)) gesture_clear(r);
}
static void row_deleted(GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
    (void)model; (void)path;
    Rename *r = data;
    /* Row-reference proxy updates may follow this signal handler. Cancel any editor
     * on deletion rather than inspecting a reference before GTK invalidates it. */
    if (r->row) cf_result_rename_cancel(r->view);
    gesture_clear(r);
}
static void model_changed(GObject *view, GParamSpec *spec, gpointer data)
{
    (void)spec;
    Rename *r = data;
    if (r->closed) return;
    cf_result_rename_cancel(GTK_TREE_VIEW(view));
    if (r->model) g_signal_handlers_disconnect_by_data(r->model, r);
    g_set_object(&r->model, gtk_tree_view_get_model(r->view));
    if (r->model) g_signal_connect(r->model, "row-deleted", G_CALLBACK(row_deleted), r);
}
static void destroyed(GtkWidget *view, gpointer data)
{
    Rename *r = data;
    r->closed = TRUE; cf_result_rename_cancel(GTK_TREE_VIEW(view));
    if (r->model) g_signal_handlers_disconnect_by_data(r->model, r);
    g_signal_handlers_disconnect_by_data(r->renderer, r);
}
static void free_rename(gpointer data)
{
    Rename *r = data;
    g_clear_object(&r->model); g_object_unref(r->renderer); g_object_unref(r->column); g_free(r);
}
void cf_result_rename_attach(GtkTreeView *view, CfRenameCommit commit)
{
    Rename *r = g_new0(Rename, 1);
    r->view = view; r->commit = commit;
    r->renderer = g_object_ref(g_object_get_data(G_OBJECT(view), "cf-name-renderer"));
    r->column = g_object_ref(g_object_get_data(G_OBJECT(view), "cf-name-column"));
    g_object_set_data_full(G_OBJECT(view), KEY, r, free_rename);
    model_changed(G_OBJECT(view), NULL, r);
    g_signal_connect(view, "notify::model", G_CALLBACK(model_changed), r);
    g_signal_connect(view, "destroy", G_CALLBACK(destroyed), r);
    g_signal_connect(r->renderer, "editing-started", G_CALLBACK(editing_started), r);
    g_signal_connect(r->renderer, "editing-canceled", G_CALLBACK(editing_cancelled), r);
    g_signal_connect(r->renderer, "edited", G_CALLBACK(edited), r);
    g_signal_connect(view, "button-press-event", G_CALLBACK(press), r);
    g_signal_connect(view, "button-release-event", G_CALLBACK(release), r);
    g_signal_connect(view, "motion-notify-event", G_CALLBACK(motion), r);
    g_signal_connect(view, "drag-begin", G_CALLBACK(drag), r);
    g_signal_connect(gtk_tree_view_get_selection(view), "changed", G_CALLBACK(selection_changed), r);
}
