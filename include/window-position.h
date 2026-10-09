#pragma once
#include <gtk/gtk.h>
/* Pure logical-coordinate clamp; returns FALSE for no usable workarea. */
gboolean cf_window_position_clamp(const GdkRectangle *areas, gsize count,
    int *x, int *y, int *width, int *height);
void cf_window_position_attach(GtkWindow *window);
