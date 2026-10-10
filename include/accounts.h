#pragma once
#include <gtk/gtk.h>
/* One lazy bounded NSS subprocess/cache per application; widgets are weakly held. */
void cf_accounts_register(GtkComboBoxText *combo, GtkWidget *enable, gboolean users);
void cf_accounts_load(void);
void cf_accounts_default(GtkComboBoxText *combo);
