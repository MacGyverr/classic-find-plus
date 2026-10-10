#include "accounts.h"
#include "config.h"
#include <unistd.h>

typedef struct { GWeakRef combo, enable; gboolean users; } Waiting;
static GPtrArray *waiting, *users, *groups;
static GSubprocess *process;
static guint deadline;
static gboolean complete;
static char *failure;

static void waiting_free(gpointer data)
{
    Waiting *w = data;
    g_weak_ref_clear(&w->combo); g_weak_ref_clear(&w->enable); g_free(w);
}
void cf_accounts_default(GtkComboBoxText *combo)
{
    gboolean is_user = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo), "cf-users"));
    g_autofree char *id = g_strdup_printf("%u", (guint)(is_user ? getuid() : getgid()));
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), id))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
}
static void fill(Waiting *w)
{
    g_autoptr(GtkComboBoxText) combo = g_weak_ref_get(&w->combo);
    g_autoptr(GtkWidget) enable = g_weak_ref_get(&w->enable);
    if (!combo || !enable) return;
    GPtrArray *list = w->users ? users : groups;
    gtk_combo_box_text_remove_all(combo);
    for (guint i = 0; list && i < list->len; i++) {
        char **item = g_ptr_array_index(list, i);
        gtk_combo_box_text_append(combo, item[0], item[1]);
    }
    cf_accounts_default(combo);
    if (failure || !list || !list->len) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable),FALSE);
    gtk_widget_set_sensitive(enable, !failure && list && list->len);
    gtk_widget_set_tooltip_text(GTK_WIDGET(combo), failure ? failure :
        "NSS system/service accounts; matching uses the numeric ID. Catalog is cached until restart.");
}
void cf_accounts_register(GtkComboBoxText *combo, GtkWidget *enable, gboolean is_user)
{
    if (!waiting) waiting = g_ptr_array_new_with_free_func(waiting_free);
    Waiting *w = g_new0(Waiting, 1); w->users = is_user;
    g_weak_ref_init(&w->combo, combo); g_weak_ref_init(&w->enable, enable);
    g_object_set_data(G_OBJECT(combo), "cf-users", GINT_TO_POINTER(is_user));
    gtk_widget_set_sensitive(enable, FALSE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(combo), "System accounts load asynchronously when More Filters opens.");
    if (complete) { fill(w); waiting_free(w); }
    else g_ptr_array_add(waiting, w);
}
static gboolean timed_out(gpointer unused)
{
    (void)unused; deadline = 0;
    failure = g_strdup("Account lookup timed out; restart CFP to retry. Other filters remain available.");
    g_subprocess_force_exit(process);
    return G_SOURCE_REMOVE;
}
static gboolean parse(const char *output)
{
    if (!output || strlen(output) > 4 * 1024 * 1024 || !g_str_has_prefix(output, "CFP-ACCOUNTS-1\n")) return FALSE;
    g_auto(GStrv) lines = g_strsplit(output + strlen("CFP-ACCOUNTS-1\n"), "\n", -1);
    for (guint n = 0; lines[n] && *lines[n]; n++) {
        g_auto(GStrv) parts = g_strsplit(lines[n], "\t", -1);
        if (g_strv_length(parts) != 3 || (strcmp(parts[0], "u") && strcmp(parts[0], "g"))) return FALSE;
        for (const char *p = parts[1]; *p; p++) if (!g_ascii_isdigit(*p)) return FALSE;
        char *end = NULL; guint64 id = g_ascii_strtoull(parts[1], &end, 10);
        if (!*parts[1] || !end || *end || id >= G_MAXUINT32) return FALSE;
        gsize size;
        g_autofree guchar *bytes = g_base64_decode(parts[2], &size);
        g_autofree char *encoded = g_base64_encode(bytes, size);
        if (!size || size > 4096 || strcmp(encoded, parts[2]) || memchr(bytes, 0, size) ||
            !g_utf8_validate((char *)bytes, size, NULL)) return FALSE;
        g_autofree char *name = g_strndup((char *)bytes, size);
        /* Escape control characters in account names; IDs disambiguate display names. */
        g_autoptr(GString) display = g_string_new(NULL);
        for (const char *p = name; *p; p = g_utf8_next_char(p)) {
            gunichar ch = g_utf8_get_char(p);
            if (g_unichar_iscntrl(ch)) g_string_append_printf(display, "\\u%04x", ch);
            else g_string_append_unichar(display, ch);
        }
        char **item = g_new0(char *, 3);
        item[0] = g_strdup_printf("%u", (guint)id);
        item[1] = g_strdup_printf("%s (%s %u)", display->str, *parts[0] == 'u' ? "UID" : "GID", (guint)id);
        GPtrArray *list = *parts[0] == 'u' ? users : groups;
        if (list->len >= 65536) { g_strfreev(item); return FALSE; }
        g_ptr_array_add(list, item);
    }
    return TRUE;
}
static void finished(GObject *object, GAsyncResult *result, gpointer unused)
{
    (void)unused;
    g_autofree char *out = NULL, *err = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object), result, &out, &err, &error) &&
                  g_subprocess_get_successful(G_SUBPROCESS(object));
    if (deadline) { g_source_remove(deadline); deadline = 0; }
    users = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    groups = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    if (!failure && (!ok || !parse(out)))
        failure = g_strdup("System account enumeration unavailable; restart CFP to retry. Other filters remain available.");
    g_clear_object(&process); complete = TRUE;
    for (guint i = 0; i < waiting->len; i++) fill(g_ptr_array_index(waiting, i));
    g_ptr_array_set_size(waiting, 0);
}
void cf_accounts_load(void)
{
    if (complete || process) return;
    const char *helper = g_getenv("CFP_ACCOUNTS_HELPER");
    g_autofree char *executable = g_file_read_link("/proc/self/exe", NULL);
    g_autofree char *bin = executable ? g_path_get_dirname(executable) : NULL;
    g_autofree char *prefix = bin ? g_path_get_dirname(bin) : NULL;
    g_autofree char *relative = prefix ? g_build_filename(prefix, "share/classic-find-plus/accounts.py", NULL) : NULL;
    g_autofree char *development=prefix ? g_build_filename(prefix,"scripts/accounts.py",NULL) : NULL;
    if (!helper || !*helper) {
        if (relative && g_file_test(relative,G_FILE_TEST_IS_REGULAR)) helper=relative;
        else if (development && g_file_test(development,G_FILE_TEST_IS_REGULAR)) helper=development;
        else helper=CF_ACCOUNTS_HELPER;
    }
    g_autoptr(GError) error = NULL;
    process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error,
                              "python3", helper, NULL);
    if (!process) {
        failure = g_strdup("Cannot launch system account lookup; other filters remain available.");
        complete = TRUE;
        for (guint i = 0; i < waiting->len; i++) fill(g_ptr_array_index(waiting, i));
        g_ptr_array_set_size(waiting, 0); return;
    }
    deadline = g_timeout_add_seconds(8, timed_out, NULL);
    g_subprocess_communicate_utf8_async(process, NULL, NULL, finished, NULL);
}
