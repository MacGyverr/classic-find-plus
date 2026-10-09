#include "date-input.h"
#include "input-limits.h"
#include <string.h>

static void insert(GtkEditable *editable, const char *text, int length, int *position, gpointer data)
{
    (void)data;
    if (g_object_get_data(G_OBJECT(editable), "date-input-formatting")) return;
    if ((length < 0 ? strlen(text) : (gsize)length) > CF_DATE_LIMIT * 4) {
        g_signal_stop_emission_by_name(editable, "insert-text"); return;
    }
    const char *old = gtk_entry_get_text(GTK_ENTRY(editable));
    gsize offset = g_utf8_offset_to_pointer(old, MIN(MAX(*position, 0), g_utf8_strlen(old, -1))) - old;
    g_autofree char *left = g_strndup(old, offset);
    g_autofree char *added = g_strndup(text, length < 0 ? strlen(text) : (gsize)length);
    g_autofree char *merged = g_strconcat(left, added, old + offset, NULL);
    /* The first token is either a numeric calendar date or a named relative base.
     * Keep incomplete edits; reject letters inside a numeric date, without touching
     * units and named times after the first space. */
    const char *space = strchr(merged, ' ');
    gsize base_length = space ? (gsize)(space - merged) : strlen(merged);
    gboolean valid = TRUE;
    if (*merged && g_ascii_isdigit(*merged)) {
        for (gsize i = 0; i < base_length; i++)
            if (!g_ascii_isdigit(merged[i]) && merged[i] != '/' && merged[i] != '-') valid = FALSE;
    } else if (*merged) {
        const char *bases[] = { "Today", "Yesterday", "Tomorrow", "StartOfWeek", "StartOfMonth", "StartOfYear" };
        valid = FALSE;
        for (guint i = 0; i < G_N_ELEMENTS(bases); i++)
            if (base_length <= strlen(bases[i]) && !strncmp(merged, bases[i], base_length)) valid = TRUE;
    }
    if (!valid) { g_signal_stop_emission_by_name(editable, "insert-text"); return; }
    char digits[9] = { 0 };
    guint count = 0, cursor_digits = 0;
    for (gsize i = 0; merged[i]; i++) {
        if (g_ascii_isdigit(merged[i])) {
            if (count == 8) return; /* Keep invalid input visible for validation; do not truncate. */
            digits[count++] = merged[i];
            if (i < offset + strlen(added)) cursor_digits++;
        } else if (merged[i] != '/' && merged[i] != '-') {
            return; /* Relative expressions and malformed input are not rewritten. */
        }
    }
    char formatted[11] = { 0 };
    guint out = 0, cursor = 0;
    for (guint i = 0; i < count; i++) {
        formatted[out++] = digits[i];
        if (i == 3 || i == 5) formatted[out++] = '/';
        if (i + 1 == cursor_digits) cursor = out;
    }
    g_signal_stop_emission_by_name(editable, "insert-text");
    g_object_set_data(G_OBJECT(editable), "date-input-formatting", GINT_TO_POINTER(1));
    gtk_entry_set_text(GTK_ENTRY(editable), formatted);
    g_object_set_data(G_OBJECT(editable), "date-input-formatting", NULL);
    *position = cursor;
    gtk_editable_set_position(editable, cursor);
}

void cf_date_input_attach(GtkEntry *entry)
{
    gtk_entry_set_max_length(entry, CF_DATE_LIMIT);
    gtk_entry_set_placeholder_text(entry, "yyyy/mm/dd");
    gtk_widget_set_tooltip_text(GTK_WIDGET(entry), "YYYY/MM/DD; typing digits inserts separators.");
    g_signal_connect(entry, "insert-text", G_CALLBACK(insert), NULL);
}
