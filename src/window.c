#include "input-limits.h"
#include "classic-find.h"
#include "preferences.h"
#include "window-position.h"
#include "settings.h"
#include "results.h"
#include "results-sync.h"
#include "export-dialog.h"
#include "search-form.h"
#include "search-history.h"
#include "date-input.h"
#include "date-dialog.h"
#include "app-info.h"
#include "help-about.h"
#include "accounts.h"
#include <stdio.h>
#include <unistd.h>

typedef struct {
    GtkWidget *window, *paned, *tabs, *location, *name, *name_mode, *case_sensitive,
              *recursive, *stay, *date_mode, *date_start, *date_end, *recent,
              *extension, *extension_only, *type, *size_mode, *size_start,
              *size_end, *unit, *find, *stop, *status, *export, *export_dialog,
              *all_name, *all_location, *all_date_start, *all_date_end, *all_recent,
              *all_size_start, *all_size_end, *all_unit, *all_after, *all_before;
    CfSearchForm *form;
    CfSettings *settings;
    GtkWidget *more, *ignored, *admin_search;
    GtkListStore *store;
    GtkTreeModel *export_model;
    gulong export_inserted_id, export_removed_id;
    GtkWidget *view;
    CfSearch *search;
    GFile *results_root;
    guint timer, status_idle;
    guint64 examined, errors;
    gboolean has_search, cancelled, administrator_search;
    char *first_error;
    CfWindowPreferences preferences;
    gboolean maximized, fullscreen, iconified, updating_bounds;
} Window;

static void find(GtkButton *button, gpointer data);
static const char *const name_modes[] = { "Partial / wildcard", "Exact name", "RegEx", NULL };
static const char *const types[] = { "Any type", "Regular files", "Folders", "Images", "Audio", "Video", "Text", NULL };
static const char *const units[] = { "bytes", "KiB", "MiB", "GiB", NULL };

static GtkWidget *grid_new(void)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    return grid;
}

static GtkWidget *entry_new(const char *text, const char *id)
{
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), CF_TEXT_LIMIT);
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_widget_set_name(entry, id);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 12);
    gtk_widget_set_hexpand(entry, TRUE);
    return entry;
}

static void field(GtkWidget *grid, const char *text, GtkWidget *widget, int row)
{
    GtkWidget *label = gtk_label_new_with_mnemonic(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, row, 1, 1);
}

static GtkWidget *combo(const char *const *items)
{
    GtkWidget *widget = gtk_combo_box_text_new();
    for (guint i = 0; items[i]; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
    return widget;
}

static GtkWidget *tab_label(const char *icon_name, const char *title)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    GtkWidget *label = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    /* GtkNotebook does not recurse into custom tab headers when showing pages. */
    gtk_widget_show_all(box);
    return box;
}

static const char *text(GtkWidget *widget)
{
    return gtk_entry_get_text(GTK_ENTRY(widget));
}

static void bind_control(Window *w, const char *property, GtkWidget *widget, const char *widget_property)
{
    if (GTK_IS_ENTRY(widget)) gtk_entry_set_max_length(GTK_ENTRY(widget), cf_input_limit(property));
    g_object_bind_property(w->form, property, widget, widget_property,
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    if (GTK_IS_ENTRY(widget)) g_signal_connect(widget, "activate", G_CALLBACK(find), w);
}

static void status(Window *w, const char *message, const char *detail)
{
    gtk_label_set_text(GTK_LABEL(w->status), message);
    gtk_widget_set_tooltip_text(w->status, detail ? detail : message);
}

static void browse(GtkButton *button, gpointer data)
{
    (void)button;
    Window *w = data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Choose search folder", GTK_WINDOW(w->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL,
        "_Select", GTK_RESPONSE_ACCEPT, NULL);
    g_autoptr(GFile) current = g_file_new_for_commandline_arg(text(w->location));
    gtk_file_chooser_set_current_folder_file(GTK_FILE_CHOOSER(dialog), current, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_autoptr(GFile) folder = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (folder) {
            g_autofree char *name = g_file_get_parse_name(folder);
            gtk_entry_set_text(GTK_ENTRY(w->location), name);
        }
    }
    gtk_widget_destroy(dialog);
}

static void select_date(GtkButton *button, gpointer data)
{
    (void)button;
    GtkWidget *entry = data;
    GtkWidget *dialog = cf_date_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(entry)), text(entry));
    g_object_ref(dialog);
    g_object_ref(entry);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK &&
        !gtk_widget_in_destruction(dialog) && cf_date_dialog_get_expression(GTK_DIALOG(dialog))) {
        gtk_entry_set_text(GTK_ENTRY(entry), cf_date_dialog_get_expression(GTK_DIALOG(dialog)));
    }
    gtk_widget_destroy(dialog);
    g_object_unref(entry);
    g_object_unref(dialog);
}

static GtkWidget *date_entry(GtkWidget **entry, const char *date, const char *id, gboolean compact)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    *entry = entry_new(date, id);
    cf_date_input_attach(GTK_ENTRY(*entry));
    GtkWidget *button = compact ? gtk_button_new_from_icon_name("x-office-calendar-symbolic", GTK_ICON_SIZE_BUTTON) :
                                 gtk_button_new_with_label("Choose…");
    gtk_widget_set_tooltip_text(button, "Choose date/time or a relative expression");
    if (compact) {
        gtk_entry_set_width_chars(GTK_ENTRY(*entry), 10);
        gtk_entry_set_max_width_chars(GTK_ENTRY(*entry), 10);
        gtk_widget_set_hexpand(*entry, FALSE);
    }
    g_autofree char *button_id = g_strdup_printf("%s-calendar", id);
    gtk_widget_set_name(button, button_id);
    g_signal_connect(button, "clicked", G_CALLBACK(select_date), *entry);
    gtk_box_pack_start(GTK_BOX(box), *entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *compact_entry(Window *w, const char *property, const char *id, int width)
{
    GtkWidget *entry = entry_new("", id);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), width);
    gtk_entry_set_max_width_chars(GTK_ENTRY(entry), width);
    gtk_widget_set_hexpand(entry, FALSE);
    bind_control(w, property, entry, "text");
    return entry;
}

/* Stable numeric IDs preserve settings while menu presentation can change. */
static gboolean date_to_id(GBinding *binding, const GValue *from, GValue *to, gpointer data)
{
    (void)binding; (void)data;
    g_value_take_string(to, g_strdup_printf("%d", g_value_get_int(from)));
    return TRUE;
}
static gboolean id_to_date(GBinding *binding, const GValue *from, GValue *to, gpointer data)
{
    (void)binding; (void)data;
    const char *id = g_value_get_string(from);
    if (!id) return FALSE;
    g_value_set_int(to, (int)g_ascii_strtoll(id, NULL, 10));
    return TRUE;
}
static void bind_date_mode(Window *w, const char *property, GtkWidget *widget)
{
    g_object_bind_property_full(w->form, property, widget, "active-id",
        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE, date_to_id, id_to_date, NULL, NULL);
}
static GtkWidget *date_mode_combo(gboolean optional)
{
    const char *labels[] = { "Any date", "After date (exclusive)", "Before date (exclusive)",
        "Between dates (inclusive)", "Last N days", "Last N months", "After and before (exclusive)",
        "Last N minutes", "Last N hours" };
    const CfDateMode order[] = { CF_DATE_ALL, CF_DATE_AFTER, CF_DATE_BEFORE, CF_DATE_BETWEEN,
        CF_DATE_MINUTES, CF_DATE_HOURS, CF_DATE_DAYS, CF_DATE_MONTHS, CF_DATE_BOUNDS };
    GtkWidget *widget = gtk_combo_box_text_new();
    for (guint i = 0; i < G_N_ELEMENTS(order); i++) {
        int mode = order[i];
        if (optional && mode != CF_DATE_ALL && mode != CF_DATE_MINUTES && mode != CF_DATE_HOURS &&
            mode != CF_DATE_DAYS && mode != CF_DATE_MONTHS) continue;
        g_autofree char *id = g_strdup_printf("%d", mode);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widget), id, labels[mode]);
    }
    return widget;
}

static GtkWidget *date_field_combo(Window *w, const char *name)
{
    const char *fields[] = { "Modified", "Created", "Accessed", NULL };
    GtkWidget *widget = combo(fields);
    gtk_widget_set_name(widget, name);
    gtk_widget_set_tooltip_text(widget, "Created searches use Modified only when birth metadata is absent. Accessed depends on filesystem mount options.");
    bind_control(w, "date-field", widget, "active");
    return widget;
}

static void bounds_changed(GtkToggleButton *button, gpointer data)
{
    (void)button;
    Window *w = data;
    if (w->updating_bounds) return;
    gboolean after = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->all_after));
    gboolean before = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->all_before));
    g_object_set(w->form, "date-mode", after && before ? CF_DATE_BOUNDS :
                 after ? CF_DATE_AFTER : before ? CF_DATE_BEFORE : CF_DATE_ALL, NULL);
}

static void filter_changed(GObject *form, GParamSpec *spec, gpointer data);

static void more_changed(GObject *object, GParamSpec *spec, gpointer data)
{
    (void)spec;
    Window *w = data;
    gboolean expanded = gtk_expander_get_expanded(GTK_EXPANDER(object));
    gtk_widget_set_visible(w->ignored, !expanded);
    if (!expanded) {
        cf_search_form_reset_optional(w->form);
        if (!w->search && w->status) status(w, "More Filters reset; ready for the next search.", NULL);
    }
    if (expanded) cf_accounts_load();
    else {
        GtkWidget *extra = gtk_bin_get_child(GTK_BIN(w->more));
        /* Reset account selections too when the current UID/GID is not enumerated. */
        GtkWidget *group = g_object_get_data(G_OBJECT(extra), "cf-group");
        GtkWidget *user = g_object_get_data(G_OBJECT(extra), "cf-user");
        if (group) cf_accounts_default(GTK_COMBO_BOX_TEXT(group));
        if (user) cf_accounts_default(GTK_COMBO_BOX_TEXT(user));
    }
    cf_settings_set_bool(w->settings, "FindPlus", "Expanded", expanded);
    filter_changed(NULL, NULL, w);
}

/* Checkbox labels double as accessible labels for compact optional inputs. */
static GtkWidget *optional_row(GtkWidget *grid, int column, int row,
    const char *label, GtkWidget *input, const char *id)
{
    GtkWidget *check = gtk_check_button_new_with_mnemonic(label);
    gtk_widget_set_name(check, id);
    gtk_widget_set_halign(check, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), check, column, row, 1, 1);
    if (input) {
        gtk_widget_set_hexpand(input, TRUE);
        gtk_grid_attach(GTK_GRID(grid), input, column + 1, row, 1, 1);
        g_object_bind_property(check, "active", input, "sensitive", G_BINDING_SYNC_CREATE);
        atk_object_add_relationship(gtk_widget_get_accessible(input), ATK_RELATION_LABELLED_BY,
                                    gtk_widget_get_accessible(check));
    }
    return check;
}

static GtkWidget *all_in_one(Window *w)
{
    GtkWidget *grid = grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 6);
    /* Local padding only; keep theme fonts and theme minimum control heights. */
    GdkScreen *screen=gtk_widget_get_screen(grid);
    if (!g_object_get_data(G_OBJECT(screen),"cf-compact-style")) {
        GtkCssProvider *compact = gtk_css_provider_new();
        gtk_css_provider_load_from_data(compact, "#all-in-one entry, #all-in-one button { padding-top: 1px; padding-bottom: 1px; }", -1, NULL);
        gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(compact), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(compact);
        g_object_set_data(G_OBJECT(screen),"cf-compact-style",GINT_TO_POINTER(1));
    }
    gtk_widget_set_name(grid, "all-in-one");
    w->all_name = entry_new("", "all-name");
    bind_control(w, "name", w->all_name, "text");
    cf_search_history_attach(GTK_ENTRY(w->all_name), w->form);
    field(grid, "File _name:", w->all_name, 0);
    GtkWidget *options = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *mode = combo(name_modes);
    gtk_widget_set_name(mode, "all-name-mode");
    gtk_widget_set_tooltip_text(mode, "File name matching mode");
    bind_control(w, "name-mode", mode, "active");
    GtkWidget *case_sensitive = gtk_toggle_button_new_with_label("Aa");
    gtk_widget_set_name(case_sensitive, "all-case-sensitive");
    gtk_widget_set_tooltip_text(case_sensitive, "Match file name case");
    bind_control(w, "case-sensitive", case_sensitive, "active");
    gtk_box_pack_start(GTK_BOX(options), mode, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(options), case_sensitive, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), options, 2, 0, 1, 1);

    GtkWidget *exclude = entry_new("", "all-exclude");
    bind_control(w, "exclude", exclude, "text");
    gtk_widget_set_tooltip_text(exclude, "Use | for OR exclusions; \\| is a literal pipe. / targets directories. RegEx uses one native expression.");
    field(grid, "_Exclude:", exclude, 1);

    GtkWidget *containing = entry_new("", "containing-text");
    gtk_entry_set_placeholder_text(GTK_ENTRY(containing), "Not implemented");
    bind_control(w, "containing-text", containing, "text");
    gtk_editable_set_editable(GTK_EDITABLE(containing), FALSE);
    gtk_widget_set_sensitive(containing, FALSE);
    gtk_widget_set_tooltip_text(containing, "Content search is not implemented; this field does not filter results.");
    GtkWidget *label = gtk_label_new_with_mnemonic("Containing _text:");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), containing);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), containing, 1, 2, 2, 1);

    w->all_location = entry_new("", "all-location");
    bind_control(w, "location", w->all_location, "text");
    field(grid, "_Look in:", w->all_location, 3);
    GtkWidget *location_options = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *recursive = gtk_check_button_new_with_label("Subfolders");
    gtk_widget_set_name(recursive, "all-recursive");
    bind_control(w, "recursive", recursive, "active");
    GtkWidget *button = gtk_button_new_from_icon_name("folder-open-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_name(button, "all-browse");
    gtk_widget_set_tooltip_text(button, "Choose search folder");
    g_signal_connect(button, "clicked", G_CALLBACK(browse), w);
    gtk_box_pack_start(GTK_BOX(location_options), recursive, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(location_options), button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), location_options, 2, 3, 1, 1);

    GtkWidget *size = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *size_modes[] = { "Any size", ">", "<", "Between", "=", NULL };
    mode = combo(size_modes);
    gtk_widget_set_name(mode, "all-size-mode");
    gtk_widget_set_tooltip_text(mode, "Size of regular files: any, greater than, less than, equal to, or strictly between.");
    bind_control(w, "size-mode", mode, "active");
    w->all_size_start = compact_entry(w, "size-start", "all-size-start", 8);
    w->all_size_end = compact_entry(w, "size-end", "all-size-end", 8);
    w->all_unit = combo(units);
    gtk_widget_set_name(w->all_unit, "all-size-unit");
    bind_control(w, "size-unit", w->all_unit, "active");
    gtk_box_pack_start(GTK_BOX(size), gtk_label_new("Size:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(size), mode, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(size), w->all_size_start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(size), gtk_label_new("to"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(size), w->all_size_end, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(size), w->all_unit, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), size, 0, 4, 3, 1);

    GtkWidget *modified = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    mode = date_mode_combo(TRUE);
    gtk_widget_set_name(mode, "all-date-mode");
    gtk_widget_set_tooltip_text(mode, "Dates use YYYY/MM/DD and local midnight. Previous periods end when Find starts.");
    bind_date_mode(w, "plus-date-mode", mode);
    GtkWidget *start = date_entry(&w->all_date_start, "", "all-date-start", TRUE);
    GtkWidget *end = date_entry(&w->all_date_end, "", "all-date-end", TRUE);
    bind_control(w, "date-start", w->all_date_start, "text");
    bind_control(w, "date-end", w->all_date_end, "text");
    w->all_recent = compact_entry(w, "plus-recent", "all-recent", 3);
    gtk_widget_set_tooltip_text(w->all_recent, "Positive whole N: elapsed minutes, hours, 24-hour days, or calendar months");
    gtk_box_pack_start(GTK_BOX(modified), date_field_combo(w, "all-date-field"), FALSE, FALSE, 0);
    w->all_after = gtk_toggle_button_new_with_label("After:");
    w->all_before = gtk_toggle_button_new_with_label("Before:");
    gtk_widget_set_name(w->all_after, "all-after");
    gtk_widget_set_name(w->all_before, "all-before");
    gtk_widget_set_tooltip_text(w->all_after, "Exclude this calendar day and earlier days.");
    gtk_widget_set_tooltip_text(w->all_before, "Exclude this calendar day and later days.");
    g_signal_connect(w->all_after, "toggled", G_CALLBACK(bounds_changed), w);
    g_signal_connect(w->all_before, "toggled", G_CALLBACK(bounds_changed), w);
    gtk_box_pack_start(GTK_BOX(modified), w->all_after, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(modified), start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(modified), w->all_before, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(modified), end, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), modified, 0, 5, 3, 1);

    GtkWidget *more = w->more = gtk_expander_new(NULL);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *title = gtk_label_new_with_mnemonic("More _filters");
    gtk_label_set_mnemonic_widget(GTK_LABEL(title), more);
    w->ignored = gtk_label_new("Reset while collapsed");
    gtk_widget_set_name(w->ignored, "more-ignored");
    gtk_style_context_add_class(gtk_widget_get_style_context(w->ignored), "dim-label");
    gtk_widget_set_no_show_all(w->ignored, TRUE);
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), w->ignored, FALSE, FALSE, 0);
    gtk_widget_show_all(header);
    gtk_expander_set_label_widget(GTK_EXPANDER(more), header);
    gboolean expanded = cf_settings_bool(w->settings, "FindPlus", "Expanded", FALSE);
    gtk_expander_set_expanded(GTK_EXPANDER(more), expanded);
    gtk_widget_set_visible(w->ignored, !expanded);
    g_signal_connect(more, "notify::expanded", G_CALLBACK(more_changed), w);
    gtk_widget_set_name(more, "all-more-filters");
    GtkWidget *extra = grid_new();
    gtk_widget_set_name(extra, "all-extra-grid");
    gtk_grid_set_row_spacing(GTK_GRID(extra), 2);
    gtk_grid_set_column_spacing(GTK_GRID(extra), 8);
    gtk_container_set_border_width(GTK_CONTAINER(extra), 4);
    GtkWidget *type = combo(types);
    gtk_widget_set_name(type, "all-file-type");
    bind_control(w, "plus-file-type", type, "active");
    GtkWidget *enable_type = optional_row(extra, 0, 0, "File t_ype:", type, "all-enable-type");
    bind_control(w, "plus-type-enabled", enable_type, "active");
    GtkWidget *extension = entry_new("", "all-extension");
    gtk_entry_set_width_chars(GTK_ENTRY(extension), 12);
    gtk_entry_set_placeholder_text(GTK_ENTRY(extension), "jpg or tar.gz");
    bind_control(w, "plus-extension", extension, "text");
    GtkWidget *enable_extension = optional_row(extra, 0, 1, "_Extension:", extension, "all-enable-extension");
    bind_control(w, "plus-extension-enabled", enable_extension, "active");
    GtkWidget *enable_date = optional_row(extra, 0, 2, "Date _mode:", mode, "all-enable-date");
    bind_control(w, "plus-date-enabled", enable_date, "active");
    GtkWidget *recent_label = gtk_label_new_with_mnemonic("Previous _N:");
    gtk_label_set_mnemonic_widget(GTK_LABEL(recent_label), w->all_recent);
    gtk_widget_set_halign(recent_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(extra), recent_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(extra), w->all_recent, 1, 3, 1, 1);
    GtkWidget *group = gtk_combo_box_text_new();
    gtk_widget_set_name(group, "all-group");
    GtkWidget *user = gtk_combo_box_text_new();
    gtk_widget_set_name(user, "all-user");
    GtkWidget *permissions = entry_new("", "all-permissions");
    gtk_entry_set_placeholder_text(GTK_ENTRY(permissions), "0644 or 4755");
    const char *ids[] = { "all-enable-group", "all-enable-user", "all-enable-permissions" };
    GtkWidget *inputs[] = { group, user, permissions };
    const char *labels[] = { "_Group:", "_User:", "_Permissions:" };
    for (guint i = 0; i < 3; i++) {
        GtkWidget *check = optional_row(extra, 2, i, labels[i], inputs[i], ids[i]);
        const char *props[] = { "plus-group", "plus-user", "plus-permissions" };
        const char *enabled[] = { "plus-group-enabled", "plus-user-enabled", "plus-permissions-enabled" };
        bind_control(w, props[i], inputs[i], i < 2 ? "active-id" : "text");
        bind_control(w, enabled[i], check, "active");
        if (i < 2) {
            GList *cells=gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(inputs[i]));
            for(GList *cell=cells;cell;cell=cell->next)
                if(GTK_IS_CELL_RENDERER_TEXT(cell->data)) g_object_set(cell->data,"ellipsize",PANGO_ELLIPSIZE_END,"max-width-chars",18,NULL);
            g_list_free(cells);
            cf_accounts_register(GTK_COMBO_BOX_TEXT(inputs[i]), check, i == 1);
        }
    }
    g_object_set_data(G_OBJECT(extra), "cf-group", group);
    g_object_set_data(G_OBJECT(extra), "cf-user", user);
    if (expanded) cf_accounts_load();
    GtkWidget *hidden = optional_row(extra, 2, 3, "No _Hidden files", NULL, "all-no-hidden");
    bind_control(w, "plus-no-hidden", hidden, "active");
    GtkWidget *traversal = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *stay = gtk_check_button_new_with_label("Stay on this filesystem");
    gtk_widget_set_name(stay, "all-stay-on-filesystem");
    bind_control(w, "stay-on-filesystem", stay, "active");
    GtkWidget *follow = gtk_check_button_new_with_label("Follow symbolic links");
    gtk_widget_set_name(follow, "all-follow-symlinks");
    bind_control(w, "follow-symlinks", follow, "active");
    w->admin_search = gtk_check_button_new_with_label("Allow admin search");
    gtk_widget_set_name(w->admin_search, "all-admin-search");
    gtk_widget_set_tooltip_text(w->admin_search,
        "Authenticate through Polkit when Find is pressed. Search only; file actions retain this application's permissions.");
    gtk_box_pack_start(GTK_BOX(traversal), stay, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(traversal), follow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(traversal), w->admin_search, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), traversal, 0, 6, 3, 1);
    gtk_container_add(GTK_CONTAINER(more), extra);
    gtk_grid_attach(GTK_GRID(grid), more, 0, 7, 3, 1);
    return grid;
}

static void filter_changed(GObject *form, GParamSpec *spec, gpointer data)
{
    (void)form;
    (void)spec;
    Window *w = data;
    CfCriteriaInput input = cf_search_form_get_input(w->form);
    int optional_mode; gboolean enabled;
    g_object_get(w->form, "plus-date-mode", &optional_mode, "plus-date-enabled", &enabled, NULL);
    gtk_widget_set_sensitive(w->all_recent, enabled && cf_date_mode_relative(optional_mode) &&
        gtk_expander_get_expanded(GTK_EXPANDER(w->more)) &&
        gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs)) == CF_TAB_PLUS);
    w->updating_bounds = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->all_after), input.date_mode == CF_DATE_AFTER ||
                                 input.date_mode == CF_DATE_BETWEEN || input.date_mode == CF_DATE_BOUNDS);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->all_before), input.date_mode == CF_DATE_BEFORE ||
                                 input.date_mode == CF_DATE_BETWEEN || input.date_mode == CF_DATE_BOUNDS);
    w->updating_bounds = FALSE;
    GtkWidget *starts[] = { w->date_start, w->all_date_start };
    GtkWidget *ends[] = { w->date_end, w->all_date_end };
    GtkWidget *recent[] = { w->recent, NULL };
    GtkWidget *lower[] = { w->size_start, w->all_size_start };
    GtkWidget *upper[] = { w->size_end, w->all_size_end };
    GtkWidget *units[] = { w->unit, w->all_unit };
    for (guint i = 0; i < G_N_ELEMENTS(starts); i++) {
        gtk_widget_set_sensitive(gtk_widget_get_parent(starts[i]),
                                input.date_mode == CF_DATE_AFTER || input.date_mode == CF_DATE_BETWEEN || input.date_mode == CF_DATE_BOUNDS);
        gtk_widget_set_sensitive(gtk_widget_get_parent(ends[i]), input.date_mode == CF_DATE_BEFORE || input.date_mode == CF_DATE_BETWEEN || input.date_mode == CF_DATE_BOUNDS);
        if (recent[i]) gtk_widget_set_sensitive(recent[i], cf_date_mode_relative(input.date_mode));
        gtk_widget_set_sensitive(lower[i], input.size_mode != CF_SIZE_ALL);
        gtk_widget_set_sensitive(upper[i], input.size_mode == CF_SIZE_BETWEEN);
        gtk_widget_set_sensitive(units[i], input.size_mode != CF_SIZE_ALL);
    }
}

static void stop(GtkButton *button, gpointer data)
{
    (void)button;
    Window *w = data;
    if (w->search) cf_search_cancel(w->search);
    gtk_widget_set_sensitive(w->stop, FALSE);
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    (void)widget;
    Window *w = data;
    gboolean control = (event->state & GDK_CONTROL_MASK) != 0;
    if (control && (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F)) {
        gboolean all = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs)) == 3;
        if (!all) gtk_notebook_set_current_page(GTK_NOTEBOOK(w->tabs), 0);
        GtkWidget *name = all ? w->all_name : w->name;
        gtk_widget_grab_focus(name);
        gtk_editable_select_region(GTK_EDITABLE(name), 0, -1);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L)) {
        gboolean all = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs)) == 3;
        if (!all) gtk_notebook_set_current_page(GTK_NOTEBOOK(w->tabs), 0);
        GtkWidget *location = all ? w->all_location : w->location;
        gtk_widget_grab_focus(location);
        gtk_editable_select_region(GTK_EDITABLE(location), 0, -1);
        return TRUE;
    }
    if (control && event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_5) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(w->tabs), event->keyval - GDK_KEY_1);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_Page_Down)) {
        int count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->tabs));
        int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs));
        gtk_notebook_set_current_page(GTK_NOTEBOOK(w->tabs), (page + count +
            (event->keyval == GDK_KEY_Page_Down ? 1 : -1)) % count);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_F5 || (control && event->keyval == GDK_KEY_Return)) {
        find(NULL, w);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Escape && w->search) {
        stop(NULL, w);
        return TRUE;
    }
    return FALSE;
}

static void tab_selected(GtkNotebook *tabs, GtkWidget *page, guint index, gpointer data)
{
    (void)tabs; (void)page;
    Window *w = data;
    gtk_widget_set_sensitive(w->find, !w->search && index != CF_TAB_HELP);
    cf_settings_set_int(w->settings, "Window", "Tab", index);
    /* switch-page precedes the notebook current-page update. */
    int mode; gboolean enabled;
    g_object_get(w->form, "plus-date-mode", &mode, "plus-date-enabled", &enabled, NULL);
    gtk_widget_set_sensitive(w->all_recent, index == CF_TAB_PLUS && enabled &&
        cf_date_mode_relative(mode) && gtk_expander_get_expanded(GTK_EXPANDER(w->more)));
}

static gboolean configured(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    Window *w = data;
    GdkWindow *native = gtk_widget_get_window(widget);
    if (!gtk_widget_get_mapped(widget) || (native && (gdk_window_get_state(native) &
        (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN | GDK_WINDOW_STATE_ICONIFIED)))) return FALSE;
    /* Some compositors ignore both GTK and legacy geometry minimums. Reassert them on configure. */
    if (!w->maximized && !w->fullscreen && !w->iconified && (event->width < 640 || event->height < 420)) {
        gtk_window_resize(GTK_WINDOW(widget), MAX(event->width, 640), MAX(event->height, 420));
        return FALSE;
    }
    if (!w->maximized && !w->fullscreen && !w->iconified && event->width >= 640 && event->height >= 420) {
        if (w->preferences.width != event->width)
            cf_settings_set_int(w->settings, "Window", "Width", event->width);
        if (w->preferences.height != event->height)
            cf_settings_set_int(w->settings, "Window", "Height", event->height);
        w->preferences.width = event->width;
        w->preferences.height = event->height;
    }
    return FALSE;
}

static gboolean window_state(GtkWidget *widget, GdkEventWindowState *event, gpointer data)
{
    (void)widget;
    Window *w = data;
    gboolean maximized = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    if (w->maximized != maximized) cf_settings_set_bool(w->settings, "Window", "Maximized", maximized);
    w->maximized = maximized;
    w->fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    w->iconified = (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
    return FALSE;
}

static void divider_changed(GObject *object, GParamSpec *property, gpointer data)
{
    (void)property;
    Window *w = data;
    int position = gtk_paned_get_position(GTK_PANED(object));
    if (position >= 100 && position <= w->preferences.height - 140 && position != w->preferences.divider) {
        w->preferences.divider = position;
        cf_settings_set_int(w->settings, "Window", "Divider", position);
    }
}

static void search_status(Window *w)
{
    const char *state = w->search ? (w->cancelled ? "Stopping" : "Searching") :
                                  (w->cancelled ? "Stopped" : "Finished");
    guint count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(w->store), NULL);
    g_autofree char *message = g_strdup_printf("%s — %s%u results, %" G_GUINT64_FORMAT
        " examined, %" G_GUINT64_FORMAT " errors%s", state,
        w->administrator_search ? "Administrator search; " : "", count, w->examined, w->errors,
        w->errors ? " (hover for first error)" : "");
    status(w, message, w->first_error);
}
static gboolean result_count_changed(gpointer data)
{
    Window *w = data;
    w->status_idle = 0;
    if (w->has_search && !w->search) search_status(w);
    return G_SOURCE_REMOVE;
}
static void row_removed(GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
    (void)model; (void)path;
    Window *w = data;
    if (!w->search && w->has_search && !w->status_idle)
        w->status_idle = g_idle_add(result_count_changed, w);
}

static gboolean poll(gpointer data)
{
    Window *w = data;
    /* Bound work both by row count and main-loop wall time. */
    gint64 deadline = g_get_monotonic_time() + 8000;
    for (guint i = 0; i < 128 && g_get_monotonic_time() < deadline; i++) {
        g_autoptr(CfResult) result = cf_search_pop(w->search);
        if (!result) break;
        cf_results_append(w->store, result);
    }
    CfSearchStats stats;
    cf_search_get_stats(w->search, &stats);
    w->examined = stats.scanned;
    w->errors = stats.errors;
    w->cancelled = stats.cancelled;
    g_free(w->first_error);
    w->first_error = stats.first_error;
    search_status(w);
    if (!stats.finished) return G_SOURCE_CONTINUE;
    cf_search_unref(w->search);
    w->search = NULL;
    w->timer = 0;
    search_status(w);
    gtk_widget_set_sensitive(w->tabs, TRUE);
    gtk_widget_set_sensitive(w->find, TRUE);
    gtk_widget_set_sensitive(w->stop, FALSE);
    return G_SOURCE_REMOVE;
}

static void find(GtkButton *button, gpointer data)
{
    (void)button;
    Window *w = data;
    if (w->search) return;
    g_autoptr(GDateTime) reference = g_date_time_new_now_local();
    CfCriteriaInput input;
    if (!cf_search_form_effective(w->form, gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs)),
                                  gtk_expander_get_expanded(GTK_EXPANDER(w->more)), &input)) return;
    g_autoptr(GError) error = NULL;
    CfCriteria *criteria = cf_criteria_new(&input, reference, &error);
    if (!criteria) {
        status(w, error->message, NULL);
        return;
    }
    const char *location = cf_search_form_get_location(w->form);
    if (!*location || !cf_input_valid(location, CF_PATH_LIMIT)) {
        cf_criteria_free(criteria);
        status(w, "Choose a search folder (at most 32768 characters).", NULL);
        return;
    }
    g_autoptr(GFile) root = g_file_new_for_commandline_arg(location);
    g_autoptr(CfCriteria) snapshot = cf_criteria_copy(criteria);
    w->administrator_search = geteuid() == 0 ||
        (gtk_notebook_get_current_page(GTK_NOTEBOOK(w->tabs)) == CF_TAB_PLUS &&
         gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->admin_search)));
    w->search = w->administrator_search && geteuid() != 0 ?
        cf_search_start_admin(root, criteria, &error) : cf_search_start(root, criteria, &error);
    if (!w->search) {
        status(w, error->message, NULL);
        return;
    }
    cf_results_sync_set_search(GTK_TREE_VIEW(w->view), root, g_steal_pointer(&snapshot));
    g_set_object(&w->results_root, root);
    cf_settings_history_record(w->settings, input.name);
    cf_settings_flush(w->settings, NULL);
    w->has_search = TRUE;
    gtk_list_store_clear(w->store);
    gtk_widget_set_sensitive(w->tabs, FALSE);
    gtk_widget_set_sensitive(w->find, FALSE);
    gtk_widget_set_sensitive(w->stop, TRUE);
    status(w, w->administrator_search ? "Administrator search — authorizing / searching…" : "Searching…", NULL);
    w->timer = g_timeout_add(30, poll, w);
}

static void export_state(Window *w)
{
    if (!w->view) return;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(w->view));
    gtk_widget_set_sensitive(w->export, model && gtk_tree_model_iter_n_children(model, NULL) > 0 &&
        (!w->export_dialog || !cf_export_dialog_is_writing(w->export_dialog)));
}
static void export_removed(GtkTreeModel *model, GtkTreePath *path, gpointer data)
{
    (void)model; (void)path; export_state(data);
}
static void export_inserted(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    (void)iter; export_removed(model, path, data);
}
static void export_model_bound(GObject *view, GParamSpec *spec, gpointer data)
{
    (void)view; (void)spec;
    Window *w = data;
    if (w->export_model) {
        g_signal_handler_disconnect(w->export_model, w->export_removed_id);
        g_signal_handler_disconnect(w->export_model, w->export_inserted_id);
    }
    g_clear_object(&w->export_model);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(w->view));
    if (model) {
        w->export_model = g_object_ref(model);
        w->export_removed_id = g_signal_connect(model, "row-deleted", G_CALLBACK(export_removed), w);
        w->export_inserted_id = g_signal_connect(model, "row-inserted", G_CALLBACK(export_inserted), w);
    }
    export_state(w);
}

static void export_writing(GObject *button, GParamSpec *spec, GtkWidget *window)
{
    (void)button; (void)spec;
    Window *w = g_object_get_data(G_OBJECT(window), "cf-export-window");
    if (w) export_state(w);
}
static void export_closed(GtkWidget *dialog, GtkWidget *window)
{
    (void)dialog;
    Window *w = g_object_get_data(G_OBJECT(window), "cf-export-window");
    if (w) { w->export_dialog = NULL; export_state(w); }
}

static void export_clicked(GtkButton *button, gpointer data)
{
    (void)button;
    Window *w = data;
    GList *windows = gtk_window_list_toplevels();
    GtkWindow *existing = NULL;
    for (GList *p = windows; p; p = p->next)
        if (gtk_window_get_transient_for(p->data) == GTK_WINDOW(w->window) &&
            gtk_widget_get_visible(p->data) && !g_strcmp0(gtk_widget_get_name(p->data), "export-dialog")) existing = p->data;
    g_list_free(windows);
    if (existing) { gtk_window_present(existing); return; }
    export_state(w);
    if (!gtk_widget_get_sensitive(w->export)) return;
    w->export_dialog = cf_export_dialog_new(GTK_WINDOW(w->window), GTK_TREE_VIEW(w->view), w->results_root);
    if (!w->export_dialog) { export_state(w); return; }
    g_signal_connect_object(w->export_dialog, "destroy", G_CALLBACK(export_closed), w->window, 0);
    GtkWidget *save = gtk_dialog_get_widget_for_response(GTK_DIALOG(w->export_dialog), GTK_RESPONSE_ACCEPT);
    g_signal_connect_object(save, "notify::sensitive", G_CALLBACK(export_writing), w->window, 0);
}

static void destroyed(GtkWidget *widget, gpointer data)
{
    (void)widget;
    Window *w = data;
    g_object_set_data(G_OBJECT(w->window), "cf-export-window", NULL);
    GObject *controllers[] = { G_OBJECT(w->window), G_OBJECT(w->tabs), G_OBJECT(w->paned),
        G_OBJECT(w->more), G_OBJECT(w->form), G_OBJECT(w->view), G_OBJECT(w->all_after), G_OBJECT(w->all_before) };
    for (guint i = 0; i < G_N_ELEMENTS(controllers); i++) g_signal_handlers_disconnect_by_data(controllers[i], w);
    if (w->timer) g_source_remove(w->timer);
    if (w->status_idle) g_source_remove(w->status_idle);
    g_signal_handlers_disconnect_by_data(w->store, w);
    if (w->export_model) g_signal_handlers_disconnect_by_data(w->export_model, w);
    g_clear_object(&w->export_model);
    g_free(w->first_error);
    g_clear_object(&w->results_root);
    if (w->search) {
        cf_search_cancel(w->search);
        cf_search_unref(w->search);
    }
    g_object_unref(w->store);
    g_object_unref(w->form);
    cf_settings_free(w->settings);
    g_free(w);
}

GtkWidget *cf_window_new(GtkApplication *app, GFile *root)
{
    Window *w = g_new0(Window, 1);
    w->settings = cf_settings_new();
    w->results_root = g_object_ref(root);
    g_autofree char *path = g_file_get_parse_name(root);
    w->form = cf_search_form_new(path);
    cf_search_form_restore(w->form, w->settings);
    g_object_set(w->form, "location", path, NULL);
    cf_settings_set_string(w->settings, "Search", "location", path);
    cf_window_preferences_read(w->settings, &w->preferences);
    w->maximized = cf_settings_bool(w->settings, "Window", "Maximized", FALSE);
    w->window = gtk_application_window_new(app);
    g_object_set_data(G_OBJECT(w->window), "cf-export-window", w);
    gtk_window_set_title(GTK_WINDOW(w->window), geteuid() == 0 ?
        CF_APP_NAME " — Administrator Mode — Find: Files or Folders" : CF_APP_NAME " — Find: Files or Folders");
    gtk_window_set_default_size(GTK_WINDOW(w->window), w->preferences.width, w->preferences.height);
    gtk_window_set_icon_name(GTK_WINDOW(w->window), "classic-find-plus");
    g_autoptr(GdkPixbuf) icon = gdk_pixbuf_new_from_resource("/org/classicfind/plus/icon.png", NULL);
    if (icon) gtk_window_set_icon(GTK_WINDOW(w->window), icon);
    GdkGeometry geometry = { .min_width = 640, .min_height = 420 };
    gtk_window_set_geometry_hints(GTK_WINDOW(w->window), NULL, &geometry, GDK_HINT_MIN_SIZE);
    /* Wayland may ignore the legacy WM size hints; enforce the same native GTK minimum. */
    gtk_widget_set_size_request(w->window, geometry.min_width, geometry.min_height);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_container_add(GTK_CONTAINER(w->window), box);
    w->paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(box), w->paned, TRUE, TRUE, 0);
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_paned_pack1(GTK_PANED(w->paned), top, FALSE, FALSE);
    if (w->preferences.divider >= 0)
        gtk_paned_set_position(GTK_PANED(w->paned), w->preferences.divider);
    g_signal_connect(w->paned, "notify::position", G_CALLBACK(divider_changed), w);
    w->tabs = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(w->tabs), TRUE);
    gtk_box_pack_start(GTK_BOX(top), w->tabs, TRUE, TRUE, 0);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(top), actions, FALSE, FALSE, 0);
    w->find = gtk_button_new_with_mnemonic("_Find Now");
    w->stop = gtk_button_new_with_mnemonic("_Stop");
    gtk_button_set_image(GTK_BUTTON(w->find), gtk_image_new_from_icon_name("system-search", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_image(GTK_BUTTON(w->stop), gtk_image_new_from_icon_name("process-stop", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(w->find), TRUE);
    gtk_button_set_always_show_image(GTK_BUTTON(w->stop), TRUE);
    gtk_widget_set_name(w->find, "find-now");
    gtk_widget_set_name(w->stop, "stop-search");
    gtk_widget_set_sensitive(w->stop, FALSE);
    gtk_box_pack_start(GTK_BOX(actions), w->find, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(actions), w->stop, FALSE, FALSE, 0);
    GtkWidget *export = w->export = gtk_button_new_with_label("Export results");
    gtk_widget_set_sensitive(export, FALSE);
    gtk_widget_set_name(export, "export-results");
    gtk_box_pack_start(GTK_BOX(actions), export, FALSE, FALSE, 0);
    g_signal_connect(export, "clicked", G_CALLBACK(export_clicked), w);
    g_signal_connect(w->find, "clicked", G_CALLBACK(find), w);
    g_signal_connect(w->stop, "clicked", G_CALLBACK(stop), w);

    GtkWidget *location = grid_new();
    w->location = entry_new(path, "search-location");
    w->name = entry_new("", "search-name");
    cf_search_history_attach(GTK_ENTRY(w->name), w->form);
    field(location, "_Named:", w->name, 0);
    w->name_mode = combo(name_modes);
    gtk_grid_attach(GTK_GRID(location), w->name_mode, 2, 0, 1, 1);
    field(location, "_Look in:", w->location, 1);
    GtkWidget *button = gtk_button_new_with_mnemonic("_Browse…");
    g_signal_connect(button, "clicked", G_CALLBACK(browse), w);
    gtk_grid_attach(GTK_GRID(location), button, 2, 1, 1, 1);
    w->recursive = gtk_check_button_new_with_mnemonic("Include _subfolders");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->recursive), TRUE);
    w->case_sensitive = gtk_check_button_new_with_label("Case sensitive");
    w->stay = gtk_check_button_new_with_label("Stay on this filesystem");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->stay), TRUE);
    gtk_grid_attach(GTK_GRID(location), w->recursive, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(location), w->case_sensitive, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(location), w->stay, 1, 3, 2, 1);
    gtk_notebook_append_page(GTK_NOTEBOOK(w->tabs), location, tab_label("folder", "Name & Location"));

    GtkWidget *dates = grid_new();
    w->date_mode = date_mode_combo(FALSE);
    gtk_widget_set_name(w->date_mode, "date-mode");
    gtk_grid_attach(GTK_GRID(dates), date_field_combo(w, "date-field"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(dates), w->date_mode, 1, 0, 2, 1);
    GtkWidget *start_date = date_entry(&w->date_start, "", "date-start", FALSE);
    GtkWidget *end_date = date_entry(&w->date_end, "", "date-end", FALSE);
    field(dates, "_After / from:", start_date, 1);
    field(dates, "_Before / through:", end_date, 2);
    w->recent = entry_new("7", "recent-period");
    field(dates, "Previous _N:", w->recent, 3);
    gtk_widget_set_tooltip_text(w->date_mode, "Calendar dates use local midnight. Previous periods end at search start time.");
    gtk_notebook_append_page(GTK_NOTEBOOK(w->tabs), dates, tab_label("x-office-calendar", "Date Modified"));

    GtkWidget *advanced = grid_new();
    w->type = combo(types);
    field(advanced, "File _type:", w->type, 0);
    w->extension = entry_new("", "search-extension");
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->extension), "e.g. jpg or tar.gz");
    field(advanced, "_Extension:", w->extension, 1);
    w->extension_only = gtk_check_button_new_with_label("Match Named against extension only");
    gtk_grid_attach(GTK_GRID(advanced), w->extension_only, 1, 2, 2, 1);
    const char *sizes[] = { "Any size", "Greater than", "Less than", "Between (exclusive)", "Equal to", NULL };
    w->size_mode = combo(sizes);
    gtk_widget_set_name(w->size_mode, "size-mode");
    field(advanced, "Si_ze:", w->size_mode, 3);
    w->size_start = entry_new("0", "size-start");
    w->size_end = entry_new("1024", "size-end");
    field(advanced, "_Value / lower:", w->size_start, 4);
    field(advanced, "_Upper:", w->size_end, 5);
    w->unit = combo(units);
    gtk_grid_attach(GTK_GRID(advanced), w->unit, 2, 4, 1, 1);
    gtk_widget_set_tooltip_text(w->size_mode, "Size filters apply to regular files. Both between bounds are exclusive.");
    gtk_notebook_append_page(GTK_NOTEBOOK(w->tabs), advanced, tab_label("preferences-system", "Advanced"));
    struct { const char *property; GtkWidget *widget; const char *widget_property; } bindings[] = {
        { "location", w->location, "text" }, { "name", w->name, "text" },
        { "name-mode", w->name_mode, "active" }, { "case-sensitive", w->case_sensitive, "active" },
        { "recursive", w->recursive, "active" }, { "stay-on-filesystem", w->stay, "active" },
        { "date-start", w->date_start, "text" },
        { "date-end", w->date_end, "text" }, { "recent", w->recent, "text" },
        { "file-type", w->type, "active" }, { "extension", w->extension, "text" },
        { "extension-only", w->extension_only, "active" }, { "size-mode", w->size_mode, "active" },
        { "size-start", w->size_start, "text" }, { "size-end", w->size_end, "text" },
        { "size-unit", w->unit, "active" }
    };
    for (guint i = 0; i < G_N_ELEMENTS(bindings); i++) {
        if (!g_strcmp0(gtk_widget_get_name(bindings[i].widget), G_OBJECT_TYPE_NAME(bindings[i].widget)))
            gtk_widget_set_name(bindings[i].widget, bindings[i].property);
        bind_control(w, bindings[i].property, bindings[i].widget, bindings[i].widget_property);
    }
    gtk_notebook_append_page(GTK_NOTEBOOK(w->tabs), all_in_one(w), tab_label("view-grid-symbolic", "Find Plus"));
    gtk_notebook_append_page(GTK_NOTEBOOK(w->tabs), cf_help_about_new(), tab_label("help-about-symbolic", "Help/About"));

    bind_date_mode(w, "date-mode", w->date_mode);
    g_signal_connect(w->form, "notify::plus-date-mode", G_CALLBACK(filter_changed), w);
    g_signal_connect(w->form, "notify::plus-date-enabled", G_CALLBACK(filter_changed), w);
    g_signal_connect(w->form, "notify::date-mode", G_CALLBACK(filter_changed), w);
    g_signal_connect(w->form, "notify::size-mode", G_CALLBACK(filter_changed), w);
    filter_changed(NULL, NULL, w);

    w->store = cf_results_new();
    GtkWidget *view = w->view = cf_results_view_new(w->store);
    g_signal_connect(w->store, "row-deleted", G_CALLBACK(row_removed), w);
    g_signal_connect(view, "notify::model", G_CALLBACK(export_model_bound), w);
    export_model_bound(NULL, NULL, w);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_widget_set_size_request(scroll, -1, 100);
    gtk_paned_pack2(GTK_PANED(w->paned), scroll, TRUE, FALSE);
    w->status = gtk_label_new("Ready — 0 results");
    gtk_widget_set_name(w->status, "search-status");
    gtk_label_set_xalign(GTK_LABEL(w->status), 0);
    gtk_label_set_ellipsize(GTK_LABEL(w->status), PANGO_ELLIPSIZE_END);
    gtk_widget_set_tooltip_text(w->status, "Ctrl+F: name · Ctrl+L: folder · Ctrl+1–5: tabs · Ctrl+Return or F5: find · Esc: stop");
    gtk_box_pack_end(GTK_BOX(box), w->status, FALSE, FALSE, 0);
    g_signal_connect(w->window, "key-press-event", G_CALLBACK(key_press), w);
    g_signal_connect(w->window, "configure-event", G_CALLBACK(configured), w);
    g_signal_connect(w->window, "window-state-event", G_CALLBACK(window_state), w);
    g_signal_connect(w->window, "destroy", G_CALLBACK(destroyed), w);
    int tab = cf_settings_int(w->settings, "Window", "Tab", CF_TAB_NAME, CF_TAB_NAME, CF_TAB_HELP);
    gtk_widget_show_all(w->tabs);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(w->tabs), tab);
    gtk_widget_set_sensitive(w->find, tab != CF_TAB_HELP);
    g_signal_connect(w->tabs, "switch-page", G_CALLBACK(tab_selected), w);
    cf_search_form_track(w->form, w->settings);
    cf_window_position_attach(GTK_WINDOW(w->window));
    if (w->maximized) gtk_window_maximize(GTK_WINDOW(w->window));
    return w->window;
}
