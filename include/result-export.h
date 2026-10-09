#pragma once
#include <gtk/gtk.h>

typedef enum { CF_EXPORT_CSV, CF_EXPORT_TEXT } CfExportFormat;
typedef struct _CfExportSnapshot CfExportSnapshot;
/* Main-thread only; owns displayed strings, never widgets/model iterators. */
CfExportSnapshot *cf_export_snapshot_new(GtkTreeView *view);
CfExportSnapshot *cf_export_snapshot_ref(CfExportSnapshot *snapshot);
void cf_export_snapshot_unref(CfExportSnapshot *snapshot);
guint cf_export_snapshot_rows(const CfExportSnapshot *snapshot);
guint cf_export_snapshot_columns(const CfExportSnapshot *snapshot);
/* Worker-safe, streams one record at a time. Caller owns closing/committing output. */
gboolean cf_export_serialize(const CfExportSnapshot *snapshot, CfExportFormat format,
                             GOutputStream *output, GCancellable *cancel, GError **error);
char *cf_export_filename(const char *name, CfExportFormat format);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfExportSnapshot, cf_export_snapshot_unref)
