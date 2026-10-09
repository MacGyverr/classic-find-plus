#pragma once
#include <gtk/gtk.h>
#include "search-form.h"
void cf_search_history_attach(GtkEntry *entry, CfSearchForm *form);
GtkWidget *cf_search_history_menu(GtkEntry *entry, CfSearchForm *form);
