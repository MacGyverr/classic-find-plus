#include "search-history.h"
#include "settings.h"

static void choose(GtkMenuItem *item, gpointer data)
{
    const char *name = g_object_get_data(G_OBJECT(item), "history-name");
    g_object_set(data, "name", name, NULL);
}
static void clear(GtkMenuItem *item, gpointer data)
{
    (void)item; (void)data;
    g_autoptr(CfSettings) settings = cf_settings_new();
    cf_settings_history_clear(settings);
}
GtkWidget *cf_search_history_menu(GtkEntry *entry, CfSearchForm *form)
{
    g_autoptr(CfSettings) settings = cf_settings_new();
    g_auto(GStrv) names = cf_settings_history(settings);
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(entry), NULL);
    for (guint i = 0; names[i]; i++) {
        GtkWidget *item = gtk_menu_item_new_with_label(names[i]);
        g_object_set_data_full(G_OBJECT(item), "history-name", g_strdup(names[i]), g_free);
        g_signal_connect_object(item, "activate", G_CALLBACK(choose), form, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    if (*names) gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget *item = gtk_menu_item_new_with_label("Clear History");
    gtk_widget_set_name(item, "clear-history");
    gtk_widget_set_sensitive(item, *names != NULL);
    g_signal_connect(item, "activate", G_CALLBACK(clear), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
    g_signal_connect_object(entry, "destroy", G_CALLBACK(gtk_widget_destroy), menu, G_CONNECT_SWAPPED);
    gtk_widget_show_all(menu);
    return menu;
}
static void pressed(GtkEntry *entry, GtkEntryIconPosition position, GdkEvent *event, gpointer data)
{
    if (position != GTK_ENTRY_ICON_SECONDARY) return;
    GtkWidget *menu = cf_search_history_menu(entry, data);
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(entry), GDK_GRAVITY_SOUTH_EAST,
                            GDK_GRAVITY_NORTH_EAST, event);
}
void cf_search_history_attach(GtkEntry *entry, CfSearchForm *form)
{
    gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "pan-down-symbolic");
    gtk_entry_set_icon_tooltip_text(entry, GTK_ENTRY_ICON_SECONDARY, "Recent filename searches");
    g_signal_connect_object(entry, "icon-press", G_CALLBACK(pressed), form, 0);
}
