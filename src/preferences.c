#include "preferences.h"
#include "settings.h"

void cf_window_preferences_read(CfSettings *settings, CfWindowPreferences *p)
{
    p->width = cf_settings_int(settings, "Window", "Width", 760, 640, 8192);
    p->height = cf_settings_int(settings, "Window", "Height", 520, 420, 8192);
    p->divider = cf_settings_int(settings, "Window", "Divider", -1, 100, p->height - 140);
}
void cf_window_preferences_load(CfWindowPreferences *p)
{
    g_autoptr(CfSettings) settings = cf_settings_new(); cf_window_preferences_read(settings, p);
}
void cf_window_preferences_save(const CfWindowPreferences *p, GError **error)
{
    g_autoptr(CfSettings) settings = cf_settings_new();
    cf_settings_set_int(settings, "Window", "Width", p->width);
    cf_settings_set_int(settings, "Window", "Height", p->height);
    cf_settings_set_int(settings, "Window", "Divider", p->divider);
    cf_settings_flush(settings, error);
}
