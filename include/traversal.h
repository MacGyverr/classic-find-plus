#pragma once
#include "search.h"

/* A stable backend identity, independent of the path used to reach a directory. */
char *cf_traversal_identity(GFileInfo *info);
/* Check final resolved native target ancestors against captured pruning rules.
 * FALSE without an error means pruned; selected root itself is exempt. */
gboolean cf_traversal_target_allowed(const CfCriteria *criteria, GFile *root,
    GFile *directory, GCancellable *cancel, GError **error);

/* Recheck pinned content target scope before opening its data stream. */
gboolean cf_traversal_content_matches(const CfCriteria *criteria, GFile *root,
    GFile *file, GCancellable *cancel, GError **error);
