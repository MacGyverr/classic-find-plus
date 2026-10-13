#include "metadata.h"
#include "results-sync.h"
#include "criteria-private.h"
#include "traversal.h"

#define KEY "cf-results-sync"
#define ATTRIBUTES "standard::name,standard::type,standard::is-symlink,standard::size,standard::icon,standard::content-type,time::created,time::created-usec,time::access,time::access-usec,unix::mode,unix::uid,unix::gid,time::modified,time::modified-usec,id::filesystem,id::file"
#define WATCH_LIMIT 64
#ifdef CF_SYNC_TEST_FAST
#define WINDOW_US (3 * G_USEC_PER_SEC)
#else
#define WINDOW_US (120 * G_USEC_PER_SEC)
#endif
#define CHECK_LIMIT 128
#ifdef CF_SYNC_TEST_FAST
static const guint retries_ms[] = { 0, 50, 150, 300, 600, 1200, 2000, 2800 };
#else
static const guint retries_ms[] = { 0, 250, 1000, 3000, 10000, 30000, 60000, 110000 };
#endif

typedef struct {
    guint refs, revision, retry;
    GFile *file, *destination, *origin;
    char *identity;
    GtkTreeRowReference *row;
    gint64 started, due, expires;
    gboolean dragging, dirty, directory;
} Item;

typedef struct {
    guint refs, generation, timer;
    GWeakRef view;
    GFile *root;
    GtkTreeModel *model;
    CfCriteria *criteria;
    GCancellable *cancel;
    GHashTable *items, *folders, *monitors;
    gint64 scheduled_at;
    gboolean closed, running;
} Sync;

typedef enum { KEEP, UPDATE, REMOVE } Outcome;
typedef struct {
    GFile *original, *destination, *result_file;
    char *identity;
    GFileInfo *info;
    guint revision;
    Outcome outcome;
} Check;
typedef struct { GPtrArray *checks; GFile *root; CfCriteria *criteria; guint generation; } Job;
typedef struct { Sync *sync; GPtrArray *items; } Completion;

static void schedule(Sync *s);
static void row_published(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static Sync *sync_ref(Sync *s) { s->refs++; return s; }
static Item *item_ref(Item *item) { item->refs++; return item; }
static void item_unref(gpointer data)
{
    Item *item = data;
    if (--item->refs) return;
    g_object_unref(item->file);
    g_clear_object(&item->destination);
    g_clear_object(&item->origin);
    g_free(item->identity);
    gtk_tree_row_reference_free(item->row);
    g_free(item);
}
static void monitor_free(gpointer data)
{
    GFileMonitor *monitor = data;
    g_signal_handlers_disconnect_by_data(monitor, g_object_get_data(G_OBJECT(monitor), KEY));
    g_file_monitor_cancel(monitor);
    g_object_unref(monitor);
}
static void sync_unref(gpointer data)
{
    Sync *s = data;
    if (--s->refs) return;
    g_weak_ref_clear(&s->view);
    g_clear_object(&s->root);
    cf_criteria_free(s->criteria);
    g_object_unref(s->cancel);
    g_hash_table_unref(s->monitors);
    g_hash_table_unref(s->items);
    g_hash_table_unref(s->folders);
    g_object_unref(s->model);
    g_free(s);
}
static void check_free(gpointer data)
{
    Check *check = data;
    g_object_unref(check->original);
    g_clear_object(&check->destination);
    g_clear_object(&check->result_file);
    g_clear_object(&check->info);
    g_free(check->identity);
    g_free(check);
}
static void job_free(gpointer data)
{
    Job *job = data;
    g_ptr_array_unref(job->checks);
    g_clear_object(&job->root);
    cf_criteria_free(job->criteria);
    g_free(job);
}
static gboolean missing(GError *error)
{
    return g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
           g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY);
}
static gboolean complete_info(const CfCriteria *criteria, GFileInfo *info)
{
    /* Some GIO backends return only a name when stat fails, without a GError. */
    return info && g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_NAME) &&
        g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
        g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE) &&
        (!criteria || (!criteria->has_start && !criteria->has_end) ||
         g_file_info_has_attribute(info, cf_criteria_date_attribute(criteria, info)));
}
/* Runs only in the verification worker. Do not traverse symlink/mount ancestors that the
 * captured search would not descend into. Unknown scope/identity never invents a new row. */
static gboolean in_scope(Job *job, GFile *file, GCancellable *cancel)
{
    if (!job->root || !job->criteria) return TRUE;
    if (!g_file_has_prefix(file, job->root)) return FALSE;
    g_autoptr(GFile) parent = g_file_get_parent(file);
    if (!job->criteria->recursive) return parent && g_file_equal(parent, job->root);
    g_autoptr(GFileInfo) root_info = NULL;
    const char *filesystem = NULL;
    if (job->criteria->stay_on_filesystem) {
        root_info = g_file_query_info(job->root, "id::filesystem", G_FILE_QUERY_INFO_NONE, cancel, NULL);
        filesystem = root_info ? g_file_info_get_attribute_string(root_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM) : NULL;
        if (!filesystem) return FALSE;
    }
    while (parent && !g_file_equal(parent, job->root)) {
        g_autoptr(GFileInfo) info = g_file_query_info(parent,
            "standard::name,standard::type,standard::is-symlink,id::filesystem", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, NULL);
        if (!info || !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE)) return FALSE;
        g_autoptr(GFileInfo) target=NULL;
        if (g_file_info_get_is_symlink(info)) {
            if (!job->criteria->follow_symlinks ||
                !cf_traversal_target_allowed(job->criteria,job->root,parent,cancel,NULL)) return FALSE;
            target=g_file_query_info(parent,"standard::type,id::filesystem,id::file",G_FILE_QUERY_INFO_NONE,cancel,NULL);
            if (!target) return FALSE;
            g_autofree char *identity=cf_traversal_identity(target);
            if (!identity) return FALSE;
            g_file_info_set_file_type(info,g_file_info_get_file_type(target));
        }
        if (g_file_info_get_file_type(info)!=G_FILE_TYPE_DIRECTORY || cf_criteria_excludes(job->criteria,info)) return FALSE;
        if (filesystem && g_strcmp0(filesystem, g_file_info_get_attribute_string(target ? target : info,G_FILE_ATTRIBUTE_ID_FILESYSTEM))) return FALSE;
        GFile *next = g_file_get_parent(parent);
        g_object_unref(parent);
        parent = next;
    }
    return parent != NULL;
}
static void verify_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
    (void)source;
    Job *job = data;
    g_autoptr(CfMetadataCache) metadata = cf_metadata_cache_new();
    for (guint n = 0; n < job->checks->len && !g_cancellable_is_cancelled(cancel); n++) {
        Check *check = g_ptr_array_index(job->checks, n);
        g_autoptr(GError) error = NULL;
        check->info = g_file_query_info(check->original, g_file_is_native(check->original) ? ATTRIBUTES : ATTRIBUTES ",owner::user,owner::group", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, &error);
        if (check->info) {
            if (complete_info(job->criteria, check->info)) check->result_file = g_object_ref(check->original);
        }
        else if (missing(error)) {
            check->outcome = REMOVE;
            if (check->destination && check->identity && in_scope(job, check->destination, cancel)) {
                check->info = g_file_query_info(check->destination, g_file_is_native(check->destination) ? ATTRIBUTES : ATTRIBUTES ",owner::user,owner::group",
                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancel, NULL);
                const char *id = check->info ? g_file_info_get_attribute_string(check->info, G_FILE_ATTRIBUTE_ID_FILE) : NULL;
                if (id && !g_strcmp0(id, check->identity) && complete_info(job->criteria, check->info))
                    check->result_file = g_object_ref(check->destination);
            }
        }
        if (check->info && g_file_is_native(check->original)) cf_metadata_prepare(metadata, check->info);
        /* Permission, mount, backend and transient errors leave the row alone. */
        if (check->result_file) {
            if (!job->criteria) check->outcome = UPDATE;
            else if (!cf_criteria_matches(job->criteria, check->info)) check->outcome = REMOVE;
            else {
                g_clear_error(&error);
                gboolean match = cf_traversal_content_matches(job->criteria,job->root,check->result_file,cancel,&error);
                /* An ordinary GUI cannot reread protected admin results. Preserve
                 * existing rows on permission/transient errors, as for metadata. */
                if (!error) check->outcome = match ? UPDATE : REMOVE;
            }
        }
    }
    g_task_return_boolean(task, TRUE);
}
static gboolean item_iter(GtkTreeModel *model, Item *item, GtkTreeIter *iter)
{
    g_autoptr(GtkTreePath) path = gtk_tree_row_reference_get_path(item->row);
    return path && gtk_tree_model_get_iter(model, iter, path);
}
static void rekey(Sync *s, Item *item, GFile *file)
{
    if (g_file_equal(item->file, file)) return;
    gpointer key, value;
    if (!g_hash_table_steal_extended(s->items, item->file, &key, &value)) return;
    g_object_unref(key);
    g_set_object(&item->file, file);
    g_hash_table_replace(s->items, g_object_ref(file), value);
}
static void mark_changed(Item *item, GFile *file, GFile *other, gboolean moved)
{
    if (moved) {
        g_autofree char *relative = g_file_equal(item->file, file) ? NULL : g_file_get_relative_path(file, item->file);
        g_autoptr(GFile) destination = relative ? g_file_resolve_relative_path(other, relative) : g_object_ref(other);
        g_set_object(&item->destination, destination);
    }
    item->revision++;
    item->dirty = TRUE;
}
static void changed(GFileMonitor *monitor, GFile *file, GFile *other, GFileMonitorEvent event, gpointer data)
{
    Sync *s = data;
    if (s->closed || event == G_FILE_MONITOR_EVENT_PRE_UNMOUNT || event == G_FILE_MONITOR_EVENT_UNMOUNTED) return;
    gboolean moved = other && (event == G_FILE_MONITOR_EVENT_RENAMED || event == G_FILE_MONITOR_EVENT_MOVED ||
                               event == G_FILE_MONITOR_EVENT_MOVED_OUT);
    Item *direct = g_hash_table_lookup(s->items, file);
    GFile *parent = g_object_get_data(G_OBJECT(monitor), "cf-parent");
    gboolean ready = FALSE;
    if (direct && !direct->directory) {
        mark_changed(direct, file, other, moved);
        ready = !direct->dragging;
    }
    else if (direct || (parent && g_file_equal(parent, file))) {
        GHashTableIter iter; gpointer value;
        g_hash_table_iter_init(&iter, s->items);
        while (g_hash_table_iter_next(&iter, NULL, &value)) {
            Item *item = value;
            if (g_file_equal(item->file, file) || g_file_has_prefix(item->file, file)) {
                mark_changed(item, file, other, moved);
                ready |= !item->dragging;
            }
        }
    } else return;
    if (ready && !s->running && (!s->timer || s->scheduled_at > g_get_monotonic_time() + 150000))
        schedule(s);
}
static void watch_parent(Sync *s, GFile *file)
{
    g_autoptr(GFile) parent = g_file_get_parent(file);
    if (!parent || !g_file_is_native(parent) || g_hash_table_contains(s->monitors, parent) ||
        g_hash_table_size(s->monitors) >= WATCH_LIMIT) return;
    /* Native directory monitoring installs a local watcher; all metadata verification is off GTK. */
    GFileMonitor *monitor = g_file_monitor_directory(parent, G_FILE_MONITOR_WATCH_MOVES, s->cancel, NULL);
    if (!monitor) return; /* Delayed checks still work on unsupported mounts/backends. */
    g_file_monitor_set_rate_limit(monitor, 100);
    g_object_set_data(G_OBJECT(monitor), KEY, s);
    g_object_set_data_full(G_OBJECT(monitor), "cf-parent", g_object_ref(parent), g_object_unref);
    g_signal_connect(monitor, "changed", G_CALLBACK(changed), s);
    g_hash_table_insert(s->monitors, g_object_ref(parent), monitor);
}
static void prune_monitors(Sync *s)
{
    g_autoptr(GHashTable) parents = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
    GHashTableIter iter; gpointer value, key;
    g_hash_table_iter_init(&iter, s->items);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        Item *item = value;
        GFile *parent = g_file_get_parent(item->file);
        if (parent) g_hash_table_add(parents, parent);
    }
    g_hash_table_iter_init(&iter, s->monitors);
    while (g_hash_table_iter_next(&iter, &key, NULL))
        if (!g_hash_table_contains(parents, key)) g_hash_table_iter_remove(&iter);
}
static void verified(GObject *source, GAsyncResult *result, gpointer data)
{
    (void)source;
    Completion *completion = data;
    Sync *s = completion->sync;
    GTask *task = G_TASK(result);
    Job *job = g_task_get_task_data(task);
    g_autoptr(GError) error = NULL;
    gboolean success = g_task_propagate_boolean(task, &error);
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&s->view);
    if (job->generation == s->generation) s->running = FALSE;
    if (success && view && !s->closed && job->generation == s->generation) {
        GtkTreeModel *model = gtk_tree_view_get_model(view);
        gboolean cleanup = FALSE;
        for (guint n = 0; n < completion->items->len; n++) {
            Item *item = g_ptr_array_index(completion->items, n);
            Check *check = g_ptr_array_index(job->checks, n);
            GtkTreeIter iter;
            if (g_hash_table_lookup(s->items, item->file) != item || item->dragging ||
                item->revision != check->revision || g_get_monotonic_time() >= item->expires ||
                !item_iter(model, item, &iter)) continue;
            g_autoptr(GFile) current = NULL;
            gtk_tree_model_get(model, &iter, CF_COL_FILE, &current, -1);
            if (!current || !g_file_equal(current, check->original)) continue;
            if (check->outcome == REMOVE) {
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                g_hash_table_remove(s->items, item->file);
                cleanup = TRUE;
            } else if (check->outcome == UPDATE) {
                CfResult refreshed = { check->result_file, check->info };
                if (!g_file_equal(current, refreshed.file)) {
                    cleanup = TRUE;
                    /* A correlated rename may overwrite another existing result. */
                    cf_results_remove(GTK_LIST_STORE(model), refreshed.file);
                    if (!item_iter(model, item, &iter)) continue;
                }
                cf_results_update(GTK_LIST_STORE(model), &iter, &refreshed);
                rekey(s, item, refreshed.file);
                g_clear_object(&item->destination);
                g_free(item->identity);
                item->identity = g_strdup(g_file_info_get_attribute_string(check->info, G_FILE_ATTRIBUTE_ID_FILE));
                watch_parent(s, item->file);
            }
        }
        if (cleanup) prune_monitors(s);
    }
    g_ptr_array_unref(completion->items); /* GTK row references are released only on GTK's thread. */
    g_free(completion);
    schedule(s);
    sync_unref(s);
}
static gboolean tick(gpointer data)
{
    Sync *s = data;
    s->timer = 0;
    gint64 now = g_get_monotonic_time();
    g_autoptr(GtkTreeView) view = g_weak_ref_get(&s->view);
    if (!view || s->closed) return G_SOURCE_REMOVE;
    Job *job = g_new0(Job, 1);
    job->checks = g_ptr_array_new_with_free_func(check_free);
    Completion *completion = g_new0(Completion, 1);
    completion->items = g_ptr_array_new_with_free_func(item_unref);
    gboolean cleanup = FALSE;
    GHashTableIter iter; gpointer value;
    g_hash_table_iter_init(&iter, s->folders);
    while (g_hash_table_iter_next(&iter, NULL, &value))
        if (now >= ((Item *)value)->expires) g_hash_table_iter_remove(&iter);
    g_hash_table_iter_init(&iter, s->items);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        Item *item = value;
        if (now >= item->expires || !gtk_tree_row_reference_valid(item->row)) {
            g_hash_table_iter_remove(&iter);
            cleanup = TRUE;
            continue;
        }
        if (s->running || item->dragging || (!item->dirty && now < item->due) || job->checks->len >= CHECK_LIMIT) continue;
        Check *check = g_new0(Check, 1);
        check->original = g_object_ref(item->file);
        check->destination = item->destination ? g_object_ref(item->destination) : NULL;
        check->identity = g_strdup(item->identity);
        check->revision = item->revision;
        g_ptr_array_add(job->checks, check);
        g_ptr_array_add(completion->items, item_ref(item));
        item->dirty = FALSE;
        while (item->retry < G_N_ELEMENTS(retries_ms) &&
               item->started + (gint64)retries_ms[item->retry] * 1000 <= now) item->retry++;
        item->due = item->retry < G_N_ELEMENTS(retries_ms) ?
                    item->started + (gint64)retries_ms[item->retry] * 1000 : item->expires;
    }
    if (cleanup) prune_monitors(s);
    if (job->checks->len) {
        job->root = s->root ? g_object_ref(s->root) : NULL;
        job->criteria = cf_criteria_copy(s->criteria);
        job->generation = s->generation;
        completion->sync = sync_ref(s);
        s->running = TRUE;
        g_autoptr(GTask) task = g_task_new(NULL, s->cancel, verified, completion);
        g_task_set_task_data(task, job, job_free);
        g_task_run_in_thread(task, verify_worker);
    } else {
        job_free(job);
        g_ptr_array_unref(completion->items);
        g_free(completion);
    }
    schedule(s);
    return G_SOURCE_REMOVE;
}
static void schedule(Sync *s)
{
    if (s->timer) { g_source_remove(s->timer); s->timer = 0; }
    if (s->closed || (!g_hash_table_size(s->items) && !g_hash_table_size(s->folders))) return;
    gint64 now = g_get_monotonic_time(), next = G_MAXINT64;
    GHashTableIter iter; gpointer value;
    g_hash_table_iter_init(&iter, s->items);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        Item *item = value;
        next = MIN(next, item->expires);
        if (!s->running && !item->dragging) next = MIN(next, item->dirty ? now + 150000 : item->due);
    }
    g_hash_table_iter_init(&iter, s->folders);
    while (g_hash_table_iter_next(&iter, NULL, &value)) next = MIN(next, ((Item *)value)->expires);
    guint delay = (guint)MAX(1, (next - now + 999) / 1000);
    s->scheduled_at = now + (gint64)delay * 1000;
    s->timer = g_timeout_add_full(G_PRIORITY_DEFAULT, delay, tick, sync_ref(s), sync_unref);
}
static void reset(Sync *s)
{
    s->generation++;
    g_cancellable_cancel(s->cancel);
    g_clear_object(&s->cancel);
    s->cancel = g_cancellable_new();
    if (s->timer) { g_source_remove(s->timer); s->timer = 0; }
    g_hash_table_remove_all(s->monitors);
    g_hash_table_remove_all(s->items);
    g_hash_table_remove_all(s->folders);
    s->running = FALSE;
}
static void destroyed(GtkWidget *view, gpointer data)
{
    (void)view;
    Sync *s = data;
    s->closed = TRUE;
    g_signal_handlers_disconnect_by_data(s->model, s);
    reset(s);
}
void cf_results_sync_attach(GtkTreeView *view)
{
    Sync *s = g_new0(Sync, 1);
    s->refs = 1;
    g_weak_ref_init(&s->view, view);
    s->cancel = g_cancellable_new();
    s->model = g_object_ref(gtk_tree_view_get_model(view));
    s->items = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, item_unref);
    s->folders = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, item_unref);
    s->monitors = g_hash_table_new_full(g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, monitor_free);
    g_object_set_data_full(G_OBJECT(view), KEY, s, sync_unref);
    g_signal_connect(view, "destroy", G_CALLBACK(destroyed), s);
    g_signal_connect(s->model, "row-inserted", G_CALLBACK(row_published), s);
}
void cf_results_sync_set_search(GtkTreeView *view, GFile *root, CfCriteria *criteria)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    reset(s);
    g_set_object(&s->root, root);
    cf_criteria_free(s->criteria);
    s->criteria = criteria;
}
guint cf_results_sync_generation(GtkTreeView *view)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    return s->generation;
}
void cf_results_sync_drag_begin(GtkTreeView *view, GPtrArray *files)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    if (s->closed) return;
    /* Build the path index once, not a full model scan per dragged file. */
    g_autoptr(GHashTable) selected = g_hash_table_new(g_file_hash, (GEqualFunc)g_file_equal);
    for (guint n = 0; n < files->len; n++) g_hash_table_add(selected, g_ptr_array_index(files, n));
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    for (gboolean valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
        g_autoptr(GFile) file = NULL;
        gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, -1);
        if (!file) continue;
        g_autoptr(GFile) origin = g_object_ref(file);
        while (origin && !g_hash_table_contains(selected, origin)) {
            GFile *parent = g_file_get_parent(origin);
            g_object_unref(origin);
            origin = parent;
        }
        if (!origin) continue;
        Item *item = g_hash_table_lookup(s->items, file);
        if (!item) {
            item = g_new0(Item, 1);
            item->refs = 1;
            item->file = g_object_ref(file);
            g_autofree char *type = NULL;
            gtk_tree_model_get(model, &iter, CF_COL_ID, &item->identity, CF_COL_TYPE, &type, -1);
            item->directory = !g_strcmp0(type, "Folder");
            g_autoptr(GtkTreePath) path = gtk_tree_model_get_path(model, &iter);
            item->row = gtk_tree_row_reference_new(model, path);
            g_hash_table_insert(s->items, g_object_ref(file), item);
            if (item->directory) g_hash_table_replace(s->folders, g_object_ref(file), item_ref(item));
        }
        g_set_object(&item->origin, origin);
        item->dragging = TRUE;
        item->revision++;
        item->expires = g_get_monotonic_time() + WINDOW_US;
        watch_parent(s, file);
    }
    schedule(s);
}
void cf_results_sync_drag_end(GtkTreeView *view, GPtrArray *files)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    if (s->closed) return;
    g_autoptr(GHashTable) selected = g_hash_table_new(g_file_hash, (GEqualFunc)g_file_equal);
    for (guint n = 0; n < files->len; n++) g_hash_table_add(selected, g_ptr_array_index(files, n));
    GHashTableIter iter; gpointer value;
    g_hash_table_iter_init(&iter, s->items);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        Item *item = value;
        if (!item->origin || !g_hash_table_contains(selected, item->origin)) continue;
        item->dragging = FALSE;
        item->dirty = TRUE;
        item->started = item->due = g_get_monotonic_time();
        item->expires = item->started + WINDOW_US;
        item->retry = 0;
    }
    /* Folder hints also cover rows still queued by an active search when the folder moves. */
    g_hash_table_iter_init(&iter, s->folders);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        Item *item = value;
        if (!item->origin || !g_hash_table_contains(selected, item->origin)) continue;
        item->dragging = FALSE;
        item->started = g_get_monotonic_time();
        item->expires = item->started + WINDOW_US;
    }
    schedule(s);
}
static void row_published(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    Sync *s = data;
    if (s->closed || !g_hash_table_size(s->folders)) return;
    g_autoptr(GFile) file = NULL;
    gtk_tree_model_get(model, iter, CF_COL_FILE, &file, -1);
    if (!file || g_hash_table_contains(s->items, file)) return;
    g_autoptr(GFile) ancestor = g_file_get_parent(file);
    Item *folder = NULL;
    while (ancestor && !(folder = g_hash_table_lookup(s->folders, ancestor))) {
        GFile *parent = g_file_get_parent(ancestor);
        g_object_unref(ancestor); ancestor = parent;
    }
    if (!folder || g_get_monotonic_time() >= folder->expires) return;
    Item *item = g_new0(Item, 1);
    item->refs = 1;
    item->file = g_object_ref(file);
    item->origin = g_object_ref(folder->origin);
    item->row = gtk_tree_row_reference_new(model, path);
    g_autofree char *type = NULL;
    gtk_tree_model_get(model, iter, CF_COL_ID, &item->identity, CF_COL_TYPE, &type, -1);
    item->directory = !g_strcmp0(type, "Folder");
    item->started = folder->started;
    item->due = g_get_monotonic_time();
    item->expires = folder->expires;
    item->dragging = folder->dragging;
    item->dirty = TRUE;
    GFile *target = folder->destination ? folder->destination :
                    !g_file_equal(folder->file, ancestor) ? folder->file : NULL;
    if (target) {
        g_autofree char *relative = g_file_get_relative_path(ancestor, file);
        item->destination = g_file_resolve_relative_path(target, relative);
    }
    g_hash_table_insert(s->items, g_object_ref(file), item);
    if (item->directory) g_hash_table_replace(s->folders, g_object_ref(file), item_ref(item));
    watch_parent(s, file);
    schedule(s);
}
static gboolean descendant(gpointer key, gpointer value, gpointer previous)
{
    (void)value;
    return g_file_has_prefix(G_FILE(key), G_FILE(previous));
}
void cf_results_sync_invalidate_descendants(GtkTreeView *view, GFile *previous)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    g_hash_table_foreach_remove(s->items, descendant, previous);
    g_hash_table_foreach_remove(s->folders, descendant, previous);
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        g_autoptr(GFile) file = NULL;
        gtk_tree_model_get(model, &iter, CF_COL_FILE, &file, -1);
        if (file && g_file_has_prefix(file, previous)) valid = gtk_list_store_remove(store, &iter);
        else valid = gtk_tree_model_iter_next(model, &iter);
    }
}
void cf_results_sync_renamed(GtkTreeView *view, GFile *previous, const CfResult *result)
{
    Sync *s = g_object_get_data(G_OBJECT(view), KEY);
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    if (s->closed) return;
    if (!complete_info(s->criteria, result->info) ||
        (s->criteria && (!cf_criteria_matches(s->criteria, result->info) || cf_criteria_excludes(s->criteria, result->info)))) {
        cf_results_remove(store, previous);
        return;
    }
    cf_results_replace(store, previous, result);
    Item *item = g_hash_table_lookup(s->items, previous);
    if (item) {
        item->revision++;
        item->dirty = TRUE;
        g_clear_object(&item->destination);
        rekey(s, item, result->file);
        watch_parent(s, item->file);
        schedule(s);
    }
}
