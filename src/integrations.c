#include "integrations.h"
#include "config.h"

static const char *managers[] = { "nemo", "nautilus", "dolphin", "caja", "thunar" };
typedef struct {
    GtkWidget *box, *message, *labels[5], *install[5], *enable[5], *remove[5];
    gboolean enabled[5], closed;
    GSubprocess *process;
    GCancellable *cancel;
    char *operation;
} Integrations;
static void run(Integrations *s, const char *operation, const char *manager);
static void free_state(gpointer data)
{
    Integrations *s = data;
    g_clear_object(&s->process); g_clear_object(&s->cancel); g_free(s->operation); g_free(s);
}
static void closed(GtkWidget *widget, gpointer data)
{
    (void) widget;
    Integrations *s = data; s->closed = TRUE;
    g_cancellable_cancel(s->cancel);
    if (s->process) g_subprocess_force_exit(s->process);
}
static void finished(GObject *object, GAsyncResult *result, gpointer data)
{
    GtkWidget *box = data;
    Integrations *s = g_object_get_data(G_OBJECT(box), "integrations");
    g_autofree char *out = NULL, *err = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object), result, &out, &err, &error) &&
        g_subprocess_get_successful(G_SUBPROCESS(object));
    g_clear_object(&s->process);
    if (s->closed) { g_object_unref(box); return; }
    gtk_widget_set_sensitive(s->box, TRUE);
    if (!ok) {
        gtk_label_set_text(GTK_LABEL(s->message), error ? error->message : (err && *err ? err : "Integration update failed."));
    } else if (g_strcmp0(s->operation, "status")) {
        gtk_label_set_text(GTK_LABEL(s->message), "Updated. Restart the affected file manager to reload its menu. No packages were installed.");
        run(s, "status", NULL);
    } else {
        g_autoptr(GKeyFile) keys = g_key_file_new();
        if (!out || !g_key_file_load_from_data(keys, out, -1, G_KEY_FILE_NONE, &error)) {
            gtk_label_set_text(GTK_LABEL(s->message), error ? error->message : "Invalid integration status.");
        } else for (guint i = 0; i < G_N_ELEMENTS(managers); i++) {
            const char *name = managers[i];
            gboolean detected = g_key_file_get_boolean(keys, name, "detected", NULL);
            gboolean installed = g_key_file_get_boolean(keys, name, "installed", NULL);
            s->enabled[i] = g_key_file_get_boolean(keys, name, "enabled", NULL);
            gboolean reload = g_key_file_get_boolean(keys, name, "reload", NULL);
            g_autofree char *missing = g_key_file_get_string(keys, name, "missing", NULL);
            g_autofree char *version = g_key_file_get_string(keys, name, "version", NULL);
            g_autofree char *method = g_key_file_get_string(keys, name, "method", NULL);
            g_autofree char *label = g_strdup_printf("%s%s%s · %s · %s%s%s%s",
                detected ? "Detected" : "Not detected", version && *version ? " " : "", version ? version : "",
                installed ? "Installed" : "Not installed", s->enabled[i] ? "Enabled" : "Disabled",
                missing && *missing ? " · Missing: " : "", missing ? missing : "",
                reload ? " · Restart/reload needed" : "");
            gtk_label_set_text(GTK_LABEL(s->labels[i]), label);
            if (method && *method) {
                g_autofree char *with_method = g_strconcat(label, " · ", method, NULL);
                gtk_label_set_text(GTK_LABEL(s->labels[i]), with_method);
            }
            gboolean switch_caja = !g_strcmp0(name, "caja") &&
                g_key_file_get_boolean(keys, name, "legacy_extension", NULL);
            gtk_widget_set_sensitive(s->install[i], !installed || switch_caja);
            gtk_button_set_label(GTK_BUTTON(s->install[i]), switch_caja ? "Switch to Scripts" : "Install");
            g_object_set_data(G_OBJECT(s->install[i]), "operation", switch_caja ? "enable" : "install");
            gtk_widget_set_sensitive(s->enable[i], installed);
            gtk_button_set_label(GTK_BUTTON(s->enable[i]), s->enabled[i] ? "Disable" : "Enable");
            gtk_widget_set_sensitive(s->remove[i], installed);
        }
    }
    g_object_unref(box);
}
static void run(Integrations *s, const char *operation, const char *manager)
{
    if (s->process || s->closed) return;
    const char *helper = g_getenv("CFP_INTEGRATION_HELPER");
    g_autofree char *executable = g_file_read_link("/proc/self/exe", NULL);
    g_autofree char *bin = executable ? g_path_get_dirname(executable) : NULL;
    g_autofree char *prefix = bin ? g_path_get_dirname(bin) : NULL;
    g_autofree char *relative = prefix ? g_build_filename(prefix, "share/classic-find-plus/integration-manager.py", NULL) : NULL;
    if (!helper || !*helper) helper = relative && g_file_test(relative, G_FILE_TEST_IS_REGULAR) ? relative : CF_INTEGRATION_HELPER;
    const char *argv[] = { "python3", helper, operation, manager ? manager : "--keyfile", NULL };
    g_autoptr(GError) error = NULL;
    s->process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (!s->process) { gtk_label_set_text(GTK_LABEL(s->message), error->message); return; }
    g_free(s->operation); s->operation = g_strdup(operation);
    gtk_widget_set_sensitive(s->box, FALSE);
    g_subprocess_communicate_utf8_async(s->process, NULL, s->cancel, finished, g_object_ref(s->box));
}
static void clicked(GtkButton *button, gpointer data)
{
    Integrations *s = data;
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "manager"));
    const char *op = g_object_get_data(G_OBJECT(button), "operation");
    if (!g_strcmp0(op, "toggle")) op = s->enabled[i] ? "disable" : "enable";
    run(s, op, managers[i]);
}
static void refresh(GtkButton *button, gpointer data)
{
    (void)button; run(data, "status", NULL);
}
static GtkWidget *control(Integrations *s, guint i, const char *label, const char *op)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_autofree char *id = g_strdup_printf("integration-%s-%s", managers[i], op);
    gtk_widget_set_name(button, id);
    g_object_set_data(G_OBJECT(button), "manager", GUINT_TO_POINTER(i));
    g_object_set_data(G_OBJECT(button), "operation", (gpointer)op);
    g_signal_connect(button, "clicked", G_CALLBACK(clicked), s);
    gtk_widget_set_sensitive(button, FALSE);
    return button;
}
GtkWidget *cf_integrations_new(void)
{
    Integrations *s = g_new0(Integrations, 1);
    s->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); s->cancel = g_cancellable_new();
    gtk_widget_set_name(s->box, "file-manager-integrations");
    g_object_set_data_full(G_OBJECT(s->box), "integrations", s, free_state);
    g_signal_connect(s->box, "destroy", G_CALLBACK(closed), s);
    GtkWidget *title = gtk_label_new("File Manager Integrations");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_pack_start(GTK_BOX(s->box), title, FALSE, FALSE, 0);
    s->message = gtk_label_new("Optional user-local menus. Install registers a disabled adapter; Enable activates it. "
        "Remove deletes only unchanged CFP-owned entries. Dependencies are never installed automatically. "
        "Caja uses Scripts > Find with Classic Find Plus. Restart the manager after changes. "
        "Existing system Nemo actions are retained; Enable/Disable uses Nemo’s per-user preference.");
    gtk_label_set_line_wrap(GTK_LABEL(s->message), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(s->message), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(s->message), 65); gtk_label_set_xalign(GTK_LABEL(s->message), 0);
    gtk_box_pack_start(GTK_BOX(s->box), s->message, FALSE, FALSE, 0);
    for (guint i = 0; i < G_N_ELEMENTS(managers); i++) {
        GtkWidget *name = gtk_label_new(managers[i]); gtk_label_set_xalign(GTK_LABEL(name), 0);
        gtk_box_pack_start(GTK_BOX(s->box), name, FALSE, FALSE, 0);
        s->labels[i] = gtk_label_new("Status not checked — press Refresh");
        g_autofree char *id = g_strdup_printf("integration-%s-status", managers[i]);
        gtk_widget_set_name(s->labels[i], id);
        gtk_label_set_xalign(GTK_LABEL(s->labels[i]), 0); gtk_label_set_line_wrap(GTK_LABEL(s->labels[i]), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(s->labels[i]), 65);
        gtk_box_pack_start(GTK_BOX(s->box), s->labels[i], FALSE, FALSE, 0);
        GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        s->install[i] = control(s, i, "Install", "install");
        s->enable[i] = control(s, i, "Enable", "toggle");
        s->remove[i] = control(s, i, "Remove", "remove");
        gtk_box_pack_start(GTK_BOX(buttons), s->install[i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(buttons), s->enable[i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(buttons), s->remove[i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(s->box), buttons, FALSE, FALSE, 0);
    }
    GtkWidget *button = gtk_button_new_with_label("Refresh integration status");
    gtk_widget_set_name(button, "integration-refresh");
    g_signal_connect(button, "clicked", G_CALLBACK(refresh), s);
    gtk_box_pack_start(GTK_BOX(s->box), button, FALSE, FALSE, 0);
    /* Lazy detection avoids subprocess work on every normal window launch. */
    return s->box;
}
