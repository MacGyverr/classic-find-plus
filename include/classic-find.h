#pragma once
#include <gtk/gtk.h>
GFile *cf_location_resolve(const char *argument, GError **error);
GFile *cf_location_startup(const char *argument, GError **error);
GtkWidget *cf_window_new(GtkApplication *app, GFile *root);
