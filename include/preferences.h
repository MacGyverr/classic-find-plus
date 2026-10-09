#pragma once
#include <gtk/gtk.h>
#include "settings.h"

typedef struct {
    int width, height, divider;
} CfWindowPreferences;

void cf_window_preferences_load(CfWindowPreferences *preferences);
void cf_window_preferences_save(const CfWindowPreferences *preferences, GError **error);

void cf_window_preferences_read(CfSettings *settings, CfWindowPreferences *preferences);
