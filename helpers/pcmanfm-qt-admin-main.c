#include "thunar-admin.h"
#include <gtk/gtk.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    g_autoptr(GError) error = NULL;
    const char *target = argc == 3 && !g_strcmp0(argv[1], "--") ? argv[2] : NULL;
    if (geteuid() == 0) {
        g_printerr("Run this action as your normal desktop user, not root.\n");
        return 1;
    }
    if (!target)
        g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Choose one local directory; remote locations are unsupported. Usage: classic-find-plus-pcmanfm-qt-admin -- LOCAL_DIRECTORY_OR_FILE_URI");
    else {
        g_autofree char *uri = cf_thunar_admin_uri(target, &error);
        if (uri) {
            g_autofree char *program = g_find_program_in_path("pcmanfm-qt");
            if (!program) {
                g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                    "PCManFM-Qt is unavailable. Configure it separately.");
            } else if (cf_thunar_admin_launch(uri, program, g_vfs_get_supported_uri_schemes(g_vfs_get_default()), &error))
                return 0;
        }
    }
    g_printerr("Open as Administrator: %s\n", error->message);
    if (gtk_init_check(NULL, NULL)) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "Could not open the directory as Administrator");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s\n\n"
            "Check GVfs admin support and the desktop Polkit authentication agent. "
            "Authentication cancellation, permission and location errors are handled by PCManFM-Qt/GVfs.", error->message);
        gtk_dialog_run(GTK_DIALOG(dialog)); gtk_widget_destroy(dialog);
    }
    return 1;
}
