#pragma once
#include <gio/gio.h>

typedef struct _CfSettings CfSettings;
CfSettings *cf_settings_new(void);
void cf_settings_free(CfSettings *settings);
GKeyFile *cf_settings_data(CfSettings *settings); /* borrowed, read-only */
char *cf_settings_path(void);
int cf_settings_int(CfSettings *settings, const char *group, const char *key, int fallback, int min, int max);
gboolean cf_settings_bool(CfSettings *settings, const char *group, const char *key, gboolean fallback);
void cf_settings_set_string(CfSettings *settings, const char *group, const char *key, const char *value);
void cf_settings_set_int(CfSettings *settings, const char *group, const char *key, int value);
void cf_settings_set_bool(CfSettings *settings, const char *group, const char *key, gboolean value);
void cf_settings_set_strings(CfSettings *settings, const char *group, const char *key, const char *const *values, gsize count);
gboolean cf_settings_flush(CfSettings *settings, GError **error);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfSettings, cf_settings_free)

/* History operations merge against the latest disk list under the same settings lock. */
char **cf_settings_history(CfSettings *settings); /* newly allocated, newest first */
void cf_settings_history_record(CfSettings *settings, const char *name);
void cf_settings_history_clear(CfSettings *settings);
/* Fill absent keys only; a concurrent writer's existing values always take precedence. */
void cf_settings_defaults(CfSettings *settings, GKeyFile *defaults);

/* One logical position: force both pending keys, even against stale local values. */
void cf_settings_set_window_position(CfSettings *settings, int x, int y);
