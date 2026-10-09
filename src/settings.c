#define _POSIX_C_SOURCE 200809L
#include "input-limits.h"
#include "settings.h"
#include <glib/gstdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

struct _CfSettings { GKeyFile *data, *pending, *defaults; guint timer; gboolean migrate; GPtrArray *history; };
char *cf_settings_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "classic-find-plus", "settings.ini", NULL);
}
static void merge(GKeyFile *target, GKeyFile *source)
{
    g_auto(GStrv) groups = g_key_file_get_groups(source, NULL);
    for (guint g = 0; groups[g]; g++) {
        g_auto(GStrv) keys = g_key_file_get_keys(source, groups[g], NULL, NULL);
        for (guint k = 0; keys && keys[k]; k++) {
            g_autofree char *value = g_key_file_get_value(source, groups[g], keys[k], NULL);
            g_key_file_set_value(target, groups[g], keys[k], value);
        }
    }
}
static void missing_defaults(GKeyFile *target, GKeyFile *defaults)
{
    g_auto(GStrv) groups = g_key_file_get_groups(defaults, NULL);
    for (guint g = 0; groups[g]; g++) {
        g_auto(GStrv) keys = g_key_file_get_keys(defaults, groups[g], NULL, NULL);
        for (guint k = 0; keys && keys[k]; k++) if (!g_key_file_has_key(target, groups[g], keys[k], NULL)) {
            g_autofree char *raw = g_key_file_get_value(defaults, groups[g], keys[k], NULL);
            g_key_file_set_value(target, groups[g], keys[k], raw);
        }
    }
}
/* Read regular files only, with a hard byte bound even if a writer grows the file.
 * Failed loads return safe defaults for readers, but strict saves preserve the file. */
static gboolean read_keys(const char *path, GKeyFile *keys, GError **error)
{
    int fd = g_open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW, 0);
    if (fd < 0) {
        if (errno == ENOENT) return TRUE;
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Cannot read settings: %s", g_strerror(errno));
        return FALSE;
    }
    struct stat stat;
    gboolean ok = fstat(fd, &stat) == 0 && S_ISREG(stat.st_mode) && stat.st_size <= CF_SETTINGS_LIMIT;
    g_autoptr(GByteArray) bytes = g_byte_array_new();
    while (ok) {
        guint8 buffer[8192];
        ssize_t n = read(fd, buffer, MIN(sizeof buffer, CF_SETTINGS_LIMIT + 1 - bytes->len));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { ok = FALSE; break; }
        if (!n) break;
        g_byte_array_append(bytes, buffer, n);
        if (bytes->len > CF_SETTINGS_LIMIT) { ok = FALSE; break; }
    }
    close(fd);
    if (!ok) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "Settings must be a readable regular file of at most 1 MiB; existing file preserved.");
        return FALSE;
    }
    return !bytes->len || g_key_file_load_from_data(keys, (const char *)bytes->data, bytes->len, G_KEY_FILE_KEEP_COMMENTS, error);
}
static GKeyFile *load(GError **error)
{
    GKeyFile *data = g_key_file_new();
    g_autofree char *path = cf_settings_path();
    if (!read_keys(path, data, error)) {
        g_key_file_unref(data);
        return error ? NULL : g_key_file_new();
    }
    return data;
}
static gboolean save_later(gpointer data)
{
    CfSettings *s = data;
    s->timer = 0;
    g_autoptr(GError) error = NULL;
    if (!cf_settings_flush(s, &error)) g_printerr("Classic Find Plus settings: %s\n", error->message);
    return G_SOURCE_REMOVE;
}
static void queue(CfSettings *s)
{
    if (s->timer) g_source_remove(s->timer);
    s->timer = g_timeout_add(350, save_later, s);
}
CfSettings *cf_settings_new(void)
{
    CfSettings *s = g_new0(CfSettings, 1);
    s->data = load(NULL); s->pending = g_key_file_new(); s->defaults = g_key_file_new();
    s->history = g_ptr_array_new_with_free_func(g_free);
    g_autofree char *path = cf_settings_path();
    /* Migrate once, only when the new configuration does not exist. Keep originals intact. */
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        const char *files[] = { "window.ini", "columns.ini" };
        for (guint n = 0; n < G_N_ELEMENTS(files); n++) {
            g_autofree char *old = g_build_filename(g_get_user_config_dir(), "classic-find", files[n], NULL);
            g_autoptr(GKeyFile) legacy = g_key_file_new();
            if (read_keys(old, legacy, NULL) && g_file_test(old, G_FILE_TEST_EXISTS)) {
                merge(s->data, legacy); s->migrate = TRUE;
            }
        }
    }
    return s;
}
GKeyFile *cf_settings_data(CfSettings *s) { return s->data; }
int cf_settings_int(CfSettings *s, const char *group, const char *key, int fallback, int min, int max)
{
    g_autoptr(GError) error = NULL;
    int value = g_key_file_get_integer(s->data, group, key, &error);
    return !error && value >= min && value <= max ? value : fallback;
}
gboolean cf_settings_bool(CfSettings *s, const char *group, const char *key, gboolean fallback)
{
    g_autoptr(GError) error = NULL;
    gboolean value = g_key_file_get_boolean(s->data, group, key, &error);
    return error ? fallback : value;
}
static void set(CfSettings *s, const char *group, const char *key, const char *raw)
{
    g_autofree char *previous = g_key_file_get_value(s->data, group, key, NULL);
    if (!g_strcmp0(previous, raw)) return;
    g_key_file_set_value(s->data, group, key, raw);
    g_key_file_set_value(s->pending, group, key, raw);
    queue(s);
}
void cf_settings_set_string(CfSettings *s, const char *group, const char *key, const char *value)
{
    g_autoptr(GKeyFile) encoded = g_key_file_new();
    g_key_file_set_string(encoded, group, key, value ? value : "");
    g_autofree char *raw = g_key_file_get_value(encoded, group, key, NULL);
    set(s, group, key, raw);
}
void cf_settings_set_int(CfSettings *s, const char *group, const char *key, int value)
{
    g_autofree char *raw = g_strdup_printf("%d", value); set(s, group, key, raw);
}
void cf_settings_set_window_position(CfSettings *s, int x, int y)
{
    /* A changed global position is a pair. Comparing each coordinate against stale
     * per-window data could otherwise merge X from one window with Y from another. */
    g_key_file_set_integer(s->data, "Window", "X", x);
    g_key_file_set_integer(s->data, "Window", "Y", y);
    g_key_file_set_integer(s->pending, "Window", "X", x);
    g_key_file_set_integer(s->pending, "Window", "Y", y);
    queue(s);
}
void cf_settings_set_bool(CfSettings *s, const char *group, const char *key, gboolean value)
{
    set(s, group, key, value ? "true" : "false");
}
void cf_settings_set_strings(CfSettings *s, const char *group, const char *key, const char *const *values, gsize count)
{
    g_autoptr(GKeyFile) encoded = g_key_file_new();
    g_key_file_set_string_list(encoded, group, key, values, count);
    g_autofree char *raw = g_key_file_get_value(encoded, group, key, NULL);
    set(s, group, key, raw);
}
static GPtrArray *history_read(GKeyFile *key)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    g_auto(GStrv) entries = g_key_file_get_string_list(key, "History", "Names", NULL, NULL);
    for (guint i = 0; entries && entries[i] && list->len < 20; i++) {
        if (!*entries[i] || !cf_input_valid(entries[i], CF_TEXT_LIMIT)) continue;
        gboolean duplicate = FALSE;
        for (guint j = 0; j < list->len; j++) duplicate |= !strcmp(entries[i], g_ptr_array_index(list, j));
        if (!duplicate) g_ptr_array_add(list, g_strdup(entries[i]));
    }
    return list;
}
static void history_apply(CfSettings *s, GKeyFile *latest)
{
    if (!s->history->len) return;
    g_autoptr(GPtrArray) list = history_read(latest);
    for (guint i = 0; i < s->history->len; i++) {
        const char *name = g_ptr_array_index(s->history, i);
        if (!name) { g_ptr_array_set_size(list, 0); continue; }
        for (guint j = 0; j < list->len; j++)
            if (!strcmp(name, g_ptr_array_index(list, j))) { g_ptr_array_remove_index(list, j); break; }
        g_ptr_array_insert(list, 0, g_strdup(name));
        if (list->len > 20) g_ptr_array_remove_index(list, 20);
    }
    g_key_file_set_string_list(latest, "History", "Names", (const char *const *)list->pdata, list->len);
}
static gboolean io_error(GError **error)
{
    int saved = errno;
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved), "Cannot save settings: %s", g_strerror(saved));
    return FALSE;
}
gboolean cf_settings_flush(CfSettings *s, GError **error)
{
    if (s->timer) { g_source_remove(s->timer); s->timer = 0; }
    gsize groups;
    g_auto(GStrv) names = g_key_file_get_groups(s->pending, &groups);
    gsize defaults_count;
    g_auto(GStrv) default_groups = g_key_file_get_groups(s->defaults, &defaults_count);
    if (!groups && !defaults_count && !s->migrate && !s->history->len) return TRUE;
    g_autofree char *path = cf_settings_path();
    g_autofree char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) < 0) return io_error(error);
    g_autofree char *lock = g_build_filename(directory, "settings.lock", NULL);
    int fd = g_open(lock, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return io_error(error);
    int status;
    do { status = flock(fd, LOCK_EX); } while (status < 0 && errno == EINTR);
    if (status < 0) { int saved = errno; close(fd); errno = saved; return io_error(error); }
    g_autoptr(GError) read_error = NULL;
    g_autoptr(GKeyFile) latest = load(&read_error);
    if (!latest) {
        flock(fd, LOCK_UN); close(fd);
        g_propagate_error(error, g_steal_pointer(&read_error)); return FALSE;
    }
    if (s->migrate && !g_file_test(path, G_FILE_TEST_EXISTS)) merge(latest, s->data);
    missing_defaults(latest, s->defaults);
    merge(latest, s->pending);
    history_apply(s, latest);
    gsize length;
    g_autofree char *contents = g_key_file_to_data(latest, &length, NULL);
    g_autoptr(GFile) file = g_file_new_for_path(path);
    gboolean ok;
    if (length > CF_SETTINGS_LIMIT) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE, "Settings exceed the 1 MiB limit; existing file preserved.");
        ok = FALSE;
    } else ok = g_file_replace_contents(file, contents, length, NULL, FALSE,
        G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, error);
    flock(fd, LOCK_UN); close(fd);
    if (ok) { g_key_file_unref(s->pending); s->pending = g_key_file_new(); s->migrate = FALSE; g_ptr_array_set_size(s->history, 0);
        g_key_file_unref(s->defaults); s->defaults = g_key_file_new(); }
    return ok;
}
void cf_settings_free(CfSettings *s)
{
    if (!s) return;
    g_autoptr(GError) error = NULL;
    if (!cf_settings_flush(s, &error)) g_printerr("Classic Find Plus settings: %s\n", error->message);
    g_key_file_unref(s->data); g_key_file_unref(s->pending); g_key_file_unref(s->defaults); g_ptr_array_unref(s->history); g_free(s);
}

char **cf_settings_history(CfSettings *s)
{
    /* Flush local operations, then read fresh entries from other windows/processes. */
    if (!cf_settings_flush(s, NULL)) {
        g_autoptr(GKeyFile) copy = load(NULL); history_apply(s, copy);
        GPtrArray *list = history_read(copy); g_ptr_array_add(list, NULL);
        return (char **)g_ptr_array_free(list, FALSE);
    }
    g_autoptr(GKeyFile) latest = load(NULL);
    GPtrArray *list = history_read(latest); g_ptr_array_add(list, NULL);
    return (char **)g_ptr_array_free(list, FALSE);
}
void cf_settings_history_record(CfSettings *s, const char *name)
{
    if (!cf_input_valid(name, CF_TEXT_LIMIT) || !*name) return;
    g_ptr_array_add(s->history, g_strdup(name)); queue(s);
}
void cf_settings_history_clear(CfSettings *s)
{
    g_ptr_array_add(s->history, NULL); queue(s);
}

void cf_settings_defaults(CfSettings *s, GKeyFile *defaults)
{
    g_auto(GStrv) groups = g_key_file_get_groups(defaults, NULL);
    for (guint g = 0; groups[g]; g++) {
        g_auto(GStrv) keys = g_key_file_get_keys(defaults, groups[g], NULL, NULL);
        for (guint k = 0; keys && keys[k]; k++) if (!g_key_file_has_key(s->data, groups[g], keys[k], NULL)) {
            g_autofree char *raw = g_key_file_get_value(defaults, groups[g], keys[k], NULL);
            g_key_file_set_value(s->defaults, groups[g], keys[k], raw);
            g_key_file_set_value(s->data, groups[g], keys[k], raw);
            queue(s);
        }
    }
}
