#pragma once
#include "results.h"

/* Per-view, bounded verification of recently dragged files; never performs transfers. */
void cf_results_sync_attach(GtkTreeView *view);
/* Takes ownership of an immutable snapshot; cancels the previous search generation. */
void cf_results_sync_set_search(GtkTreeView *view, GFile *root, CfCriteria *criteria);
guint cf_results_sync_generation(GtkTreeView *view);
void cf_results_sync_drag_begin(GtkTreeView *view, GPtrArray *files);
void cf_results_sync_drag_end(GtkTreeView *view, GPtrArray *files);
/* Apply a successful in-app rename against the captured search filters. */
void cf_results_sync_renamed(GtkTreeView *view, GFile *previous, const CfResult *result);
