#pragma once
#include "result-export.h"
/* Dialog owns its click-time snapshot. Parent closure cancels pending I/O. */
GtkWidget *cf_export_dialog_new(GtkWindow *parent, GtkTreeView *view, GFile *root);
/* Worker-safe atomic local-file output. NULL etag means exclusive creation.
 * Existing regular files require the version captured at overwrite confirmation. */
gboolean cf_export_write(CfExportSnapshot *snapshot, CfExportFormat format, GFile *file,
                         const char *etag, GCancellable *cancel, GError **error);

gboolean cf_export_dialog_is_writing(GtkWidget *dialog);
