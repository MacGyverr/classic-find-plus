#include "metadata.h"
#include "criteria-private.h"
#include "admin-protocol.h"
#include "traversal.h"

#define QUEUE_LIMIT 256
#define ATTRIBUTES "standard::name,standard::display-name,standard::type,standard::is-symlink,standard::size,standard::icon,standard::content-type,time::created,time::created-usec,time::access,time::access-usec,unix::mode,unix::uid,unix::gid,time::modified,time::modified-usec,id::filesystem,id::file"

struct _CfSearch {
    gint refs;
    GMutex mutex;
    GCond space;
    GQueue results;
    GFile *root;
    CfCriteria *criteria;
    GCancellable *cancel;
    guint64 scanned, matched, errors, directories_enumerated, directories_pruned;
    char *first_error;
    gboolean finished;
    gboolean admin;
};

typedef struct {
    GFile *directory;
    GFileEnumerator *enumerator;
} Frame;

static void frame_free(Frame *frame)
{
    /* The enumerator owns the backend resources; destruction closes it. */
    g_object_unref(frame->enumerator);
    g_object_unref(frame->directory);
    g_free(frame);
}

void cf_result_free(CfResult *r)
{
    if (!r) return;
    g_object_unref(r->file);
    g_object_unref(r->info);
    g_free(r);
}

void cf_search_unref(CfSearch *s)
{
    if (!s || !g_atomic_int_dec_and_test(&s->refs)) return;
    g_queue_clear_full(&s->results, (GDestroyNotify)cf_result_free);
    g_clear_object(&s->root);
    g_clear_object(&s->cancel);
    cf_criteria_free(s->criteria);
    g_free(s->first_error);
    g_cond_clear(&s->space);
    g_mutex_clear(&s->mutex);
    g_free(s);
}

void cf_search_cancel(CfSearch *s)
{
    g_cancellable_cancel(s->cancel);
    g_mutex_lock(&s->mutex);
    g_cond_broadcast(&s->space);
    g_mutex_unlock(&s->mutex);
}

static void record_error(CfSearch *s, GFile *file, const char *message)
{
    if (g_cancellable_is_cancelled(s->cancel)) return;
    g_autofree char *path = g_file_get_parse_name(file);
    g_mutex_lock(&s->mutex);
    s->errors++;
    if (!s->first_error)
        s->first_error = g_strdup_printf("%s: %s", path, message);
    g_mutex_unlock(&s->mutex);
}

static gboolean push_directory(CfSearch *s, GPtrArray *stack, GFile *dir)
{
    g_autoptr(GError) error = NULL;
    g_mutex_lock(&s->mutex);
    s->directories_enumerated++;
    g_mutex_unlock(&s->mutex);
    GFileEnumerator *enumerator = g_file_enumerate_children(dir, g_file_is_native(dir) ? ATTRIBUTES : ATTRIBUTES ",owner::user,owner::group",
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, s->cancel, &error);
    if (!enumerator) {
        record_error(s, dir, error->message);
        return FALSE;
    }
    Frame *frame = g_new0(Frame, 1);
    frame->directory = g_object_ref(dir);
    frame->enumerator = enumerator;
    g_ptr_array_add(stack, frame);
    return TRUE;
}

static void publish(CfSearch *s, GFile *file, GFileInfo *info)
{
    g_mutex_lock(&s->mutex);
    while (s->results.length >= QUEUE_LIMIT && !g_cancellable_is_cancelled(s->cancel))
        g_cond_wait(&s->space, &s->mutex);
    if (!g_cancellable_is_cancelled(s->cancel)) {
        CfResult *result = g_new0(CfResult, 1);
        result->file = g_object_ref(file);
        result->info = g_object_ref(info);
        g_queue_push_tail(&s->results, result);
        s->matched++;
    }
    g_mutex_unlock(&s->mutex);
}

static void remote_result(CfResult *r, gpointer data)
{
    publish(data, r->file, r->info);
    cf_result_free(r);
}
static void remote_stats(const CfSearchStats *stats, gpointer data)
{
    CfSearch *s = data;
    g_mutex_lock(&s->mutex);
    s->scanned = stats->scanned; s->errors = stats->errors;
    s->directories_enumerated = stats->directories_enumerated;
    s->directories_pruned = stats->directories_pruned;
    if (!s->first_error) s->first_error = g_strdup(stats->first_error);
    g_mutex_unlock(&s->mutex);
}

static gint compare_uri(gconstpointer a,gconstpointer b,gpointer unused)
{
    (void)unused;return strcmp(a,b);
}

static void worker(gpointer data, gpointer unused)
{
    (void) unused;
    CfSearch *s = data;
    if (s->admin) {
        g_autoptr(GError) remote_error = NULL;
        cf_admin_client_run(s->root, s->criteria, s->cancel, remote_result, remote_stats, s, &remote_error);
        if (remote_error) record_error(s, s->root, remote_error->message);
        g_mutex_lock(&s->mutex); s->finished = TRUE; g_mutex_unlock(&s->mutex);
        cf_search_unref(s);
        return;
    }
    g_autoptr(CfMetadataCache) metadata = cf_metadata_cache_new();
    g_autoptr(GPtrArray) stack = g_ptr_array_new_with_free_func((GDestroyNotify)frame_free);
    g_autoptr(GHashTable) visited = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
    g_autoptr(GTree) pending = g_tree_new_full(compare_uri,NULL,g_free,g_object_unref);
    g_autoptr(GError) error = NULL;
    g_autoptr(GFileInfo) root_info = g_file_query_info(s->root,
        "standard::type,id::filesystem,id::file", G_FILE_QUERY_INFO_NONE, s->cancel, &error);
    if (!root_info) {
        record_error(s, s->root, error->message);
        goto done;
    }
    if (g_file_info_get_file_type(root_info) != G_FILE_TYPE_DIRECTORY) {
        record_error(s, s->root, "Search location must be a directory.");
        goto done;
    }
    const char *filesystem = g_file_info_get_attribute_string(root_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    if (s->criteria->recursive && s->criteria->stay_on_filesystem && !filesystem) {
        record_error(s, s->root, "Backend cannot identify filesystems; disable Stay on this filesystem to search here.");
        goto done;
    }
    if (s->criteria->follow_symlinks) {
        g_autofree char *id=cf_traversal_identity(root_info);
        if (!id || !g_file_is_native(s->root)) {
            record_error(s,s->root,"Safe symbolic-link traversal requires native paths and stable directory identities.");
            goto done;
        }
        g_hash_table_add(visited,g_steal_pointer(&id));
    }
    push_directory(s, stack, s->root);
    while ((stack->len || g_tree_nnodes(pending)) && !g_cancellable_is_cancelled(s->cancel)) {
        if (!stack->len) {
            GTreeNode *node=g_tree_node_first(pending);
            g_autoptr(GFile) dir=g_object_ref(g_tree_node_value(node));
            g_tree_remove(pending,g_tree_node_key(node));
            g_clear_error(&error);
            g_autoptr(GFileInfo) target=g_file_query_info(dir,"standard::type,id::filesystem,id::file",
                G_FILE_QUERY_INFO_NONE,s->cancel,&error);
            if (!target) { record_error(s,dir,error->message); continue; }
            if (g_file_info_get_file_type(target)!=G_FILE_TYPE_DIRECTORY) continue;
            const char *target_fs=g_file_info_get_attribute_string(target,G_FILE_ATTRIBUTE_ID_FILESYSTEM);
            if (!target_fs) { record_error(s,dir,"Cannot identify target filesystem; directory skipped.");continue; }
            if (s->criteria->stay_on_filesystem && g_strcmp0(filesystem,target_fs)) continue;
            g_autofree char *id=cf_traversal_identity(target);
            if (!id) { record_error(s,dir,"Cannot identify symbolic-link target; directory skipped."); continue; }
            if (g_hash_table_contains(visited,id)) continue;
            if (g_hash_table_size(visited)>=1000000) {
                record_error(s,dir,"Directory identity limit reached (1000000); narrow the search root."); break;
            }
            if (push_directory(s,stack,dir)) g_hash_table_add(visited,g_steal_pointer(&id));
            continue;
        }
        Frame *frame = g_ptr_array_index(stack, stack->len - 1);
        g_clear_error(&error);
        g_autoptr(GFileInfo) info = g_file_enumerator_next_file(frame->enumerator, s->cancel, &error);
        if (!info) {
            if (error) record_error(s, frame->directory, error->message);
            g_ptr_array_remove_index(stack, stack->len - 1);
            continue;
        }
        const char *name = g_file_info_get_name(info);
        if (!name) continue;
        g_autoptr(GFile) child = g_file_get_child(frame->directory, name);
        g_mutex_lock(&s->mutex);
        s->scanned++;
        g_mutex_unlock(&s->mutex);
        if (cf_criteria_excludes(s->criteria, info)) {
            if (s->criteria->recursive && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
                g_mutex_lock(&s->mutex); s->directories_pruned++; g_mutex_unlock(&s->mutex);
            }
            continue; /* No enumeration, filesystem-ID query or metadata preparation. */
        }
        if (cf_criteria_matches(s->criteria, info)) {
            if (g_file_is_native(child)) cf_metadata_prepare(metadata, info);
            publish(s, child, info);
        }
        if (s->criteria->recursive && s->criteria->follow_symlinks) {
            gboolean link=g_file_info_get_is_symlink(info);
            if (!link && g_file_info_get_file_type(info)!=G_FILE_TYPE_DIRECTORY) continue;
            if (link) {
                g_clear_error(&error);
                g_autoptr(GFileInfo) target=g_file_query_info(child,"standard::type",G_FILE_QUERY_INFO_NONE,s->cancel,&error);
                if (!target) { record_error(s,child,error->message); continue; }
                if (g_file_info_get_file_type(target)!=G_FILE_TYPE_DIRECTORY) continue;
                g_autoptr(GFileInfo) lexical=g_file_info_dup(info);
                g_file_info_set_file_type(lexical,G_FILE_TYPE_DIRECTORY);
                gboolean allowed=!cf_criteria_excludes(s->criteria,lexical) &&
                    cf_traversal_target_allowed(s->criteria,s->root,child,s->cancel,&error);
                if (!allowed) {
                    if (error) record_error(s,child,error->message);
                    else { g_mutex_lock(&s->mutex);s->directories_pruned++;g_mutex_unlock(&s->mutex); }
                    continue;
                }
            }
            if (g_tree_nnodes(pending)>=1000000) {
                record_error(s,child,"Pending directory limit reached (1000000); narrow the search root.");break;
            }
            /* Directory visits are ordered by bytewise URI, independent of backend
             * enumeration order. The first eligible alias owns that target's children. */
            g_tree_replace(pending,g_file_get_uri(child),g_object_ref(child));
            continue;
        }
        if (!s->criteria->recursive || g_file_info_get_is_symlink(info) ||
            g_file_info_get_file_type(info) != G_FILE_TYPE_DIRECTORY) continue;
        if (s->criteria->stay_on_filesystem) {
            const char *child_fs = g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
            /* Some backends do not supply filesystem IDs during enumeration. */
            g_autoptr(GFileInfo) fs_info = NULL;
            if (!child_fs) {
                g_clear_error(&error);
                fs_info = g_file_query_info(child, "id::filesystem", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, s->cancel, &error);
                if (fs_info) child_fs = g_file_info_get_attribute_string(fs_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
            }
            if (!child_fs) {
                record_error(s, child, error ? error->message : "Cannot identify filesystem; directory skipped.");
                continue;
            }
            if (g_strcmp0(filesystem, child_fs)) continue;
        }
        push_directory(s, stack, child);
    }
done:
    /* Close directory resources before marking the job complete. */
    g_ptr_array_set_size(stack, 0);
    g_mutex_lock(&s->mutex);
    s->finished = TRUE;
    g_mutex_unlock(&s->mutex);
    cf_search_unref(s);
}

static CfSearch *start(GFile *root, CfCriteria *criteria, gboolean admin, GError **error)
{
    static GThreadPool *pool;
    static gsize initialized;
    static char *pool_failure;
    if (g_once_init_enter(&initialized)) {
        g_autoptr(GError) creation_error = NULL;
        pool = g_thread_pool_new(worker, NULL, 2, TRUE, &creation_error);
        if (creation_error) {
            pool_failure = g_strdup(creation_error->message);
            if (pool) g_thread_pool_free(pool, TRUE, TRUE);
            pool = NULL;
        }
        g_once_init_leave(&initialized, 1);
    }
    if (!pool) {
        cf_criteria_free(criteria);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Cannot create search worker pool: %s",
                    pool_failure ? pool_failure : "unknown error");
        return NULL;
    }
    CfSearch *s = g_new0(CfSearch, 1);
    s->refs = 1;
    s->admin = admin;
    g_mutex_init(&s->mutex);
    g_cond_init(&s->space);
    g_queue_init(&s->results);
    s->root = g_object_ref(root);
    s->criteria = criteria;
    s->cancel = g_cancellable_new();
    g_atomic_int_inc(&s->refs);
    if (!g_thread_pool_push(pool, s, error)) {
        cf_search_unref(s);
        cf_search_unref(s);
        return NULL;
    }
    return s;
}

CfSearch *cf_search_start(GFile *root, CfCriteria *criteria, GError **error)
{
    return start(root, criteria, FALSE, error);
}
CfSearch *cf_search_start_admin(GFile *root, CfCriteria *criteria, GError **error)
{
    return start(root, criteria, TRUE, error);
}

CfResult *cf_search_pop(CfSearch *s)
{
    g_mutex_lock(&s->mutex);
    CfResult *result = g_queue_pop_head(&s->results);
    if (result) g_cond_signal(&s->space);
    g_mutex_unlock(&s->mutex);
    return result;
}

void cf_search_get_stats(CfSearch *s, CfSearchStats *stats)
{
    g_mutex_lock(&s->mutex);
    *stats = (CfSearchStats) {
        .scanned = s->scanned, .matched = s->matched, .errors = s->errors,
        .directories_enumerated = s->directories_enumerated, .directories_pruned = s->directories_pruned,
        .queued = s->results.length, .finished = s->finished && g_queue_is_empty(&s->results),
        .cancelled = g_cancellable_is_cancelled(s->cancel), .first_error = g_strdup(s->first_error)
    };
    g_mutex_unlock(&s->mutex);
}
