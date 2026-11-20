#include "search-history.h"
#include "settings.h"

static void choose(GtkMenuItem *item, gpointer data)
{
    const char *name = g_object_get_data(G_OBJECT(item), "history-name");
    gtk_entry_set_text(GTK_ENTRY(data), name);
}
static void clear(GtkMenuItem *item, gpointer data)
{
    (void)item;
    g_autoptr(CfSettings) settings = cf_settings_new();
    cf_settings_history_clear(settings, GPOINTER_TO_INT(data));
}
static GtkWidget *history_menu(GtkEntry *entry, CfHistoryField field)
{
    g_autoptr(CfSettings) settings = cf_settings_new();
    g_auto(GStrv) names = cf_settings_history(settings, field);
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(entry), NULL);
    for (guint i = 0; names[i]; i++) {
        /* Keep large text/Hex expressions from making the popup oversized. */
        g_autofree char *label = g_utf8_substring(names[i], 0, MIN(g_utf8_strlen(names[i], -1), 80));
        if (names[i][strlen(label)]) {
            char *shortened = g_strconcat(label, "…", NULL);
            g_free(label);
            label = shortened;
        }
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        gtk_label_set_single_line_mode(GTK_LABEL(gtk_bin_get_child(GTK_BIN(item))), TRUE);
        g_object_set_data_full(G_OBJECT(item), "history-name", g_strdup(names[i]), g_free);
        g_signal_connect_object(item, "activate", G_CALLBACK(choose), entry, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    if (*names) gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget *item = gtk_menu_item_new_with_label("Clear History");
    gtk_widget_set_name(item, "clear-history");
    gtk_widget_set_sensitive(item, *names != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(clear), GINT_TO_POINTER(field));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
    g_signal_connect_object(entry, "destroy", G_CALLBACK(gtk_widget_destroy), menu, G_CONNECT_SWAPPED);
    gtk_widget_show_all(menu);
    return menu;
}
static void popup(GtkEntry *entry, GdkEvent *event, gpointer data)
{
    GtkWidget *menu = history_menu(entry, GPOINTER_TO_INT(data));
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(entry), GDK_GRAVITY_SOUTH_EAST,
                            GDK_GRAVITY_NORTH_EAST, event);
}
static void pressed(GtkEntry *entry, GtkEntryIconPosition position, GdkEvent *event, gpointer data)
{
    if (position == GTK_ENTRY_ICON_SECONDARY) popup(entry, event, data);
}
static gboolean history_key(GtkEntry *entry, GdkEventKey *event, gpointer data)
{
    if (event->keyval != GDK_KEY_Down ||
        (event->state & gtk_accelerator_get_default_mod_mask()) != GDK_MOD1_MASK) return FALSE;
    popup(entry, (GdkEvent *)event, data);
    return TRUE;
}
void cf_search_history_attach(GtkEntry *entry, CfHistoryField field)
{
    const char *const tooltips[] = {
        "Recent file names (Alt+Down)", "Recent text expressions (Alt+Down)",
        "Recent search locations (Alt+Down)", "Recent exclusions (Alt+Down)"
    };
    gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "pan-down-symbolic");
    gtk_entry_set_icon_tooltip_text(entry, GTK_ENTRY_ICON_SECONDARY, tooltips[field]);
    g_signal_connect(entry, "icon-press", G_CALLBACK(pressed), GINT_TO_POINTER(field));
    g_signal_connect(entry, "key-press-event", G_CALLBACK(history_key), GINT_TO_POINTER(field));
}
