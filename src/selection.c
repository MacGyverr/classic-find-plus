#include "selection.h"
#include "results.h"
#include "results-sync.h"

GPtrArray *cf_selection_files(GtkTreeView *view)
{
    GPtrArray *files = g_ptr_array_new_with_free_func(g_object_unref);
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(view), &model);
    for (GList *p = rows; p; p = p->next) {
        GtkTreeIter iter;
        GFile *file = NULL;
        if (gtk_tree_model_get_iter(model, &iter, p->data))
            gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, -1);
        if (file) g_ptr_array_add(files, file);
    }
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
    return files;
}

char **cf_selection_uris(GPtrArray *files)
{
    char **uris = g_new0(char *, files->len + 1);
    for (guint i = 0; i < files->len; i++) uris[i] = g_file_get_uri(g_ptr_array_index(files, i));
    return uris;
}

char *cf_selection_payload(GPtrArray *files, gboolean cut, gboolean icons)
{
    GString *text = g_string_new(icons ? "" : cut ? "cut\n" : "copy\n");
    for (guint i = 0; i < files->len; i++) {
        g_autofree char *uri = g_file_get_uri(g_ptr_array_index(files, i));
        g_string_append(text, uri);
        /* Nemo's icon-list includes geometry; zero dimensions are intentional. */
        g_string_append(text, icons ? "\r0:0:0:0\r\n" : i + 1 < files->len ? "\n" : "");
    }
    return g_string_free(text, FALSE);
}

typedef struct { GPtrArray *files; gboolean cut; } Clipboard;
static void clipboard_get(GtkClipboard *clipboard, GtkSelectionData *data, guint info, gpointer user_data)
{
    (void)clipboard;
    Clipboard *copy = user_data;
    if (info == 0) {
        g_autofree char *text = cf_selection_payload(copy->files, copy->cut, FALSE);
        gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8, (const guchar *)text, strlen(text));
    } else if (info == 1) {
        g_auto(GStrv) uris = cf_selection_uris(copy->files);
        gtk_selection_data_set_uris(data, uris);
    } else {
        g_auto(GStrv) uris = cf_selection_uris(copy->files);
        g_autofree char *text = g_strjoinv("\n", uris);
        gtk_selection_data_set_text(data, text, -1);
    }
}
static void clipboard_clear(GtkClipboard *clipboard, gpointer data)
{
    (void)clipboard;
    Clipboard *copy = data;
    g_ptr_array_unref(copy->files);
    g_free(copy);
}
void cf_selection_copy(GtkTreeView *view, gboolean cut)
{
    Clipboard *copy = g_new0(Clipboard, 1);
    copy->files = cf_selection_files(view);
    copy->cut = cut;
    if (!copy->files->len) { clipboard_clear(NULL, copy); return; }
    GtkTargetEntry targets[] = {
        { "x-special/gnome-copied-files", 0, 0 }, { "text/uri-list", 0, 1 },
        { "UTF8_STRING", 0, 2 }, { "text/plain;charset=utf-8", 0, 2 }
    };
    GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(view), GDK_SELECTION_CLIPBOARD);
    if (!gtk_clipboard_set_with_data(clipboard, targets, G_N_ELEMENTS(targets), clipboard_get, clipboard_clear, copy))
        clipboard_clear(NULL, copy);
    else gtk_clipboard_set_can_store(clipboard, NULL, 0);
}
static void drag_begin(GtkWidget *view, GdkDragContext *context, gpointer unused)
{
    (void)unused;
    GPtrArray *files = cf_selection_files(GTK_TREE_VIEW(view));
    g_object_set_data_full(G_OBJECT(context), "cf-files", files, (GDestroyNotify)g_ptr_array_unref);
    cf_results_sync_drag_begin(GTK_TREE_VIEW(view), files);
}
static void drag_end(GtkWidget *view, GdkDragContext *context, gpointer unused)
{
    (void)unused;
    GPtrArray *files = g_object_get_data(G_OBJECT(context), "cf-files");
    if (files) cf_results_sync_drag_end(GTK_TREE_VIEW(view), files);
}

static void drag_get(GtkWidget *view, GdkDragContext *context, GtkSelectionData *data,
                     guint info, guint time, gpointer unused)
{
    (void)time; (void)unused;
    GPtrArray *files = g_object_get_data(G_OBJECT(context), "cf-files");
    if (files) {
        if (info == 0) {
            g_auto(GStrv) uris = cf_selection_uris(files);
            gtk_selection_data_set_uris(data, uris);
        } else {
            g_autofree char *text = cf_selection_payload(files, FALSE, TRUE);
            gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8,
                                   (const guchar *)text, strlen(text));
        }
    }
    g_signal_stop_emission_by_name(view, "drag-data-get");
}
static void drag_delete(GtkWidget *view, GdkDragContext *context, gpointer unused)
{
    (void)context; (void)unused;
    /* The external receiver moves files. GtkListStore must not delete rows by path. */
    g_signal_stop_emission_by_name(view, "drag-data-delete");
}
typedef struct { GtkTreeRowReference *row; double x, y; } Press;
static void press_free(gpointer data)
{
    Press *press = data;
    if (press->row) gtk_tree_row_reference_free(press->row);
    g_free(press);
}
static gboolean press_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    Press *press = data;
    if (press->row) { gtk_tree_row_reference_free(press->row); press->row = NULL; }
    if (event->button != 1 || event->type != GDK_BUTTON_PRESS ||
        (event->state & gtk_accelerator_get_default_mod_mask())) return FALSE;
    GtkTreeView *view = GTK_TREE_VIEW(widget);
    GtkTreePath *path = NULL;
    if (!gtk_tree_view_get_path_at_pos(view, event->x, event->y, &path, NULL, NULL, NULL)) return FALSE;
    gboolean selected = gtk_tree_selection_path_is_selected(gtk_tree_view_get_selection(view), path);
    if (selected) {
        /* Delay collapsing a multi-selection until release so a drag keeps every row. */
        press->row = gtk_tree_row_reference_new(gtk_tree_view_get_model(view), path);
        press->x = event->x; press->y = event->y;
        gtk_widget_grab_focus(widget);
    }
    gtk_tree_path_free(path);
    return selected;
}
static gboolean release_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    Press *press = data;
    if (event->button != 1 || !press->row) return FALSE;
    GtkTreePath *path = gtk_tree_row_reference_get_path(press->row);
    gtk_tree_row_reference_free(press->row); press->row = NULL;
    if (path) {
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), path, NULL, FALSE);
        gtk_tree_path_free(path);
    }
    return TRUE;
}
static gboolean motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
    Press *press = data;
    if (!press->row || !(event->state & GDK_BUTTON1_MASK)) return FALSE;
    if (gtk_drag_check_threshold(widget, press->x, press->y, event->x, event->y)) {
        gtk_tree_row_reference_free(press->row); press->row = NULL;
        GdkDragContext *context = gtk_drag_begin_with_coordinates(widget,
            gtk_drag_source_get_target_list(widget), GDK_ACTION_COPY | GDK_ACTION_MOVE,
            1, (GdkEvent *)event, -1, -1);
        if (context) gtk_drag_set_icon_default(context);
    }
    return TRUE;
}
void cf_selection_attach(GtkTreeView *view)
{
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view), GTK_SELECTION_MULTIPLE);
    GtkTargetEntry targets[] = { { "x-special/gnome-icon-list", 0, 1 }, { "text/uri-list", 0, 0 } };
    gtk_tree_view_enable_model_drag_source(view, GDK_BUTTON1_MASK, targets, G_N_ELEMENTS(targets),
                                          GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(view, "drag-begin", G_CALLBACK(drag_begin), NULL);
    g_signal_connect(view, "drag-end", G_CALLBACK(drag_end), NULL);
    g_signal_connect(view, "drag-data-get", G_CALLBACK(drag_get), NULL);
    g_signal_connect(view, "drag-data-delete", G_CALLBACK(drag_delete), NULL);
    Press *press = g_new0(Press, 1);
    g_object_set_data_full(G_OBJECT(view), "cf-selection-press", press, press_free);
    gtk_widget_add_events(GTK_WIDGET(view), GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(view, "button-press-event", G_CALLBACK(press_button), press);
    g_signal_connect(view, "button-release-event", G_CALLBACK(release_button), press);
    g_signal_connect(view, "motion-notify-event", G_CALLBACK(motion), press);
}
