#define _GNU_SOURCE
#include "admin-protocol.h"
#include "read-only.h"
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <limits.h>
#include <errno.h>

#ifdef CF_WORKER_TEST
/* Private fixture accelerates deadlines only; authorization remains mandatory. */
#define START_IDLE 1500000
#define AUTH_TIMEOUT 1000000
#define READ_IDLE 1000000
#define MAX_LIFETIME 4000000
#define CANCEL_GRACE 500000
#else
#define START_IDLE 10000000
#define AUTH_TIMEOUT 120000000
#define READ_IDLE 30000000
#define MAX_LIFETIME 900000000
#define CANCEL_GRACE 5000000
#endif

static const char xml[] =
    "<node><interface name='" CF_ADMIN_INTERFACE "'>"
    "<method name='Start'><arg type='s' direction='in'/><arg type='s' direction='in'/>"
    "<arg type='" CF_ADMIN_CRITERIA_TYPE "' direction='in'/></method>"
    "<method name='Read'><arg type='s' direction='in'/><arg type='u' direction='in'/>"
    "<arg type='a(sa{sv})' direction='out'/><arg type='" CF_ADMIN_STATS_TYPE "' direction='out'/></method>"
    "<method name='Cancel'><arg type='s' direction='in'/></method>"
    "</interface></node>";
typedef struct {
    GMainLoop *loop;
    GDBusConnection *bus;
    GDBusMethodInvocation *pending;
    GCancellable *authorization;
    CfSearch *search;
    CfCriteria *criteria;
    GFile *root;
    char *owner, *id, *auth_id;
    gint64 started, last_read, stop_at;
    gboolean exiting;
} Service;

static void fail(GDBusMethodInvocation *call, const char *code, const char *message)
{
    g_autofree char *name = g_strconcat(CF_ADMIN_INTERFACE, ".", code, NULL);
    g_dbus_method_invocation_return_dbus_error(call, name, message);
}
static void stop(Service *s, gboolean abandon)
{
    if (s->exiting) return;
    if (s->authorization) {
        g_cancellable_cancel(s->authorization);
        g_dbus_connection_call(s->bus, "org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
            "org.freedesktop.PolicyKit1.Authority", "CancelCheckAuthorization",
            g_variant_new("(s)", s->auth_id), NULL, G_DBUS_CALL_FLAGS_NONE, 2000, NULL, NULL, NULL);
    }
    if (s->pending) {
        fail(s->pending, "Cancelled", "Administrator search cancelled.");
        g_clear_object(&s->pending);
    }
    if (s->search) cf_search_cancel(s->search);
    s->stop_at = g_get_monotonic_time() + (abandon ? 200000 : CANCEL_GRACE);
    s->exiting = TRUE;
}
static void authorized(GObject *object, GAsyncResult *result, gpointer data)
{
    Service *s = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    g_clear_object(&s->authorization);
    if (!s->pending) return;
    gboolean allowed = FALSE, challenge = FALSE;
    g_autoptr(GVariant) details = NULL;
    if (reply) g_variant_get(reply, "((bb@a{ss}))", &allowed, &challenge, &details);
    (void) challenge;
    if (!allowed) {
        fail(s->pending, "NotAuthorized", error ? error->message : "Authorization denied or authentication cancelled.");
        g_clear_object(&s->pending);
        stop(s, TRUE);
        return;
    }
    s->search = cf_search_start(s->root, g_steal_pointer(&s->criteria), &error);
    if (!s->search) {
        fail(s->pending, "Failed", error->message);
        g_clear_object(&s->pending); stop(s, TRUE); return;
    }
    s->last_read = g_get_monotonic_time();
    g_dbus_method_invocation_return_value(s->pending, NULL);
    g_clear_object(&s->pending);
}
static gboolean identity(Service *s, const char *sender)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(s->bus,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixUser",
        g_variant_new("(s)", sender), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
    return reply != NULL && sender[0] == ':';
}
static void method(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *name, GVariant *params, GDBusMethodInvocation *call, gpointer data)
{
    (void) bus; (void) path; (void) interface;
    Service *s = data;
    if (g_variant_get_size(params) > CF_ADMIN_MAX_MESSAGE) { fail(call, "Invalid", "Message too large."); return; }
    const char *id;
    g_variant_get_child(params, 0, "&s", &id);
    if (!g_uuid_string_is_valid(id)) { fail(call, "Invalid", "Invalid request identifier."); return; }
    if (!strcmp(name, "Start")) {
        if (s->owner || s->exiting) { fail(call, "Busy", "Worker already belongs to a request; retry after it exits."); return; }
        g_autoptr(GError) error = NULL;
        const char *uri;
        g_variant_get_child(params, 1, "&s", &uri);
        g_autoptr(GVariant) value = g_variant_get_child_value(params, 2);
        g_autoptr(GFile) root = cf_admin_root(uri, &error);
        g_autoptr(CfCriteria) criteria = root ? cf_admin_criteria_decode(value, &error) : NULL;
        if (!criteria) { fail(call, "Invalid", error->message); return; }
        if (!identity(s, sender)) { fail(call, "NotAuthorized", "Cannot identify requesting bus user."); return; }
        s->owner = g_strdup(sender); s->id = g_strdup(id); s->auth_id = g_uuid_string_random();
        s->root = g_steal_pointer(&root); s->criteria = g_steal_pointer(&criteria);
        s->pending = g_object_ref(call); s->authorization = g_cancellable_new();
        s->started = s->last_read = g_get_monotonic_time();
        GVariantBuilder subject, details;
        g_variant_builder_init(&subject, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&subject, "{sv}", "name", g_variant_new_string(sender));
        g_variant_builder_init(&details, G_VARIANT_TYPE("a{ss}"));
        g_variant_builder_add(&details, "{ss}", "polkit.message", "Classic Find Plus needs administrator permission to search protected folders.");
        g_dbus_connection_call(s->bus, "org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
            "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization",
            g_variant_new("((s@a{sv})s@a{ss}us)", "system-bus-name", g_variant_builder_end(&subject),
                CF_ADMIN_ACTION, g_variant_builder_end(&details), 1u, s->auth_id),
            G_VARIANT_TYPE("((bba{ss}))"), G_DBUS_CALL_FLAGS_NONE, 120000, s->authorization, authorized, s);
        return;
    }
    if (g_strcmp0(s->owner, sender) || g_strcmp0(s->id, id)) {
        fail(call, "NotAuthorized", "Request belongs to another caller or identifier."); return;
    }
    if (!strcmp(name, "Cancel")) {
        stop(s, FALSE);
        g_dbus_method_invocation_return_value(call, NULL);
        return;
    }
    if (!s->search) { fail(call, "NotReady", "Authorization has not completed."); return; }
    guint count;
    g_variant_get_child(params, 1, "u", &count);
    if (!count || count > CF_ADMIN_BATCH) { fail(call, "Invalid", "Invalid batch size."); return; }
    GVariantBuilder rows;
    g_variant_builder_init(&rows, G_VARIANT_TYPE("a(sa{sv})"));
    gsize bytes = 0;
    for (guint i = 0; i < count && bytes < 48 * 1024; i++) {
        g_autoptr(CfResult) r = cf_search_pop(s->search);
        if (!r) break;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) row = cf_admin_result_encode(r, &error);
        if (!row) {
            g_variant_builder_clear(&rows); fail(call, "Failed", error->message); stop(s, TRUE); return;
        }
        bytes += g_variant_get_size(row);
        g_variant_builder_add_value(&rows, row);
    }
    CfSearchStats stats;
    cf_search_get_stats(s->search, &stats);
    g_dbus_method_invocation_return_value(call, g_variant_new("(@a(sa{sv})@" CF_ADMIN_STATS_TYPE ")",
        g_variant_builder_end(&rows), cf_admin_stats_encode(&stats)));
    g_free(stats.first_error);
    s->last_read = g_get_monotonic_time();
    if (stats.finished) { s->stop_at = s->last_read + 300000; s->exiting = TRUE; }
}
static void owner_changed(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *name, GVariant *params, gpointer data)
{
    (void) bus; (void) sender; (void) path; (void) interface; (void) name;
    Service *s = data;
    const char *changed, *old, *current;
    g_variant_get(params, "(&s&s&s)", &changed, &old, &current);
    if (s->owner && !strcmp(changed, s->owner) && !*current) stop(s, TRUE);
}
static gboolean tick(gpointer data)
{
    Service *s = data;
    gint64 now = g_get_monotonic_time();
    if (s->stop_at && now >= s->stop_at) {
        /* A backend/NSS may be stuck in a syscall. Process exit, rather than a
         * blocking thread-pool join, guarantees no orphan privileged worker. */
        _exit(0);
    }
    if (!s->exiting && ((!s->owner && now - s->started > START_IDLE) ||
        (s->pending && now - s->started > AUTH_TIMEOUT) ||
        (s->search && (now - s->last_read > READ_IDLE || now - s->started > MAX_LIFETIME)))) stop(s, TRUE);
    if (g_dbus_connection_is_closed(s->bus)) stop(s, TRUE);
    return G_SOURCE_CONTINUE;
}
int main(void)
{
#ifndef CF_WORKER_TEST
    if (geteuid() != 0) { g_printerr("Search worker requires system-bus activation as root.\n"); return 1; }
#endif
    /* No command arguments, inherited descriptors, credentials or user plugins.
     * GIO local VFS prevents metadata requests invoking remote desktop backends. */
#ifdef __NR_close_range
    if (syscall(__NR_close_range, 3u, UINT_MAX, 0u) < 0)
#endif
    {
        long maximum = sysconf(_SC_OPEN_MAX);
        for (int fd = 3; fd < maximum; fd++) close(fd);
    }
    g_setenv("GIO_USE_VFS", "local", TRUE);
    g_unsetenv("GIO_EXTRA_MODULES");
    g_setenv("HOME", "/", TRUE);
    struct rlimit memory = { 512 * 1024 * 1024, 512 * 1024 * 1024 };
    struct rlimit files = { 64, 64 }, core = { 0, 0 };
    struct rlimit cpu = { 120, 120 };
    if (setrlimit(RLIMIT_AS, &memory) || setrlimit(RLIMIT_NOFILE, &files) || setrlimit(RLIMIT_CORE, &core) || setrlimit(RLIMIT_CPU, &cpu)) {
        g_printerr("Cannot set search worker resource limits: %s\n", g_strerror(errno)); return 1;
    }
    g_autoptr(GError) error = NULL;
    Service s = { .started = g_get_monotonic_time() };
    s.bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!s.bus) { g_printerr("%s\n", error->message); return 1; }
    g_dbus_connection_set_exit_on_close(s.bus, FALSE);
    g_autoptr(GDBusNodeInfo) node = g_dbus_node_info_new_for_xml(xml, &error);
    static const GDBusInterfaceVTable vtable = { .method_call = method };
    if (!node || !g_dbus_connection_register_object(s.bus, CF_ADMIN_PATH, node->interfaces[0], &vtable, &s, NULL, &error)) {
        g_printerr("%s\n", error->message); return 1;
    }
    g_dbus_connection_signal_subscribe(s.bus, "org.freedesktop.DBus", "org.freedesktop.DBus", "NameOwnerChanged",
        "/org/freedesktop/DBus", NULL, G_DBUS_SIGNAL_FLAGS_NONE, owner_changed, &s, NULL);
    if (!cf_worker_read_only(&error)) { g_printerr("%s\n", error->message); return 1; }
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(s.bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "RequestName", g_variant_new("(su)", CF_ADMIN_BUS, 4u), G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &error);
    guint acquired = 0;
    if (reply) g_variant_get(reply, "(u)", &acquired);
    if (acquired != 1) { g_printerr("Cannot own privileged search service name.\n"); return 1; }
    s.loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(100, tick, &s);
    g_main_loop_run(s.loop);
    return 0;
}
