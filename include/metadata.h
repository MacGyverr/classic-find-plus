#pragma once
#include <gio/gio.h>
typedef struct CfMetadataCache CfMetadataCache;
CfMetadataCache *cf_metadata_cache_new(void);
void cf_metadata_cache_free(CfMetadataCache *cache);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfMetadataCache, cf_metadata_cache_free)
/* Worker only: bounded, per-job NSS cache; never called by GTK renderers. */
void cf_metadata_prepare(CfMetadataCache *cache, GFileInfo *info);
/* No NSS lookup: provider name, numeric fallback, or blank. */
char *cf_metadata_owner(GFileInfo *info, gboolean group);

/* Provenance of the normalized display MIME: filename estimate or GIO backend
 * classification explicitly requested for a MIME filter. Not a wire attribute. */
#define CF_METADATA_MIME_SOURCE "classic-find::mime-source"
/* Uses supplied metadata/MIME database only; never opens the result's contents. */
void cf_metadata_prepare_type(GFileInfo *info, gboolean backend_type);
