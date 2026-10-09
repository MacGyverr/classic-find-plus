#define _GNU_SOURCE
#include "export-dialog.h"
#include <glib/gstdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#define INFO G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_ETAG_VALUE

gboolean cf_export_write(CfExportSnapshot *snapshot, CfExportFormat format, GFile *file,
                         const char *etag, GCancellable *cancel, GError **error)
{
    g_autofree char *path = g_file_get_path(file);
    if (!path) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "Export requires a local or mounted filesystem destination.");
        return FALSE;
    }
    if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
    g_autofree char *directory = g_path_get_dirname(path);
    g_autofree char *uuid = g_uuid_string_random();
    g_autofree char *temp_name = g_strconcat(".classic-find-plus-export-", uuid, NULL);
    g_autofree char *temp_path = g_build_filename(directory, temp_name, NULL);
    g_autoptr(GFile) temporary = g_file_new_for_path(temp_path);
    g_autoptr(GFileOutputStream) stream = g_file_create(temporary, G_FILE_CREATE_PRIVATE, cancel, error);
    if (!stream) return FALSE;
    gboolean ok = cf_export_serialize(snapshot, format, G_OUTPUT_STREAM(stream), cancel, error);
    if (ok) ok = g_output_stream_close(G_OUTPUT_STREAM(stream), cancel, error);
    else g_output_stream_close(G_OUTPUT_STREAM(stream), NULL, NULL);
    if (ok) ok = !g_cancellable_set_error_if_cancelled(cancel, error);
    if (ok && etag) {
        g_autoptr(GFileInfo) info = g_file_query_info(file, INFO, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, error);
        ok = info != NULL;
        if (ok && (g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR ||
                   g_strcmp0(etag, g_file_info_get_etag(info)))) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_WRONG_ETAG,
                                "The destination changed after confirmation. Choose it again to confirm its current version.");
            ok = FALSE;
        }
    }
    if (ok) {
        /* Same-directory rename is atomic; link provides atomic no-overwrite creation. */
        int result = etag ? g_rename(temp_path, path) : renameat2(AT_FDCWD, temp_path, AT_FDCWD, path, RENAME_NOREPLACE);
        if (!etag && result != 0 && (errno == ENOSYS || errno == EINVAL)) result = link(temp_path, path);
        if (result != 0) {
            int saved = errno;
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved), "Cannot save export: %s", g_strerror(saved));
            ok = FALSE;
        }
    }
    /* Never commit a partial file, even if serialization/close/cancellation failed. */
    g_unlink(temp_path);
    return ok;
}

typedef struct {
    gint refs;
    CfExportSnapshot *snapshot;
    GtkFileFilter *csv, *text;
    GtkWidget *message;
    GFile *confirmed;
    char *etag;
    GCancellable *cancel;
    gboolean busy;
} Dialog;
static void dialog_free(gpointer data);
typedef struct {
    Dialog *owner;
    GApplication *application;
    CfExportSnapshot *snapshot;
    CfExportFormat format;
    GFile *file;
    char *etag;
    GWeakRef dialog;
} Write;

static CfExportFormat selected(GtkFileChooser *chooser, Dialog *d)
{
    return gtk_file_chooser_get_filter(chooser) == d->text ? CF_EXPORT_TEXT : CF_EXPORT_CSV;
}
static void message(Dialog *d, const char *text)
{
    gtk_label_set_text(GTK_LABEL(d->message), text);
    gtk_widget_set_visible(d->message, *text != 0);
}
static gboolean normalize(GtkFileChooser *chooser, Dialog *d)
{
    g_autofree char *name = gtk_file_chooser_get_current_name(chooser);
    g_autofree char *fixed = cf_export_filename(name, selected(chooser, d));
    if (!g_strcmp0(name, fixed)) return FALSE;
    gtk_file_chooser_set_current_name(chooser, fixed);
    return TRUE;
}
static void format_changed(GObject *chooser, GParamSpec *pspec, gpointer data)
{
    (void)pspec;
    Dialog *d = data;
    normalize(GTK_FILE_CHOOSER(chooser), d);
    g_clear_object(&d->confirmed);
    g_clear_pointer(&d->etag, g_free);
}
static GtkFileChooserConfirmation confirm(GtkFileChooser *chooser, gpointer data)
{
    Dialog *d = data;
    g_clear_object(&d->confirmed);
    g_clear_pointer(&d->etag, g_free);
    if (normalize(chooser, d)) {
        message(d, "The extension now matches the selected format. Press Save to continue.");
        return GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
    }
    g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
    g_autoptr(GError) error = NULL;
    g_autoptr(GFileInfo) info = g_file_query_info(file, INFO, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
    if (!info || g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR || !g_file_info_get_etag(info)) {
        message(d, error ? error->message : "Choose a regular destination file, not a folder or symbolic link.");
        return GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
    }
    d->confirmed = g_object_ref(file);
    d->etag = g_strdup(g_file_info_get_etag(info));
    return GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;
}
static void write_free(gpointer data)
{
    Write *w = data;
    cf_export_snapshot_unref(w->snapshot);
    g_object_unref(w->file);
    g_free(w->etag);
    g_weak_ref_clear(&w->dialog);
    dialog_free(w->owner);
    g_clear_object(&w->application);
    g_free(w);
}
static void worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
    (void)source;
    Write *w = data;
    GError *error = NULL;
    if (cf_export_write(w->snapshot, w->format, w->file, w->etag, cancel, &error)) g_task_return_boolean(task, TRUE);
    else g_task_return_error(task, error);
}
static void written(GObject *source, GAsyncResult *result, gpointer unused)
{
    (void)source; (void)unused;
    GTask *task = G_TASK(result);
    Write *w = g_task_get_task_data(task);
    g_autoptr(GError) error = NULL;
    gboolean ok = g_task_propagate_boolean(task, &error);
    w->owner->busy = FALSE;
    if (w->application) {
        g_application_release(w->application);
        g_clear_object(&w->application);
    }
    g_autoptr(GtkWidget) dialog = g_weak_ref_get(&w->dialog);
    if (!dialog) return;
    Dialog *d = g_object_get_data(G_OBJECT(dialog), "cf-export-dialog");
    if (!d) return;
    d->busy = FALSE;
    if (g_cancellable_is_cancelled(d->cancel)) return;
    gtk_widget_set_sensitive(dialog, TRUE);
    if (ok) gtk_widget_destroy(dialog);
    else {
        g_autofree char *text = g_strdup_printf("Export failed: %s\nChoose another filename or folder and try again. Your results are unchanged.", error->message);
        message(d, text);
        g_clear_object(&d->confirmed);
        g_clear_pointer(&d->etag, g_free);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, TRUE);
    }
}
static void response(GtkDialog *dialog, gint id, gpointer data)
{
    Dialog *d = data;
    if (id != GTK_RESPONSE_ACCEPT) { gtk_widget_destroy(GTK_WIDGET(dialog)); return; }
    if (d->busy) return;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    if (normalize(chooser, d)) {
        message(d, "The extension now matches the selected format. Press Save to continue.");
        return;
    }
    g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
    if (!file) { message(d, "Choose an output filename."); return; }
    Write *w = g_new0(Write, 1);
    g_atomic_int_inc(&d->refs);
    w->owner = d;
    GtkWindow *parent = gtk_window_get_transient_for(GTK_WINDOW(dialog));
    GtkApplication *application = parent ? gtk_window_get_application(parent) : NULL;
    if (application) {
        w->application = g_object_ref(G_APPLICATION(application));
        g_application_hold(w->application);
    }
    w->snapshot = cf_export_snapshot_ref(d->snapshot);
    w->format = selected(chooser, d);
    w->file = g_object_ref(file);
    w->etag = d->confirmed && g_file_equal(file, d->confirmed) ? g_strdup(d->etag) : NULL;
    g_weak_ref_init(&w->dialog, dialog);
    GTask *task = g_task_new(NULL, d->cancel, written, NULL);
    g_task_set_task_data(task, w, write_free);
    /* A finished commit is success even if cancellation arrives just afterward. */
    g_task_set_check_cancellable(task, FALSE);
    d->busy = TRUE;
    message(d, "Writing export… Closing this dialog cancels pending work.");
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, FALSE);
    g_task_run_in_thread(task, worker);
    g_object_unref(task);
}
static void destroyed(GtkWidget *widget, gpointer data)
{
    Dialog *d = data;
    g_cancellable_cancel(d->cancel);
    g_signal_handlers_disconnect_by_data(widget, d);
}
static void dialog_free(gpointer data)
{
    Dialog *d = data;
    if (!g_atomic_int_dec_and_test(&d->refs)) return;
    cf_export_snapshot_unref(d->snapshot);
    g_clear_object(&d->confirmed);
    g_free(d->etag);
    g_object_unref(d->cancel);
    g_free(d);
}
GtkWidget *cf_export_dialog_new(GtkWindow *parent, GtkTreeView *view, GFile *root)
{
    Dialog *d = g_new0(Dialog, 1);
    d->refs = 1;
    d->snapshot = cf_export_snapshot_new(view);
    if (!cf_export_snapshot_rows(d->snapshot)) {
        cf_export_snapshot_unref(d->snapshot);
        g_free(d);
        return NULL;
    }
    d->cancel = g_cancellable_new();
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Export results", parent, GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_widget_set_name(dialog, "export-dialog");
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
    g_object_set_data_full(G_OBJECT(dialog), "cf-export-dialog", d, dialog_free);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_local_only(chooser, TRUE);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    d->csv = gtk_file_filter_new();
    gtk_file_filter_set_name(d->csv, "CSV (Comma-separated values) (*.csv)");
    gtk_file_filter_add_pattern(d->csv, "*.csv");
    gtk_file_filter_add_pattern(d->csv, "*.CSV");
    gtk_file_chooser_add_filter(chooser, d->csv);
    d->text = gtk_file_filter_new();
    gtk_file_filter_set_name(d->text, "Text (Tab-delimited) (*.txt)");
    gtk_file_filter_add_pattern(d->text, "*.txt");
    gtk_file_filter_add_pattern(d->text, "*.TXT");
    gtk_file_chooser_add_filter(chooser, d->text);
    gtk_file_chooser_set_filter(chooser, d->csv);
    gtk_file_chooser_set_current_name(chooser, "Classic Find Plus Results.csv");
    g_autoptr(GFileInfo) root_info = root ? g_file_query_info(root,
        G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_ACCESS_CAN_READ, G_FILE_QUERY_INFO_NONE, NULL, NULL) : NULL;
    if (!root_info || g_file_info_get_file_type(root_info) != G_FILE_TYPE_DIRECTORY ||
        (g_file_info_has_attribute(root_info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ) &&
         !g_file_info_get_attribute_boolean(root_info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ)) ||
        !gtk_file_chooser_set_current_folder_file(chooser, root, NULL))
        gtk_file_chooser_set_current_folder(chooser, g_get_home_dir());
    d->message = gtk_label_new("");
    gtk_widget_set_name(d->message, "export-message");
    gtk_label_set_line_wrap(GTK_LABEL(d->message), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(d->message), 70);
    gtk_label_set_xalign(GTK_LABEL(d->message), 0);
    gtk_file_chooser_set_extra_widget(chooser, d->message);
    g_signal_connect(dialog, "notify::filter", G_CALLBACK(format_changed), d);
    g_signal_connect(dialog, "confirm-overwrite", G_CALLBACK(confirm), d);
    g_signal_connect(dialog, "response", G_CALLBACK(response), d);
    g_signal_connect(dialog, "destroy", G_CALLBACK(destroyed), d);
    gtk_widget_show(dialog);
    return dialog;
}

gboolean cf_export_dialog_is_writing(GtkWidget *dialog)
{
    Dialog *d = g_object_get_data(G_OBJECT(dialog), "cf-export-dialog");
    return d && d->busy;
}
