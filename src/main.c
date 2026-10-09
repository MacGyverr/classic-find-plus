#include "classic-find.h"
#include "app-info.h"
#include <unistd.h>

static void activate(GtkApplication *app, gpointer data)
{
    gtk_widget_show_all(cf_window_new(app, G_FILE(data)));
}

int main(int argc, char **argv)
{
    g_autoptr(GError) error = NULL;
    g_auto(GStrv) paths = NULL;
    gboolean version = FALSE;
    GOptionEntry entries[] = {
        { "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show version", NULL },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &paths,
          "Directory to search (defaults to saved location or home)", "[DIRECTORY]" },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
    g_autoptr(GOptionContext) options = g_option_context_new("— " CF_APP_NAME);
    g_option_context_add_main_entries(options, entries, NULL);
    if (!g_option_context_parse(options, &argc, &argv, &error)) {
        g_printerr("classic-find-plus: %s\n", error->message);
        return 2;
    }
    if (version) {
        g_print(CF_APP_NAME " " CF_APP_VERSION "\n");
        return 0;
    }
    if (paths && g_strv_length(paths) > 1) {
        g_printerr("classic-find-plus: expected one directory\n");
        return 2;
    }
    if (geteuid() == 0) {
        g_printerr("classic-find-plus: refusing to run the GUI as root\n");
        return 1;
    }
    g_autoptr(GFile) root = cf_location_startup(paths ? paths[0] : NULL, &error);
    if (!root) {
        g_printerr("classic-find-plus: %s\n", error->message);
        return 1;
    }
    if (!gtk_init_check(NULL, NULL)) {
        g_printerr("classic-find-plus: cannot connect to a graphical display\n");
        return 1;
    }
    g_autoptr(GtkApplication) app = gtk_application_new(CF_APP_ID,
                                                       G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), root);
    return g_application_run(G_APPLICATION(app), 1, argv);
}
