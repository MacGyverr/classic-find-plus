#ifndef CF_THUNAR_ADMIN_H
#define CF_THUNAR_ADMIN_H
#include <gio/gio.h>
/* Local absolute paths or local file URIs only. Caller owns the escaped URI. */
char *cf_thunar_admin_uri(const char *target, GError **error);
gboolean cf_thunar_admin_launch(const char *uri, const char *program,
                                const char *const *schemes, GError **error);
#endif
