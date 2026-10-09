#pragma once
#include <gtk/gtk.h>
#include "settings.h"
#define CF_RESULT_COLUMN_COUNT 15
typedef struct {
    gboolean visible[CF_RESULT_COLUMN_COUNT];
    int order[CF_RESULT_COLUMN_COUNT];
    int widths[CF_RESULT_COLUMN_COUNT];
    int sort;
    GtkSortType direction;
} CfColumnPreferences;
void cf_column_preferences_load(CfColumnPreferences *preferences);
void cf_column_preferences_save(const CfColumnPreferences *preferences, GError **error);

extern const char *const cf_column_ids[CF_RESULT_COLUMN_COUNT];
void cf_column_preferences_read(CfSettings *settings, CfColumnPreferences *preferences);
