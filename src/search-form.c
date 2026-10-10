#include "input-limits.h"
#include "search-form.h"
#include <unistd.h>

enum {
    PROP_0, CONTAINING, EXCLUDE, LOCATION, NAME, EXTENSION, DATE_START, DATE_END, RECENT, SIZE_START, SIZE_END,
    NAME_MODE, DATE_MODE, DATE_FIELD, SIZE_MODE, FILE_TYPE, SIZE_UNIT,
    CASE_SENSITIVE, EXTENSION_ONLY, RECURSIVE, STAY_ON_FILESYSTEM, FOLLOW_SYMLINKS,
    PLUS_TYPE, PLUS_EXTENSION, PLUS_DATE_MODE, PLUS_RECENT, PLUS_GROUP, PLUS_USER, PLUS_PERMISSIONS,
    PLUS_TYPE_ENABLED, PLUS_EXTENSION_ENABLED, PLUS_DATE_ENABLED,
    PLUS_GROUP_ENABLED, PLUS_USER_ENABLED, PLUS_PERMISSIONS_ENABLED, PLUS_NO_HIDDEN, N_PROPERTIES
};

struct _CfSearchForm {
    GObject parent_instance;
    GValue values[N_PROPERTIES];
};

G_DEFINE_TYPE(CfSearchForm, cf_search_form, G_TYPE_OBJECT)
static GParamSpec *properties[N_PROPERTIES];
#define PROPERTY_FLAGS (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS)

static void get_property(GObject *object, guint id, GValue *value, GParamSpec *spec)
{
    CfSearchForm *form = CF_SEARCH_FORM(object);
    if (!id || id >= N_PROPERTIES) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
        return;
    }
    g_value_copy(&form->values[id], value);
}

static void set_property(GObject *object, guint id, const GValue *value, GParamSpec *spec)
{
    CfSearchForm *form = CF_SEARCH_FORM(object);
    if (!id || id >= N_PROPERTIES) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
        return;
    }
    if (!g_param_values_cmp(properties[id], &form->values[id], value)) return;
    g_value_reset(&form->values[id]);
    g_value_copy(value, &form->values[id]);
    if (G_VALUE_HOLDS_STRING(value) && !g_value_get_string(value))
        g_value_set_string(&form->values[id], "");
    g_object_notify_by_pspec(object, properties[id]);
}

static void finalize(GObject *object)
{
    CfSearchForm *form = CF_SEARCH_FORM(object);
    for (guint id = 1; id < N_PROPERTIES; id++) g_value_unset(&form->values[id]);
    G_OBJECT_CLASS(cf_search_form_parent_class)->finalize(object);
}

static void string_property(guint id, const char *name, const char *initial)
{
    properties[id] = g_param_spec_string(name, name, NULL, initial, PROPERTY_FLAGS);
}

static void mode_property(guint id, const char *name, int maximum)
{
    properties[id] = g_param_spec_int(name, name, NULL, 0, maximum, 0, PROPERTY_FLAGS);
}

static void flag_property(guint id, const char *name, gboolean initial)
{
    properties[id] = g_param_spec_boolean(name, name, NULL, initial, PROPERTY_FLAGS);
}

static void cf_search_form_class_init(CfSearchFormClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->finalize = finalize;
    string_property(CONTAINING, "containing-text", "");
    string_property(EXCLUDE, "exclude", "");
    string_property(LOCATION, "location", "");
    string_property(NAME, "name", "");
    string_property(EXTENSION, "extension", "");
    string_property(DATE_START, "date-start", "");
    string_property(DATE_END, "date-end", "");
    string_property(RECENT, "recent", "7");
    string_property(SIZE_START, "size-start", "0");
    string_property(SIZE_END, "size-end", "1024");
    mode_property(NAME_MODE, "name-mode", CF_NAME_REGEX);
    mode_property(DATE_MODE, "date-mode", CF_DATE_HOURS);
    mode_property(DATE_FIELD, "date-field", CF_DATE_ACCESSED);
    mode_property(SIZE_MODE, "size-mode", CF_SIZE_EQUAL);
    mode_property(FILE_TYPE, "file-type", CF_TYPE_TEXT);
    mode_property(SIZE_UNIT, "size-unit", 3);
    flag_property(CASE_SENSITIVE, "case-sensitive", FALSE);
    flag_property(EXTENSION_ONLY, "extension-only", FALSE);
    flag_property(RECURSIVE, "recursive", TRUE);
    flag_property(STAY_ON_FILESYSTEM, "stay-on-filesystem", TRUE);
    flag_property(FOLLOW_SYMLINKS, "follow-symlinks", FALSE);
    mode_property(PLUS_TYPE, "plus-file-type", CF_TYPE_TEXT);
    string_property(PLUS_EXTENSION, "plus-extension", "");
    mode_property(PLUS_DATE_MODE, "plus-date-mode", CF_DATE_HOURS);
    string_property(PLUS_RECENT, "plus-recent", "7");
    g_autofree char *gid = g_strdup_printf("%u", (guint)getgid());
    g_autofree char *uid = g_strdup_printf("%u", (guint)getuid());
    string_property(PLUS_GROUP, "plus-group", gid);
    string_property(PLUS_USER, "plus-user", uid);
    string_property(PLUS_PERMISSIONS, "plus-permissions", "");
    flag_property(PLUS_TYPE_ENABLED, "plus-type-enabled", FALSE);
    flag_property(PLUS_EXTENSION_ENABLED, "plus-extension-enabled", FALSE);
    flag_property(PLUS_DATE_ENABLED, "plus-date-enabled", FALSE);
    flag_property(PLUS_GROUP_ENABLED, "plus-group-enabled", FALSE);
    flag_property(PLUS_USER_ENABLED, "plus-user-enabled", FALSE);
    flag_property(PLUS_PERMISSIONS_ENABLED, "plus-permissions-enabled", FALSE);
    flag_property(PLUS_NO_HIDDEN, "plus-no-hidden", FALSE);
    g_object_class_install_properties(object_class, N_PROPERTIES, properties);
}

static void cf_search_form_init(CfSearchForm *form)
{
    for (guint id = 1; id < N_PROPERTIES; id++) {
        g_value_init(&form->values[id], G_PARAM_SPEC_VALUE_TYPE(properties[id]));
        g_param_value_set_default(properties[id], &form->values[id]);
    }
}

CfSearchForm *cf_search_form_new(const char *location)
{
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *today = g_date_time_format(now, "%Y/%m/%d");
    return g_object_new(CF_TYPE_SEARCH_FORM, "location", location ? location : "",
                        "date-start", today, "date-end", today, NULL);
}

const char *cf_search_form_get_location(CfSearchForm *form)
{
    return g_value_get_string(&form->values[LOCATION]);
}

CfCriteriaInput cf_search_form_get_input(CfSearchForm *form)
{
    return (CfCriteriaInput) {
        .name = g_value_get_string(&form->values[NAME]),
        .exclude = g_value_get_string(&form->values[EXCLUDE]),
        .extension = g_value_get_string(&form->values[EXTENSION]),
        .date_start = g_value_get_string(&form->values[DATE_START]),
        .date_end = g_value_get_string(&form->values[DATE_END]),
        .recent = g_value_get_string(&form->values[RECENT]),
        .size_start = g_value_get_string(&form->values[SIZE_START]),
        .size_end = g_value_get_string(&form->values[SIZE_END]),
        .name_mode = g_value_get_int(&form->values[NAME_MODE]),
        .date_mode = g_value_get_int(&form->values[DATE_MODE]),
        .date_field = g_value_get_int(&form->values[DATE_FIELD]),
        .size_mode = g_value_get_int(&form->values[SIZE_MODE]),
        .type = g_value_get_int(&form->values[FILE_TYPE]),
        .size_unit = g_value_get_int(&form->values[SIZE_UNIT]),
        .case_sensitive = g_value_get_boolean(&form->values[CASE_SENSITIVE]),
        .extension_only = g_value_get_boolean(&form->values[EXTENSION_ONLY]),
        .recursive = g_value_get_boolean(&form->values[RECURSIVE]),
        .stay_on_filesystem = g_value_get_boolean(&form->values[STAY_ON_FILESYSTEM]),
        .follow_symlinks = g_value_get_boolean(&form->values[FOLLOW_SYMLINKS])
    };
}

gboolean cf_search_form_effective(CfSearchForm *form, int tab, gboolean more_expanded, CfCriteriaInput *input)
{
    if (tab < CF_TAB_NAME || tab >= CF_TAB_HELP) return FALSE;
    *input = cf_search_form_get_input(form);
    if (tab != CF_TAB_PLUS) input->exclude = "";
    if (tab != CF_TAB_NAME && tab != CF_TAB_PLUS) {
        input->name = ""; input->name_mode = CF_NAME_AUTO;
    }
    if (tab != CF_TAB_DATE && tab != CF_TAB_PLUS) input->date_mode = CF_DATE_ALL;
    if (tab != CF_TAB_ADVANCED && tab != CF_TAB_PLUS) input->size_mode = CF_SIZE_ALL;
    if (tab != CF_TAB_ADVANCED && (tab != CF_TAB_PLUS || !more_expanded)) {
        input->type = CF_TYPE_ANY; input->extension = ""; input->extension_only = FALSE;
    }
    if (tab == CF_TAB_PLUS) {
        /* Main date bounds keep their own draft; optional mode/period are transient. */
        if (cf_date_mode_relative(input->date_mode))
            input->date_mode = CF_DATE_ALL;
        else if (input->date_mode == CF_DATE_BETWEEN) input->date_mode = CF_DATE_BOUNDS;
        input->no_hidden = more_expanded && g_value_get_boolean(&form->values[PLUS_NO_HIDDEN]);
        input->user_enabled = more_expanded && g_value_get_boolean(&form->values[PLUS_USER_ENABLED]);
        input->group_enabled = more_expanded && g_value_get_boolean(&form->values[PLUS_GROUP_ENABLED]);
        input->permissions_enabled = more_expanded && g_value_get_boolean(&form->values[PLUS_PERMISSIONS_ENABLED]);
        input->user = g_value_get_string(&form->values[PLUS_USER]);
        input->group = g_value_get_string(&form->values[PLUS_GROUP]);
        input->permissions = g_value_get_string(&form->values[PLUS_PERMISSIONS]);
        input->type = more_expanded && g_value_get_boolean(&form->values[PLUS_TYPE_ENABLED]) ?
                      g_value_get_int(&form->values[PLUS_TYPE]) : CF_TYPE_ANY;
        input->extension = more_expanded && g_value_get_boolean(&form->values[PLUS_EXTENSION_ENABLED]) ?
                           g_value_get_string(&form->values[PLUS_EXTENSION]) : "";
        input->extension_only = FALSE; /* Top filename always remains an independent matcher. */
        int mode = g_value_get_int(&form->values[PLUS_DATE_MODE]);
        if (more_expanded && g_value_get_boolean(&form->values[PLUS_DATE_ENABLED]) && cf_date_mode_relative(mode)) {
            input->relative_mode = mode;
            input->relative_recent = g_value_get_string(&form->values[PLUS_RECENT]);
        }
    }
    return TRUE;
}

void cf_search_form_reset_optional(CfSearchForm *form)
{
    g_object_freeze_notify(G_OBJECT(form));
    for (guint id = PLUS_TYPE; id < N_PROPERTIES; id++) {
        GValue value = G_VALUE_INIT;
        g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(properties[id]));
        g_param_value_set_default(properties[id], &value);
        g_object_set_property(G_OBJECT(form), properties[id]->name, &value);
        g_value_unset(&value);
    }
    g_object_thaw_notify(G_OBJECT(form));
}

static const char *section(guint id)
{
    if (id == CONTAINING || id == EXCLUDE) return "FindPlus";
    if (id == DATE_START || id == DATE_END || id == RECENT || id == DATE_MODE || id == DATE_FIELD) return "Date";
    if (id == EXTENSION || id == SIZE_START || id == SIZE_END || id == SIZE_MODE ||
        id == FILE_TYPE || id == SIZE_UNIT || id == EXTENSION_ONLY) return "Advanced";
    return "Search";
}
void cf_search_form_restore(CfSearchForm *form, CfSettings *settings)
{
    GKeyFile *key = cf_settings_data(settings);
    for (guint id = 1; id < PLUS_TYPE; id++) {
        const char *name = properties[id]->name, *group = section(id);
        if (!g_key_file_has_key(key, group, name, NULL)) continue;
        if (G_VALUE_HOLDS_STRING(&form->values[id])) {
            g_autofree char *text = g_key_file_get_string(key, group, name, NULL);
            if (cf_input_valid(text, cf_input_limit(name))) g_object_set(form, name, text, NULL);
        } else if (G_VALUE_HOLDS_BOOLEAN(&form->values[id])) {
            gboolean value = cf_settings_bool(settings, group, name, g_value_get_boolean(&form->values[id]));
            g_object_set(form, name, value, NULL);
        } else {
            GParamSpecInt *spec = G_PARAM_SPEC_INT(properties[id]);
            int value = cf_settings_int(settings, group, name, spec->default_value, spec->minimum, spec->maximum);
            g_object_set(form, name, value, NULL);
        }
    }
}
static void persist_property(GObject *object, GParamSpec *spec, gpointer data)
{
    CfSearchForm *form = CF_SEARCH_FORM(object);
    guint id = spec->param_id;
    if (id >= PLUS_TYPE) return; /* Optional values never survive a session. */
    const char *group = section(id);
    if (G_VALUE_HOLDS_STRING(&form->values[id]))
        cf_settings_set_string(data, group, spec->name, g_value_get_string(&form->values[id]));
    else if (G_VALUE_HOLDS_BOOLEAN(&form->values[id]))
        cf_settings_set_bool(data, group, spec->name, g_value_get_boolean(&form->values[id]));
    else cf_settings_set_int(data, group, spec->name, g_value_get_int(&form->values[id]));
}
void cf_search_form_track(CfSearchForm *form, CfSettings *settings)
{
    /* Capture untouched fixed-date defaults, but never overwrite another window's saved keys. */
    g_autoptr(GKeyFile) defaults = g_key_file_new();
    for (guint id = 1; id < PLUS_TYPE; id++) {
        const char *group = section(id), *name = properties[id]->name;
        if (G_VALUE_HOLDS_STRING(&form->values[id]))
            g_key_file_set_string(defaults, group, name, g_value_get_string(&form->values[id]));
        else if (G_VALUE_HOLDS_BOOLEAN(&form->values[id]))
            g_key_file_set_boolean(defaults, group, name, g_value_get_boolean(&form->values[id]));
        else g_key_file_set_integer(defaults, group, name, g_value_get_int(&form->values[id]));
    }
    cf_settings_defaults(settings, defaults);
    /* Start only after restoration and widget binding. */
    g_signal_connect(form, "notify", G_CALLBACK(persist_property), settings);
}
