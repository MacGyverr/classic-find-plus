#include "window-position.h"
#include "settings.h"
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <stdlib.h>

/* Pick the largest intersection, or nearest monitor if the saved monitor vanished.
 * All arithmetic precedes narrowing in 64 bits, including malformed INT bounds. */
gboolean cf_window_position_clamp(const GdkRectangle *areas, gsize count,
    int *x, int *y, int *width, int *height)
{
    gsize best = 0;
    gint64 overlap = -1, distance = G_MAXINT64;
    gboolean found = FALSE;
    for (gsize i = 0; i < count; i++) {
        if (areas[i].width <= 0 || areas[i].height <= 0) continue;
        gint64 ax = areas[i].x, ay = areas[i].y;
        gint64 right = ax + areas[i].width, bottom = ay + areas[i].height;
        gint64 ox = MAX((gint64)0, MIN((gint64)*x + MAX(1, *width), right) - MAX((gint64)*x, ax));
        gint64 oy = MAX((gint64)0, MIN((gint64)*y + MAX(1, *height), bottom) - MAX((gint64)*y, ay));
        /* Manhattan distance avoids squaring possibly extreme coordinates. */
        gint64 d = llabs((gint64)*x - CLAMP((gint64)*x, ax, right)) +
                   llabs((gint64)*y - CLAMP((gint64)*y, ay, bottom));
        if (ox * oy > overlap || (ox * oy == overlap && d < distance)) {
            best = i; overlap = ox * oy; distance = d; found = TRUE;
        }
    }
    if (!found) return FALSE;
    const GdkRectangle *a = &areas[best];
    *width = CLAMP(*width, 1, a->width); *height = CLAMP(*height, 1, a->height);
    *x = (int)CLAMP((gint64)*x, (gint64)a->x, (gint64)a->x + a->width - *width);
    *y = (int)CLAMP((gint64)*y, (gint64)a->y, (gint64)a->y + a->height - *height);
    return TRUE;
}

typedef struct {
    GtkWindow *window;
    CfSettings *settings;
    guint settle;
    gboolean armed, have_last, restore, seed;
    int x, y, target_x, target_y;
} Position;
static gboolean x11(GtkWindow *window)
{
#ifdef GDK_WINDOWING_X11
    return GDK_IS_X11_DISPLAY(gtk_widget_get_display(GTK_WIDGET(window)));
#else
    (void)window; return FALSE;
#endif
}
static gboolean normal(Position *p)
{
    GtkWidget *widget = GTK_WIDGET(p->window);
    GdkWindow *native = gtk_widget_get_window(widget);
    return gtk_widget_get_mapped(widget) && native &&
        !(gdk_window_get_state(native) & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN |
                                         GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_WITHDRAWN));
}
static void schedule(Position *p);
static gboolean settled(gpointer data)
{
    Position *p = data;
    p->settle = 0;
    if (!normal(p)) { p->armed = FALSE; return G_SOURCE_REMOVE; }
    int x, y;
    gtk_window_get_position(p->window, &x, &y);
    if (p->restore) {
        p->restore = FALSE;
        GdkWindow *native = gtk_widget_get_window(GTK_WIDGET(p->window));
        GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(p->window));
        int count = gdk_display_get_n_monitors(display);
        g_autofree GdkRectangle *areas = g_new0(GdkRectangle, count);
        for (int i = 0; i < count; i++) gdk_monitor_get_workarea(gdk_display_get_monitor(display, i), &areas[i]);
        GdkRectangle frame; gdk_window_get_frame_extents(native, &frame);
        int tx = p->target_x, ty = p->target_y, width = frame.width, height = frame.height;
        if (cf_window_position_clamp(areas, count, &tx, &ty, &width, &height)) {
            int client_width, client_height;
            gtk_window_get_size(p->window, &client_width, &client_height);
            gboolean resize = width < frame.width || height < frame.height;
            if (resize) gtk_window_resize(p->window,
                MAX(640, client_width - (frame.width - width)),
                MAX(420, client_height - (frame.height - height)));
            if (tx == x && ty == y && !resize) {
                p->armed = TRUE; p->have_last = TRUE; p->x = x; p->y = y;
                return G_SOURCE_REMOVE;
            }
            /* Some WMs include invisible frame margins in get_position but omit them
             * from initial move requests. Correct once using the measured offset,
             * then establish a baseline; never keep fighting WM placement. */
            gint64 nx = (gint64)p->target_x + tx - x;
            gint64 ny = (gint64)p->target_y + ty - y;
            gtk_window_move(p->window, (int)CLAMP(nx, G_MININT, G_MAXINT), (int)CLAMP(ny, G_MININT, G_MAXINT));
            schedule(p); return G_SOURCE_REMOVE;
        }
    }
    if (!p->armed) { /* A mapped baseline never overwrites an existing saved position. */
        p->armed = TRUE; p->have_last = TRUE; p->x = x; p->y = y;
        if (p->seed) {
            p->seed = FALSE;
            g_autoptr(GKeyFile) defaults = g_key_file_new();
            g_key_file_set_integer(defaults, "Window", "X", x);
            g_key_file_set_integer(defaults, "Window", "Y", y);
            /* First stable normal position only; lock-time defaults cannot overwrite
             * another window's newer position. Subsequent baselines never save. */
            cf_settings_defaults(p->settings, defaults);
            cf_settings_flush(p->settings, NULL);
        }
        return G_SOURCE_REMOVE;
    }
    if (p->have_last && x == p->x && y == p->y) return G_SOURCE_REMOVE;
    p->x = x; p->y = y; p->have_last = TRUE;
    cf_settings_set_window_position(p->settings, x, y);
    /* Commit the pair now: an older window cannot flush an older pending move at close. */
    cf_settings_flush(p->settings, NULL);
    return G_SOURCE_REMOVE;
}
static void schedule(Position *p)
{
    if (p->settle) g_source_remove(p->settle);
    p->settle = g_timeout_add(250, settled, p);
}
static gboolean configured(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    (void)widget; (void)event;
    Position *p = data;
    if (normal(p)) schedule(p);
    return FALSE;
}
static void mapped(GtkWidget *widget, gpointer data)
{
    (void)widget; Position *p = data; p->armed = FALSE; schedule(p);
}
static gboolean state_changed(GtkWidget *widget, GdkEventWindowState *event, gpointer data)
{
    (void)widget;
    Position *p = data;
    if (!(event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN |
                                 GDK_WINDOW_STATE_ICONIFIED | GDK_WINDOW_STATE_WITHDRAWN))) return FALSE;
    /* Focus changes must not disarm tracking when the user activates and drags. */
    /* Never persist a coordinate introduced by a geometry state transition. */
    p->armed = FALSE; schedule(p);
    return FALSE;
}
static void destroyed(GtkWidget *widget, gpointer data)
{
    Position *p = data;
    g_signal_handlers_disconnect_by_data(widget, p);
    if (p->settle) g_source_remove(p->settle);
    /* Save only captured changes, never the closing window's stale snapshot. */
    cf_settings_free(p->settings); g_free(p);
}
void cf_window_position_attach(GtkWindow *window)
{
    if (!x11(window)) return;
    Position *p = g_new0(Position, 1);
    p->window = window; p->settings = cf_settings_new();
    GKeyFile *keys = cf_settings_data(p->settings);
    p->seed = !g_key_file_has_key(keys, "Window", "X", NULL) && !g_key_file_has_key(keys, "Window", "Y", NULL);
    g_autoptr(GError) xe = NULL, ye = NULL;
    int x = g_key_file_get_integer(keys, "Window", "X", &xe);
    int y = g_key_file_get_integer(keys, "Window", "Y", &ye);
    if (!xe && !ye) {
        GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
        int count = gdk_display_get_n_monitors(display);
        g_autofree GdkRectangle *areas = g_new0(GdkRectangle, count);
        for (int i = 0; i < count; i++) gdk_monitor_get_workarea(gdk_display_get_monitor(display, i), &areas[i]);
        int width, height;
        gtk_window_get_default_size(window, &width, &height);
        /* Reserve typical decoration space before the WM has created the frame. */
        int outer_width = width + 16, outer_height = height + 40;
        if (cf_window_position_clamp(areas, count, &x, &y, &outer_width, &outer_height)) {
            gtk_window_set_default_size(window, MAX(640, MIN(width, outer_width - 16)),
                                                MAX(420, MIN(height, outer_height - 40)));
            p->restore = TRUE; p->target_x = x; p->target_y = y;
            gtk_window_move(window, x, y);
        }
    }
    g_signal_connect(window, "map", G_CALLBACK(mapped), p);
    g_signal_connect(window, "configure-event", G_CALLBACK(configured), p);
    g_signal_connect(window, "window-state-event", G_CALLBACK(state_changed), p);
    g_signal_connect(window, "destroy", G_CALLBACK(destroyed), p);
}
