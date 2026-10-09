#pragma once
#include <gtk/gtk.h>
void cf_result_columns_attach(GtkTreeView *view);
GtkWidget *cf_result_columns_menu(GtkTreeView *view);

/* Stable column identity and existing display-string model mapping. */
const char *cf_result_column_display_id(GtkTreeViewColumn *column, int *model_id);
