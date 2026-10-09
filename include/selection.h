#pragma once
#include <gtk/gtk.h>
/* Owned array of owned GFile references, in visible row order. */
GPtrArray *cf_selection_files(GtkTreeView *view);
char **cf_selection_uris(GPtrArray *files);
char *cf_selection_payload(GPtrArray *files, gboolean cut, gboolean icons);
void cf_selection_copy(GtkTreeView *view, gboolean cut);
void cf_selection_attach(GtkTreeView *view);
