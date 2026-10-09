#include "input-limits.h"
#include "date-dialog.h"
#include "date-expression.h"

typedef struct {
    GtkWidget *dialog, *calendar, *fixed_date, *relative_date, *date_base, *date_adjust,
              *specify_time, *fixed_time, *relative_time, *time_value, *time_base, *time_adjust,
              *date_spin[4], *time_spin[3], *expression, *evaluated;
    char *value;
} DateDialog;

static void free_dialog(gpointer data)
{
    DateDialog *d = data;
    g_free(d->value);
    g_free(d);
}

static GtkWidget *named(GtkWidget *widget, const char *name)
{
    gtk_widget_set_name(widget, name);
    return widget;
}

static GtkWidget *choices(const char *const *items, const char *name)
{
    GtkWidget *combo = named(gtk_combo_box_text_new(), name);
    for (guint i = 0; items[i]; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    return combo;
}

static gboolean enabled(GtkWidget *widget)
{
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static gboolean adjustment(GtkWidget *widget, int *value)
{
    const char *text = gtk_entry_get_text(GTK_ENTRY(widget));
    if (!cf_input_valid(text, CF_NUMBER_LIMIT) || !*text) return FALSE;
    const char *digits = (*text == '+' || *text == '-') ? text + 1 : text;
    if (!*digits || strspn(digits, "0123456789") != strlen(digits)) return FALSE;
    char *end = NULL;
    gint64 number = g_ascii_strtoll(text, &end, 10);
    if (*end || number < -1000000 || number > 1000000) return FALSE;
    *value = number; return TRUE;
}

static char *candidate(DateDialog *d)
{
    CfDateExpression e = { 0 };
    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(d->calendar), &year, &month, &day);
    e.year = year; e.month = month + 1; e.day = day;
    if (enabled(d->relative_date)) e.base = gtk_combo_box_get_active(GTK_COMBO_BOX(d->date_base)) + 1;
    e.adjust_date = enabled(d->date_adjust);
    int *date_values[] = { &e.days, &e.weeks, &e.months, &e.years };
    for (guint i = 0; e.adjust_date && i < 4; i++)
        if (!adjustment(d->date_spin[i], date_values[i])) return g_strdup("Invalid date adjustment");
    g_autofree char *date = cf_date_expression_format(&e);
    if (!enabled(d->specify_time)) return g_steal_pointer(&date);
    const char *const bases[] = { "Now", "Midnight", "Noon" };
    const char *time = enabled(d->fixed_time) ? gtk_entry_get_text(GTK_ENTRY(d->time_value)) :
                       bases[gtk_combo_box_get_active(GTK_COMBO_BOX(d->time_base))];
    if (!enabled(d->time_adjust)) return g_strdup_printf("%s @ %s", date, time);
    int values[3];
    for (guint i = 0; i < 3; i++)
        if (!adjustment(d->time_spin[i], &values[i])) return g_strdup("Invalid time adjustment");
    return g_strdup_printf("%s @ %s %+d hours %+d minutes %+d seconds", date, time, values[2], values[1], values[0]);
}

static void update(GtkWidget *widget, gpointer data)
{
    (void)widget;
    DateDialog *d = data;
    gtk_widget_set_sensitive(d->calendar, enabled(d->fixed_date));
    gtk_widget_set_sensitive(d->date_base, enabled(d->relative_date));
    for (guint i = 0; i < 4; i++) gtk_widget_set_sensitive(d->date_spin[i], enabled(d->date_adjust));
    gboolean time = enabled(d->specify_time);
    gtk_widget_set_sensitive(d->fixed_time, time);
    gtk_widget_set_sensitive(d->relative_time, time);
    gtk_widget_set_sensitive(d->time_value, time && enabled(d->fixed_time));
    gtk_widget_set_sensitive(d->time_base, time && enabled(d->relative_time));
    gtk_widget_set_sensitive(d->time_adjust, time);
    for (guint i = 0; i < 3; i++) gtk_widget_set_sensitive(d->time_spin[i], time && enabled(d->time_adjust));
    g_autofree char *expression = candidate(d);
    g_autoptr(GError) error = NULL;
    g_autoptr(GDateTime) result = cf_date_expression_resolve(expression, NULL, NULL, &error);
    g_free(d->value);
    d->value = result ? g_strdup(expression) : NULL;
    gtk_entry_set_text(GTK_ENTRY(d->expression), expression);
    g_autofree char *preview = result ? g_date_time_format(result, "%Y/%m/%d %H:%M:%S %Z") : NULL;
    gtk_entry_set_text(GTK_ENTRY(d->evaluated), preview ? preview : error->message);
    gtk_widget_set_tooltip_text(d->evaluated, preview ? preview : error->message);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(d->dialog), GTK_RESPONSE_OK, result != NULL);
}

static GtkWidget *section(GtkWidget *container, const char *title)
{
    GtkWidget *frame = gtk_frame_new(title);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    gtk_box_pack_start(GTK_BOX(container), frame, FALSE, FALSE, 0);
    return grid;
}

static void adjustments(GtkWidget *grid, GtkWidget **spins, const char *const *labels, guint count, const char *prefix)
{
    for (guint i = 0; i < count; i++) {
        GtkWidget *label = gtk_label_new(labels[i]);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), label, 2, i + 1, 1, 1);
        g_autofree char *name = g_strdup_printf("%s-%u", prefix, i);
        spins[i] = named(gtk_spin_button_new_with_range(-1000000, 1000000, 1), name);
        gtk_entry_set_width_chars(GTK_ENTRY(spins[i]), 7);
        gtk_entry_set_max_length(GTK_ENTRY(spins[i]), CF_NUMBER_LIMIT);
        gtk_grid_attach(GTK_GRID(grid), spins[i], 3, i + 1, 1, 1);
    }
}

GtkWidget *cf_date_dialog_new(GtkWindow *parent, const char *expression)
{
    DateDialog *d = g_new0(DateDialog, 1);
    d->dialog = named(gtk_dialog_new_with_buttons("Date/Time Selection", parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, NULL), "date-time-dialog");
    g_object_set_data_full(G_OBJECT(d->dialog), "date-dialog", d, free_dialog);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d->dialog));
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_box_set_spacing(GTK_BOX(box), 8);
    GtkWidget *date = section(box, "Date");
    d->fixed_date = named(gtk_radio_button_new_with_label(NULL, "Fixed Date:"), "fixed-date");
    d->relative_date = named(gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->fixed_date), "Relative Date:"), "relative-date");
    d->calendar = named(gtk_calendar_new(), "date-calendar");
    const char *const dates[] = { "Today", "Yesterday", "Tomorrow", "Start of week (Monday)", "Start of month", "Start of year", NULL };
    d->date_base = choices(dates, "relative-date-base");
    d->date_adjust = named(gtk_check_button_new_with_label("Adjust (+/-)"), "adjust-date");
    gtk_grid_attach(GTK_GRID(date), d->fixed_date, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(date), d->calendar, 1, 0, 1, 5);
    gtk_grid_attach(GTK_GRID(date), d->date_adjust, 2, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(date), d->relative_date, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(date), d->date_base, 1, 5, 1, 1);
    const char *const date_labels[] = { "Days:", "Weeks:", "Months:", "Years:" };
    adjustments(date, d->date_spin, date_labels, 4, "date-adjustment");

    GtkWidget *time = section(box, "Time");
    d->specify_time = named(gtk_check_button_new_with_label("Specify Time"), "specify-time");
    d->fixed_time = named(gtk_radio_button_new_with_label(NULL, "Fixed Time:"), "fixed-time");
    d->relative_time = named(gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->fixed_time), "Relative Time:"), "relative-time");
    d->time_value = named(gtk_entry_new(), "fixed-time-value");
    gtk_entry_set_width_chars(GTK_ENTRY(d->time_value), 9);
    gtk_entry_set_max_length(GTK_ENTRY(d->time_value), CF_NUMBER_LIMIT);
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->time_value), "HH:mm:ss (24h)");
    gtk_widget_set_tooltip_text(d->time_value, "24-hour local time: HH:mm:ss");
    const char *const time_labels[] = { "Now", "Midnight", "Noon", NULL };
    d->time_base = choices(time_labels, "relative-time-base");
    d->time_adjust = named(gtk_check_button_new_with_label("Adjust (+/-)"), "adjust-time");
    gtk_grid_attach(GTK_GRID(time), d->specify_time, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(time), d->fixed_time, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(time), d->time_value, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(time), d->relative_time, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(time), d->time_base, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(time), d->time_adjust, 2, 0, 2, 1);
    const char *const adjust_labels[] = { "Seconds:", "Minutes:", "Hours:" };
    adjustments(time, d->time_spin, adjust_labels, 3, "time-adjustment");
    GtkWidget *previews = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(previews), 10);
    gtk_grid_set_row_spacing(GTK_GRID(previews), 6);
    d->expression = named(gtk_entry_new(), "current-expression");
    d->evaluated = named(gtk_entry_new(), "evaluates-to");
    gtk_editable_set_editable(GTK_EDITABLE(d->expression), FALSE);
    gtk_editable_set_editable(GTK_EDITABLE(d->evaluated), FALSE);
    gtk_widget_set_hexpand(d->expression, TRUE);
    gtk_grid_attach(GTK_GRID(previews), gtk_label_new("Current expression:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(previews), d->expression, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(previews), gtk_label_new("Evaluates to:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(previews), d->evaluated, 1, 1, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), previews, FALSE, FALSE, 0);

    CfDateExpression e = { 0 };
    if (!cf_date_expression_parse(expression, &e, NULL)) {
        g_autoptr(GDateTime) now = g_date_time_new_now_local();
        e.year = g_date_time_get_year(now); e.month = g_date_time_get_month(now); e.day = g_date_time_get_day_of_month(now);
    }
    if (e.base == CF_DATE_FIXED) {
        gtk_calendar_select_month(GTK_CALENDAR(d->calendar), e.month - 1, e.year);
        gtk_calendar_select_day(GTK_CALENDAR(d->calendar), e.day);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e.base == CF_DATE_FIXED ? d->fixed_date : d->relative_date), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->date_base), e.base == CF_DATE_FIXED ? 0 : e.base - 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->date_adjust), e.adjust_date);
    int date_values[] = { e.days, e.weeks, e.months, e.years };
    int time_values[] = { e.seconds, e.minutes, e.hours };
    for (guint i = 0; i < 4; i++) gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->date_spin[i]), date_values[i]);
    for (guint i = 0; i < 3; i++) gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->time_spin[i]), time_values[i]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->specify_time), e.specify_time);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e.time_base == CF_TIME_FIXED ? d->fixed_time : d->relative_time), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(d->time_base), e.time_base == CF_TIME_FIXED ? 0 : e.time_base - 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->time_adjust), e.adjust_time);
    g_autofree char *fixed = g_strdup_printf("%02d:%02d:%02d", e.hour, e.minute, e.second);
    gtk_entry_set_text(GTK_ENTRY(d->time_value), fixed);
    GtkWidget *toggles[] = { d->fixed_date, d->relative_date, d->date_adjust, d->specify_time,
        d->fixed_time, d->relative_time, d->time_adjust };
    for (guint i = 0; i < G_N_ELEMENTS(toggles); i++) g_signal_connect(toggles[i], "toggled", G_CALLBACK(update), d);
    g_signal_connect(d->calendar, "day-selected", G_CALLBACK(update), d);
    g_signal_connect(d->date_base, "changed", G_CALLBACK(update), d);
    g_signal_connect(d->time_base, "changed", G_CALLBACK(update), d);
    g_signal_connect(d->time_value, "changed", G_CALLBACK(update), d);
    for (guint i = 0; i < 4; i++) g_signal_connect(d->date_spin[i], "changed", G_CALLBACK(update), d);
    for (guint i = 0; i < 3; i++) g_signal_connect(d->time_spin[i], "changed", G_CALLBACK(update), d);
    update(NULL, d);
    gtk_dialog_set_default_response(GTK_DIALOG(d->dialog), GTK_RESPONSE_OK);
    return d->dialog;
}

const char *cf_date_dialog_get_expression(GtkDialog *dialog)
{
    DateDialog *d = g_object_get_data(G_OBJECT(dialog), "date-dialog");
    return d->value;
}
