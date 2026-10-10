#include "admin-protocol.h"

typedef struct { GDBusConnection *bus; const char *id; } Request;
static void cancel_request(GCancellable *cancel, gpointer data)
{
    (void) cancel;
    Request *request = data;
    g_dbus_connection_call(request->bus, CF_ADMIN_BUS, CF_ADMIN_PATH, CF_ADMIN_INTERFACE,
        "Cancel", g_variant_new("(s)", request->id), G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, 2000, NULL, NULL, NULL);
}
void cf_admin_client_run(GFile *root, CfCriteria *criteria, GCancellable *cancel,
    CfAdminResultFunc result_func, CfAdminStatsFunc stats_func, gpointer data, GError **error)
{
    g_autofree char *uri = g_file_get_uri(root);
    g_autoptr(GFile) checked = cf_admin_root(uri, error);
    if (!checked) return;
    /* Dedicated connection: window closure disconnects this request without
     * waiting for other application users of the shared system bus. */
    g_autofree char *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, cancel, error);
    if (!address) return;
    g_autoptr(GDBusConnection) bus = g_dbus_connection_new_for_address_sync(address,
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
        NULL, cancel, error);
    if (!bus) return;
    g_dbus_connection_set_exit_on_close(bus, FALSE);
    g_autofree char *id = g_uuid_string_random();
    Request request = { bus, id };
    gulong handler = g_cancellable_connect(cancel, G_CALLBACK(cancel_request), &request, NULL);
    g_autoptr(GVariant) started = g_dbus_connection_call_sync(bus, CF_ADMIN_BUS, CF_ADMIN_PATH, CF_ADMIN_INTERFACE,
        "Start", g_variant_new("(ss@" CF_ADMIN_CRITERIA_TYPE ")", id, uri, cf_admin_criteria_encode(criteria)),
        G_VARIANT_TYPE("()"), G_DBUS_CALL_FLAGS_NONE, 125000, cancel, error);
    if (!started) goto done;
    while (!g_cancellable_is_cancelled(cancel)) {
        g_autoptr(GVariant) reply = g_dbus_connection_call_sync(bus, CF_ADMIN_BUS, CF_ADMIN_PATH, CF_ADMIN_INTERFACE,
            "Read", g_variant_new("(su)", id, CF_ADMIN_BATCH),
            G_VARIANT_TYPE("(a(sa{sv})" CF_ADMIN_STATS_TYPE ")"),
            G_DBUS_CALL_FLAGS_NO_AUTO_START, 5000, cancel, error);
        if (!reply) break;
        if (g_variant_get_size(reply) > CF_ADMIN_MAX_MESSAGE) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Administrator search reply too large."); break;
        }
        g_autoptr(GVariant) rows = g_variant_get_child_value(reply, 0);
        if (g_variant_n_children(rows) > CF_ADMIN_BATCH) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Administrator search batch too large."); break;
        }
        for (gsize i = 0; i < g_variant_n_children(rows); i++) {
            g_autoptr(GVariant) row = g_variant_get_child_value(rows, i);
            CfResult *result = cf_admin_result_decode(row, error);
            if (!result) goto done;
            if (!g_file_has_prefix(result->file, root)) {
                cf_result_free(result);
                g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Administrator result is outside the search root.");
                goto done;
            }
            result_func(result, data);
        }
        g_autoptr(GVariant) value = g_variant_get_child_value(reply, 1);
        CfSearchStats stats;
        if (!cf_admin_stats_decode(value, &stats, error)) break;
        stats_func(&stats, data);
        g_free(stats.first_error);
        if (stats.finished) break;
        /* Bounded pull requests; the GUI consumes its usual bounded queue. */
        if (!g_variant_n_children(rows)) g_usleep(30000);
    }
done:
    g_cancellable_disconnect(cancel, handler);
    /* Closing the dedicated bus connection also cancels pending authorization
     * even if Stop raced with activation before the Start message arrived. */
    g_dbus_connection_close_sync(bus, NULL, NULL);
}
