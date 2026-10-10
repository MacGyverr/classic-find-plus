#include "input-limits.h"
#include "criteria-private.h"
#include "date-expression.h"
#include "file-time.h"
#include <string.h>

static gboolean invalid(GError **error, const char *message)
{
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, message);
    return FALSE;
}

static gboolean number(const char *text, guint64 *value, GError **error)
{
    if (!cf_input_valid(text, CF_NUMBER_LIMIT) || !*text)
        return invalid(error, "Enter a whole, nonnegative number (digits only).");
    guint64 result = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9')
            return invalid(error, "Numbers must contain digits only; no signs, spaces or decimals.");
        guint digit = *p - '0';
        if (result > (G_MAXUINT64 - digit) / 10)
            return invalid(error, "Number exceeds the 64-bit range.");
        result = result * 10 + digit;
    }
    *value = result;
    return TRUE;
}

gboolean cf_date_mode_relative(CfDateMode mode)
{
    return mode == CF_DATE_MINUTES || mode == CF_DATE_HOURS || mode == CF_DATE_DAYS || mode == CF_DATE_MONTHS;
}

/* GLib supports years 1..9999. Multiplication is checked before calendar calls;
 * the actual maximum N also depends on the captured instant and its time zone. */
static gboolean relative_interval(CfDateMode mode, const char *text, GDateTime *clock,
                                  gint64 *lower, gint64 *upper, GError **error)
{
    guint64 count;
    if (!number(text, &count, error)) {
        if (error && *error) g_prefix_error(error, "Previous N must be a positive whole number: ");
        return FALSE;
    }
    if (!count) return invalid(error, "Previous N must be a positive whole number (at least 1).");
    g_autoptr(GDateTime) start = NULL;
    if (mode == CF_DATE_MONTHS) {
        guint64 available = (guint64)(g_date_time_get_year(clock) - 1) * 12 + g_date_time_get_month(clock) - 1;
        if (count > available) return invalid(error, "Previous N exceeds the available calendar months (year 1 minimum).");
        start = g_date_time_add_months(clock, -(gint)count);
    } else {
        guint64 unit = mode == CF_DATE_MINUTES ? 60 : mode == CF_DATE_HOURS ? 3600 : 86400;
        guint64 scale = unit * G_USEC_PER_SEC;
        g_autoptr(GDateTime) earliest = g_date_time_new(g_date_time_get_timezone(clock), 1, 1, 1, 0, 0, 0);
        if (!earliest || count > (guint64)(g_date_time_difference(clock, earliest)) / scale ||
            count > G_MAXINT64 / scale)
            return invalid(error, "Previous N exceeds the supported elapsed-time range.");
        start = g_date_time_add(clock, -(gint64)(count * scale));
    }
    if (!start) return invalid(error, "Previous N extends before the supported calendar (year 1).");
    *lower = g_date_time_to_unix(start) * G_USEC_PER_SEC + g_date_time_get_microsecond(start);
    /* Exclusive internal upper boundary represents inclusive reference at us precision. */
    *upper = g_date_time_to_unix(clock) * G_USEC_PER_SEC + g_date_time_get_microsecond(clock) + 1;
    return TRUE;
}

static gboolean account_id(const char *text, guint32 *id, GError **error)
{
    guint64 value;
    if (!number(text, &value, error)) return FALSE;
    if (value >= G_MAXUINT32) return invalid(error, "Select an available account (valid numeric UID/GID required).");
    *id = value;
    return TRUE;
}
static gboolean permissions(const char *text, guint32 *mode, GError **error)
{
    if (!text || (strlen(text) != 3 && strlen(text) != 4))
        return invalid(error, "Permissions must be three or four octal digits, e.g. 644 or 4755.");
    guint32 value = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '7') return invalid(error, "Permissions accepts only octal digits 0 through 7.");
        value = value * 8 + *p - '0';
    }
    *mode = value;
    return TRUE;
}

gboolean cf_parse_size(const char *text, guint unit, guint64 *value, GError **error)
{
    guint64 raw;
    if (unit > 3)
        return invalid(error, "Invalid size unit.");
    if (!number(text, &raw, error))
        return FALSE;
    guint64 multiplier = G_GUINT64_CONSTANT(1) << (10 * unit);
    if (raw > G_MAXUINT64 / multiplier)
        return invalid(error, "Size in the selected unit exceeds the 64-bit byte range.");
    *value = raw * multiplier;
    return TRUE;
}

/* Inclusion and exclusion compile/match through the same filename rules. */
static gboolean compile_name(const char *raw, CfNameMode mode, gboolean sensitive,
    char **text, GPatternSpec **pattern, GRegex **regex, GError **error)
{
    *text = sensitive ? g_strdup(raw) : g_utf8_casefold(raw, -1);
    if (mode == CF_NAME_REGEX) {
        g_autoptr(GError) regex_error = NULL;
        g_autofree char *bounded = g_strconcat("(*LIMIT_MATCH=100000)(*LIMIT_RECURSION=1000)", raw, NULL);
        *regex = g_regex_new(bounded, sensitive ? 0 : G_REGEX_CASELESS, 0, &regex_error);
        if (!*regex) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Invalid regular expression: %s", regex_error->message);
            return FALSE;
        }
    } else if (mode == CF_NAME_AUTO && strpbrk(raw, "*?")) *pattern = g_pattern_spec_new(*text);
    return TRUE;
}
static gboolean match_name(const char *text, GPatternSpec *pattern, GRegex *regex,
    CfNameMode mode, const char *target, gboolean prefix)
{
    if (regex) return g_regex_match(regex, target, 0, NULL);
    if (pattern) return g_pattern_spec_match_string(pattern, target);
    if (prefix) return g_str_has_prefix(target, text);
    if (mode == CF_NAME_EXACT) return !strcmp(text, target);
    return strstr(target, text) != NULL;
}
static void exclusion_free(gpointer data)
{
    CfExcludeRule *rule = data;
    g_free(rule->text);
    if (rule->pattern) g_pattern_spec_free(rule->pattern);
    if (rule->regex) g_regex_unref(rule->regex);
    g_free(rule);
}
static gboolean exclusion_add(CfCriteria *c, const char *text, GError **error)
{
    if (!*text) return TRUE; /* Spaces are significant; do not strip. */
    /* Derived from input character bound: each nonempty rule needs one character
     * and each separator another. Regex always uses one rule. */
    if (c->exclusions->len >= (CF_TEXT_LIMIT + 1) / 2)
        return invalid(error, "Too many exclusions for the supported input length.");
    CfExcludeRule *rule = g_new0(CfExcludeRule, 1);
    rule->directories = *text == '/';
    if (rule->directories && !*++text) {
        exclusion_free(rule);
        return invalid(error, "Exclude needs a directory-component pattern after /.");
    }
    if (!compile_name(text, c->name_mode, c->case_sensitive,
                      &rule->text, &rule->pattern, &rule->regex, error)) {
        exclusion_free(rule);
        g_prefix_error(error, "Exclude: ");
        return FALSE;
    }
    g_ptr_array_add(c->exclusions, rule);
    return TRUE;
}
static gboolean compile_exclusions(CfCriteria *c, const char *raw, GError **error)
{
    c->exclusions = g_ptr_array_new_with_free_func(exclusion_free);
    if (c->name_mode == CF_NAME_REGEX) return exclusion_add(c, raw, error);
    g_autoptr(GString) rule = g_string_sized_new(strlen(raw));
    for (const char *p = raw; ; p++) {
        if (*p == '\\' && (p[1] == '|' || p[1] == '\\')) {
            g_string_append_c(rule, *++p);
        } else if (*p == '|' || !*p) {
            if (!exclusion_add(c, rule->str, error)) return FALSE;
            if (!*p) break;
            g_string_truncate(rule, 0);
        } else g_string_append_c(rule, *p);
    }
    return TRUE;
}

static gboolean excludes_name(const CfCriteria *c, const char *display, const char *folded, gboolean directory)
{
    for (guint i = 0; i < c->exclusions->len; i++) {
        const CfExcludeRule *rule = g_ptr_array_index(c->exclusions, i);
        if (rule->directories && !directory) continue;
        if (match_name(rule->text, rule->pattern, rule->regex, c->name_mode,
                       rule->regex ? display : folded, rule->directories)) return TRUE;
    }
    return FALSE;
}
gboolean cf_criteria_excludes(const CfCriteria *c, GFileInfo *info)
{
    const char *raw = g_file_info_get_name(info);
    if (!raw) return FALSE;
    if (c->no_hidden && *raw == '.') return TRUE;
    if (!c->exclusions->len) return FALSE;
    g_autofree char *display = g_filename_display_name(raw);
    g_autofree char *folded = c->case_sensitive ? NULL : g_utf8_casefold(display, -1);
    return excludes_name(c, display, folded ? folded : display,
                         g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);
}

void cf_criteria_free(CfCriteria *c)
{
    if (!c) return;
    g_free(c->input_name); g_free(c->input_exclude); g_free(c->input_extension);
    g_free(c->name);
    g_free(c->extension);
    if (c->exclusions) g_ptr_array_unref(c->exclusions);
    if (c->pattern) g_pattern_spec_free(c->pattern);
    if (c->regex) g_regex_unref(c->regex);
    g_free(c);
}

CfCriteria *cf_criteria_copy(const CfCriteria *source)
{
    if (!source) return NULL;
    CfCriteria *copy = g_memdup2(source, sizeof *source);
    copy->input_name = g_strdup(source->input_name);
    copy->input_exclude = g_strdup(source->input_exclude);
    copy->input_extension = g_strdup(source->input_extension);
    copy->name = g_strdup(source->name);
    copy->extension = g_strdup(source->extension);
    copy->exclusions = g_ptr_array_ref(source->exclusions);
    copy->pattern = source->pattern ? g_pattern_spec_copy(source->pattern) : NULL;
    copy->regex = source->regex ? g_regex_ref(source->regex) : NULL;
    return copy;
}

CfCriteria *cf_criteria_new(const CfCriteriaInput *in, GDateTime *now, GError **error)
{
    g_autoptr(CfCriteria) c = g_new0(CfCriteria, 1);
    if (in->name_mode > CF_NAME_REGEX || (guint)in->date_mode > CF_DATE_HOURS ||
        (in->relative_mode != CF_DATE_ALL && !cf_date_mode_relative(in->relative_mode)) || in->date_field > CF_DATE_ACCESSED ||
        in->size_mode > CF_SIZE_EQUAL || in->type > CF_TYPE_TEXT) {
        invalid(error, "Invalid filter mode.");
        return NULL;
    }
    const char *name = in->name ? in->name : "";
    const char *extension = in->extension ? in->extension : "";
    const char *exclude = in->exclude ? in->exclude : "";
    if (!cf_input_valid(name, CF_TEXT_LIMIT) || !cf_input_valid(extension, CF_TEXT_LIMIT) || !cf_input_valid(exclude, CF_TEXT_LIMIT)) {
        invalid(error, "Search text must be valid UTF-8 and at most 8192 characters.");
        return NULL;
    }
    if (g_str_has_prefix(extension, "*.")) extension += 2;
    else if (*extension == '.') extension++;
    /* Keep original validated text for IPC. Unicode casefolding can expand
     * character counts, and regex escapes must retain their original case. */
    c->input_name = g_strdup(name); c->input_exclude = g_strdup(exclude);
    c->input_extension = g_strdup(in->extension ? in->extension : "");
    if (strpbrk(extension, "*?/")) {
        invalid(error, "Enter one extension, such as jpg or tar.gz; wildcards belong in Named.");
        return NULL;
    }
    c->case_sensitive = in->case_sensitive;
    c->extension = in->case_sensitive ? g_strdup(extension) : g_utf8_casefold(extension, -1);
    c->name_mode = in->name_mode;
    if (!compile_name(name, c->name_mode, c->case_sensitive, &c->name, &c->pattern, &c->regex, error)) return NULL;
    if (!compile_exclusions(c, exclude, error)) return NULL;
    c->has_uid = in->user_enabled; c->has_gid = in->group_enabled;
    c->has_permissions = in->permissions_enabled;
    if (c->has_uid && !account_id(in->user, &c->uid, error)) return NULL;
    if (c->has_gid && !account_id(in->group, &c->gid, error)) return NULL;
    if (c->has_permissions && !permissions(in->permissions, &c->permissions, error)) return NULL;
    c->no_hidden = in->no_hidden; c->follow_symlinks = in->follow_symlinks;
    c->type = in->type;
    c->date_field = in->date_field;
    c->extension_only = in->extension_only;
    c->recursive = in->recursive;
    c->stay_on_filesystem = in->stay_on_filesystem;
    c->size_mode = in->size_mode;
    if (in->size_mode != CF_SIZE_ALL) {
        if (!cf_parse_size(in->size_start, in->size_unit, &c->size_start, error)) return NULL;
        if (in->size_mode == CF_SIZE_BETWEEN) {
            if (!cf_parse_size(in->size_end, in->size_unit, &c->size_end, error)) return NULL;
            if (c->size_start >= c->size_end) {
                invalid(error, "The upper size must be greater than the lower size.");
                return NULL;
            }
        }
    }
    /* Capture once, before either bounds expression or optional interval is resolved. */
    g_autoptr(GDateTime) clock = now ? g_date_time_ref(now) : g_date_time_new_now_local();
    if (in->date_mode != CF_DATE_ALL) {
        g_autoptr(GDateTime) start = NULL;
        g_autoptr(GDateTime) end = NULL;
        if (cf_date_mode_relative(in->date_mode)) {
            if (!relative_interval(in->date_mode, in->recent, clock, &c->date_start, &c->date_end, error)) return NULL;
            c->has_start = c->has_end = TRUE;
        } else {
            const char *first_text = in->date_mode == CF_DATE_BEFORE && in->date_end && *in->date_end ?
                                     in->date_end : in->date_start;
            gboolean first_time = FALSE;
            g_autoptr(GDateTime) first = cf_date_expression_resolve(first_text, clock, &first_time, error);
            if (!first) return NULL;
            if (in->date_mode == CF_DATE_BEFORE) {
                end = g_date_time_ref(first);
                c->has_end = TRUE;
            } else {
                start = in->date_mode == CF_DATE_AFTER || in->date_mode == CF_DATE_BOUNDS ?
                        (first_time ? g_date_time_add(first, 1) : g_date_time_add_days(first, 1)) : g_date_time_ref(first);
                c->has_start = TRUE;
                if (!start) {
                    invalid(error, "The date is beyond the supported calendar range.");
                    return NULL;
                }
                if (in->date_mode == CF_DATE_BETWEEN || in->date_mode == CF_DATE_BOUNDS) {
                    gboolean last_time = FALSE;
                    g_autoptr(GDateTime) last = cf_date_expression_resolve(in->date_end, clock, &last_time, error);
                    if (!last) return NULL;
                    if (g_date_time_compare(first, last) > 0) {
                        invalid(error, "End date must be on or after start date.");
                        return NULL;
                    }
                    end = in->date_mode == CF_DATE_BOUNDS ? g_date_time_ref(last) : (last_time ? g_date_time_add(last, 1) : g_date_time_add_days(last, 1));
                    if (!end) {
                        invalid(error, "End date is beyond the supported calendar range.");
                        return NULL;
                    }
                    c->has_end = TRUE;
                    if (in->date_mode == CF_DATE_BOUNDS && g_date_time_compare(start, end) >= 0) {
                        invalid(error, "After/Before bounds must leave a nonempty interval.");
                        return NULL;
                    }
                }
            }
            if (end) c->date_end = g_date_time_to_unix(end) * G_USEC_PER_SEC + g_date_time_get_microsecond(end);
        }
        if (start) c->date_start = g_date_time_to_unix(start) * G_USEC_PER_SEC + g_date_time_get_microsecond(start);
    }
    if (in->relative_mode != CF_DATE_ALL) {
        gint64 lower, upper;
        if (!relative_interval(in->relative_mode, in->relative_recent, clock, &lower, &upper, error)) return NULL;
        c->date_start = c->has_start ? MAX(c->date_start, lower) : lower;
        c->date_end = c->has_end ? MIN(c->date_end, upper) : upper;
        c->has_start = c->has_end = TRUE;
        if (c->date_start >= c->date_end) {
            invalid(error, "Main date bounds and Last N interval do not overlap.");
            return NULL;
        }
    }
    return g_steal_pointer(&c);
}

gboolean cf_criteria_matches(const CfCriteria *c, GFileInfo *info)
{
    GFileType type = g_file_info_get_file_type(info);
    const char *raw = g_file_info_get_name(info);
    if (!raw) return FALSE;
    if (c->no_hidden && *raw == '.') return FALSE;
    if (c->has_uid && (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_UID) ||
        g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_UID) != c->uid)) return FALSE;
    if (c->has_gid && (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_GID) ||
        g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_GID) != c->gid)) return FALSE;
    if (c->has_permissions && (!g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_MODE) ||
        (g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE) & 07777) != c->permissions)) return FALSE;
    g_autofree char *display = g_filename_display_name(raw);
    g_autofree char *folded = c->case_sensitive ? g_strdup(display) : g_utf8_casefold(display, -1);
    if (excludes_name(c, display, folded, type == G_FILE_TYPE_DIRECTORY)) return FALSE;
    const char *target = c->regex ? display : folded;
    if (c->extension_only) {
        const char *dot = strrchr(target, '.');
        target = dot && dot != target ? dot + 1 : "";
    }
    if (*c->name && !match_name(c->name, c->pattern, c->regex, c->name_mode, target, FALSE)) return FALSE;
    if (*c->extension) {
        gsize len = strlen(folded), extlen = strlen(c->extension);
        if (type == G_FILE_TYPE_DIRECTORY || len <= extlen + 1 ||
            folded[len - extlen - 1] != '.' || !g_str_has_suffix(folded, c->extension)) return FALSE;
    }
    if (c->type == CF_TYPE_FILES && type != G_FILE_TYPE_REGULAR) return FALSE;
    if (c->type == CF_TYPE_FOLDERS && type != G_FILE_TYPE_DIRECTORY) return FALSE;
    if (c->type >= CF_TYPE_IMAGE) {
        const char *content = g_file_info_get_content_type(info);
        g_autofree char *mime = content ? g_content_type_get_mime_type(content) : NULL;
        const char *prefixes[] = { "image/", "audio/", "video/", "text/" };
        if (!mime || !g_str_has_prefix(mime, prefixes[c->type - CF_TYPE_IMAGE])) return FALSE;
    }
    if (c->size_mode != CF_SIZE_ALL) {
        if (type != G_FILE_TYPE_REGULAR || !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) return FALSE;
        goffset signed_size = g_file_info_get_size(info);
        if (signed_size < 0) return FALSE;
        guint64 size = (guint64)signed_size;
        if (c->size_mode == CF_SIZE_EQUAL && size != c->size_start) return FALSE;
        if ((c->size_mode == CF_SIZE_GREATER || c->size_mode == CF_SIZE_BETWEEN) && size <= c->size_start) return FALSE;
        if (c->size_mode == CF_SIZE_LESS && size >= c->size_start) return FALSE;
        if (c->size_mode == CF_SIZE_BETWEEN && size >= c->size_end) return FALSE;
    }
    if (c->has_start || c->has_end) {
        const char *attribute = cf_criteria_date_attribute(c, info);
        gint64 stamp;
        if (!cf_file_time(info, attribute, &stamp)) return FALSE;
        if (c->has_start && stamp < c->date_start) return FALSE;
        if (c->has_end && stamp >= c->date_end) return FALSE;
    }
    return TRUE;
}

const char *cf_criteria_date_attribute(const CfCriteria *c, GFileInfo *info)
{
    if (c->date_field == CF_DATE_ACCESSED) return G_FILE_ATTRIBUTE_TIME_ACCESS;
    if (c->date_field == CF_DATE_CREATED && g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_TIME_CREATED))
        return G_FILE_ATTRIBUTE_TIME_CREATED;
    return G_FILE_ATTRIBUTE_TIME_MODIFIED;
}
