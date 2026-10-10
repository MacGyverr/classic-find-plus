#pragma once
#include <gio/gio.h>
#include <sys/types.h>
#define CF_CONTENT_MAX_PATTERN (64 * 1024)
#define CF_CONTENT_MAX_INPUT (4 * CF_CONTENT_MAX_PATTERN)
#define CF_CONTENT_BUFFER (32 * 1024)
typedef enum { CF_CONTENT_TEXT, CF_CONTENT_HEX } CfContentMode;
typedef struct _CfContentPattern CfContentPattern;
CfContentPattern *cf_content_pattern_new(const char *input, CfContentMode mode,
                                         gboolean sensitive, GError **error);
CfContentPattern *cf_content_pattern_ref(CfContentPattern *pattern);
void cf_content_pattern_unref(CfContentPattern *pattern);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfContentPattern, cf_content_pattern_unref)
/* Read callback permits deterministic short-read/EINTR/cancellation fixtures.
 * Caller owns fd/data. A NULL pattern matches without calling the reader. */
typedef gssize (*CfContentRead)(gpointer data, void *buffer, gsize count);
gboolean cf_content_scan(CfContentPattern *pattern, CfContentRead read_func,
                         gpointer data, GCancellable *cancel, GError **error);
/* Native paths only. NULL pattern performs no filesystem operations. */
gboolean cf_content_match_file(CfContentPattern *pattern, GFile *file,
                               gboolean follow, GCancellable *cancel, GError **error);
gboolean cf_content_regular_mode(mode_t mode);
gboolean cf_content_safe_filesystem(long magic);

/* Optional worker-owned policy check on the pinned inode's resolved location,
 * before any data open. Device identifies the pinned target filesystem. */
typedef gboolean (*CfContentCheck)(GFile *target, guint64 device, gpointer data, GError **error);
gboolean cf_content_match_checked(CfContentPattern *pattern, GFile *file,
    gboolean follow, GCancellable *cancel, CfContentCheck check, gpointer data, GError **error);
