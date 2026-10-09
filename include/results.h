#pragma once
#include <gtk/gtk.h>
#include "search.h"
typedef enum { CF_COL_NAME, CF_COL_FOLDER, CF_COL_SIZE_TEXT, CF_COL_TYPE,
               CF_COL_MODIFIED_TEXT, CF_COL_SIZE, CF_COL_MODIFIED,
               CF_COL_FILE, CF_COL_ICON, CF_COL_PATH, CF_COL_ID, CF_COL_CREATED_TEXT, CF_COL_ACCESSED_TEXT,
               CF_COL_PERMISSIONS_TEXT, CF_COL_CREATED, CF_COL_ACCESSED, CF_COL_PERMISSIONS, CF_COL_MIME, CF_COL_DETAIL, CF_COL_GROUP, CF_COL_OWNER, CF_COL_BYTES_TEXT, CF_COL_BITS_TEXT, CF_N_COLUMNS } CfResultColumn;
GtkListStore *cf_results_new(void);
GtkWidget *cf_results_view_new(GtkListStore *store);
void cf_results_append(GtkListStore *store, const CfResult *result);
void cf_results_replace(GtkListStore *store, GFile *previous, const CfResult *result);

/* Iterators are persistent GtkListStore iterators; callers resolve row references first. */
void cf_results_update(GtkListStore *store, GtkTreeIter *iter, const CfResult *result);
void cf_results_remove(GtkListStore *store, GFile *file);

/* Exact locale-independent decimal, including full UINT64_MAX × 8. */
char *cf_result_exact_size(guint64 bytes, gboolean bits);
