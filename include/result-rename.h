#pragma once
#include <gtk/gtk.h>
typedef void (*CfRenameCommit)(GtkTreeView *, GFile *, const char *, const char *);
void cf_result_rename_attach(GtkTreeView *view, CfRenameCommit commit);
void cf_result_rename_start(GtkTreeView *view, GtkTreePath *path);
/* Cancel the editor and pending click gesture before changing search generations. */
void cf_result_rename_cancel(GtkTreeView *view);
