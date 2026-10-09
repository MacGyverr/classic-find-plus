#pragma once
#include <gtk/gtk.h>
GtkWidget *cf_date_dialog_new(GtkWindow *parent, const char *expression);
const char *cf_date_dialog_get_expression(GtkDialog *dialog);
