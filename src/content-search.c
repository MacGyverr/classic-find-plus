#define _GNU_SOURCE
#include "content-search.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/vfs.h>

struct _CfContentPattern {
    gint refs;
    guint8 *bytes;
    gsize length, *failure;
    gboolean fold;
};

static gboolean invalid(GError **error, const char *message)
{
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, message);
    return FALSE;
}
CfContentPattern *cf_content_pattern_new(const char *text, CfContentMode mode,
                                         gboolean sensitive, GError **error)
{
    if ((guint)mode > CF_CONTENT_HEX) {
        invalid(error, "Invalid content matching mode."); return NULL;
    }
    if (!text || !*text) return NULL; /* inactive, including empty Hex */
    gsize raw = strnlen(text, CF_CONTENT_MAX_INPUT + 1);
    if (raw > CF_CONTENT_MAX_INPUT || !g_utf8_validate(text, raw, NULL)) {
        invalid(error, "Content input must be valid UTF-8, at most 256 KiB; pattern limit is 64 KiB."); return NULL;
    }
    g_autoptr(GByteArray) bytes = g_byte_array_new();
    if (mode == CF_CONTENT_HEX) {
        int high = -1;
        for (gsize i = 0; i < raw; i++) {
            if (text[i] == ' ' || text[i] == '\t') {
                if (high < 0) continue;
                invalid(error, "Hex spaces/tabs must separate complete byte pairs."); return NULL;
            }
            int digit = g_ascii_xdigit_value(text[i]);
            if (digit < 0) { invalid(error, "Hex requires hexadecimal byte pairs, without 0x prefixes."); return NULL; }
            if (high < 0) high = digit;
            else {
                guint8 byte = (high << 4) | digit;
                g_byte_array_append(bytes, &byte, 1); high = -1;
                if (bytes->len > CF_CONTENT_MAX_PATTERN) goto too_long;
            }
        }
        if (high >= 0 || !bytes->len) {
            invalid(error, "Hex requires a nonempty, even number of hexadecimal digits."); return NULL;
        }
    } else {
        if (raw > CF_CONTENT_MAX_PATTERN) goto too_long;
        g_autofree char *folded = sensitive ? NULL : g_utf8_casefold(text, raw);
        const char *value = folded ? folded : text;
        gsize length = strlen(value);
        if (length > CF_CONTENT_MAX_PATTERN) goto too_long;
        g_byte_array_append(bytes, (const guint8 *)value, length);
    }
    CfContentPattern *p = g_new0(CfContentPattern, 1);
    p->refs = 1; p->length = bytes->len;
    p->bytes = g_byte_array_free(g_steal_pointer(&bytes), FALSE);
    p->fold = mode == CF_CONTENT_TEXT && !sensitive;
    p->failure = g_new0(gsize, p->length);
    for (gsize i = 1, j = 0; i < p->length; i++) {
        while (j && p->bytes[i] != p->bytes[j]) j = p->failure[j-1];
        if (p->bytes[i] == p->bytes[j]) j++;
        p->failure[i] = j;
    }
    return p;
too_long:
    invalid(error, "Content pattern exceeds 64 KiB (decoded Hex or UTF-8 text, including case-folded text).");
    return NULL;
}
CfContentPattern *cf_content_pattern_ref(CfContentPattern *p)
{
    if (p) g_atomic_int_inc(&p->refs);
    return p;
}
void cf_content_pattern_unref(CfContentPattern *p)
{
    if (!p || !g_atomic_int_dec_and_test(&p->refs)) return;
    g_free(p->bytes); g_free(p->failure); g_free(p);
}
static gboolean feed(CfContentPattern *p, gsize *state, const guint8 *bytes, gsize len, GCancellable *cancel)
{
    for (gsize i = 0; i < len; i++) {
        if (!(i & 4095) && g_cancellable_is_cancelled(cancel)) return FALSE;
        while (*state && bytes[i] != p->bytes[*state]) *state = p->failure[*state-1];
        if (bytes[i] == p->bytes[*state]) (*state)++;
        if (*state == p->length) return TRUE;
    }
    return FALSE;
}
static gboolean folded_span(CfContentPattern *p, gsize *state, const guint8 *s, gsize len, GCancellable *cancel)
{
    if (!len) return FALSE;
    g_autofree char *fold = g_utf8_casefold((const char *)s, len);
    return feed(p, state, (const guint8 *)fold, strlen(fold), cancel);
}
gboolean cf_content_scan(CfContentPattern *p, CfContentRead reader, gpointer data,
                         GCancellable *cancel, GError **error)
{
    if (!p) return TRUE;
    guint8 buffer[CF_CONTENT_BUFFER + 4];
    gsize carry = 0, state = 0;
    for (;;) {
        if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
        gssize count = reader(data, buffer + carry, CF_CONTENT_BUFFER);
        if (count < 0) {
            if (errno == EINTR) continue;
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno), "Content read failed: %s", g_strerror(errno));
            return FALSE;
        }
        if (g_cancellable_set_error_if_cancelled(cancel, error)) return FALSE;
        if (!count) return FALSE; /* incomplete UTF-8 suffix is a barrier */
        gsize length = carry + (gsize)count;
        if (!p->fold) {
            if (feed(p, &state, buffer, length, cancel)) return TRUE;
            continue;
        }
        /* Fold only valid spans. NUL and malformed bytes delimit spans; malformed
         * bytes reset KMP so a text match cannot bridge corrupt data. Carry at
         * most three bytes of an incomplete character into the next read. */
        gsize start = 0, i = 0;
        carry = 0;
        while (i < length) {
            if (g_cancellable_set_error_if_cancelled(cancel,error)) return FALSE;
            gunichar c = g_utf8_get_char_validated((const char *)buffer+i, length-i);
            if (buffer[i] == 0 || c == (gunichar)-1 || c == (gunichar)-2) {
                if (folded_span(p, &state, buffer+start, i-start, cancel)) return TRUE;
                /* GLib also returns -2 for a NUL inside a sequence, even
                 * with many bytes remaining. Only a short, NUL-free suffix
                 * may be carried; all other malformed sequences are barriers. */
                if (c == (gunichar)-2 && length-i <= 3 &&
                    !memchr(buffer+i,0,length-i)) {
                    carry = length-i; memmove(buffer, buffer+i, carry); break;
                }
                state = 0; i++; start = i;
            } else i += g_utf8_skip[buffer[i]];
        }
        if (!carry && folded_span(p, &state, buffer+start, i-start, cancel)) return TRUE;
    }
}

gboolean cf_content_regular_mode(mode_t mode) { return S_ISREG(mode); }
gboolean cf_content_safe_filesystem(long magic)
{
    /* Linux UAPI magic values; also exclude cgroup, BPF, efivar, pstore and
     * namespace/control interfaces. tmpfs, network and FUSE remain eligible. */
    static const long unsafe[] = {0x9fa0, 0x62656572, 0x64626720, 0x74726163,
        0x73636673, 0x62656570, 0x27e0eb, 0x63677270, 0xcafe4a11,
        0xde5e81e4, 0x6165676c, 0x6e736673, 0x19800202, 0x50495045,
        0x534f434b, 0x1cd1, 0xf97cff8c, 0x43415d53, 0x5a3c69f0,
        0x7655821, 0xabba1974, 0x9fa1, 0x9fa2, 0x11307854, 0x09041934,
        0x62646576, 0x64646178, 0x42494e4d, 0x6c6f6f70, 0x444d4142, 0x454d444d,
        0x5345434d, 0x50494446};
    for (guint i=0; i<G_N_ELEMENTS(unsafe); i++) if (magic == unsafe[i]) return FALSE;
    return TRUE;
}
static gssize read_fd(gpointer data, void *buffer, gsize count)
{
    return read(GPOINTER_TO_INT(data), buffer, count);
}
static gboolean io_error(GError **error, const char *action)
{
    int saved = errno;
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved), "%s: %s", action, g_strerror(saved));
    return FALSE;
}
gboolean cf_content_match_checked(CfContentPattern *p, GFile *file, gboolean follow,
    GCancellable *cancel, CfContentCheck check, gpointer data, GError **error)
{
    if (!p) return TRUE;
    if (g_cancellable_set_error_if_cancelled(cancel,error)) return FALSE;
    g_autofree char *path = g_file_get_path(file);
    if (!path) return invalid(error,"Content search requires a native filesystem path (mounted network/FUSE paths are supported).");
    struct stat before, pinned, actual, after;
    struct statfs fs;
    if (lstat(path,&before)) return io_error(error,"Cannot inspect content candidate");
    if (!S_ISREG(before.st_mode) && !(follow && S_ISLNK(before.st_mode))) return FALSE;
    /* O_PATH does not activate device drivers or open FIFO/socket data streams.
     * Pin first, then inspect the actual inode and filesystem before data open. */
    int pin = open(path,O_PATH|O_CLOEXEC|(follow ? 0 : O_NOFOLLOW));
    if (pin < 0) return io_error(error,"Cannot pin content candidate");
    gboolean result = FALSE;
    int fd = -1;
    if (fstat(pin,&pinned) || fstatfs(pin,&fs)) { io_error(error,"Cannot verify content candidate"); goto out; }
    if (!S_ISREG(pinned.st_mode) || !cf_content_safe_filesystem(fs.f_type)) goto out;
    if (S_ISREG(before.st_mode) && (before.st_dev != pinned.st_dev || before.st_ino != pinned.st_ino)) {
        invalid(error,"Content candidate changed before opening; skipped."); goto out;
    }
    if (g_cancellable_set_error_if_cancelled(cancel,error)) goto out;
    char descriptor[64];
    g_snprintf(descriptor,sizeof descriptor,"/proc/self/fd/%d",pin);
    if (check) {
        char resolved[4096];
        ssize_t length=readlink(descriptor,resolved,sizeof resolved-1);
        if (length<0) { io_error(error,"Cannot resolve pinned content candidate"); goto out; }
        if ((gsize)length>=sizeof resolved-1 || !length || resolved[0]!='/') {
            invalid(error,"Cannot safely resolve pinned content path; skipped."); goto out;
        }
        resolved[length]=0;
        g_autoptr(GFile) target=g_file_new_for_path(resolved);
        if (!check(target,(guint64)pinned.st_dev,data,error)) goto out;
    }
    /* Reopen the pinned inode, never the original mutable pathname. O_NONBLOCK
     * is extra defense; it does not guarantee interruptible regular-file I/O. */
    fd = open(descriptor,O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if (fd < 0) { io_error(error,"Cannot open regular file contents"); goto out; }
    if (fstat(fd,&actual) || fstatfs(fd,&fs)) { io_error(error,"Cannot verify opened contents"); goto out; }
    if (!S_ISREG(actual.st_mode) || actual.st_dev != pinned.st_dev || actual.st_ino != pinned.st_ino ||
        !cf_content_safe_filesystem(fs.f_type)) { invalid(error,"Opened content identity changed; skipped."); goto out; }
    result = cf_content_scan(p,read_fd,GINT_TO_POINTER(fd),cancel,error);
    if (fstat(fd,&after)) { result = io_error(error,"Cannot recheck content file"); goto out; }
    if (!after.st_nlink || actual.st_size != after.st_size ||
        actual.st_mtim.tv_sec != after.st_mtim.tv_sec || actual.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        actual.st_ctim.tv_sec != after.st_ctim.tv_sec || actual.st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
        result = FALSE;
        if (!error || !*error) invalid(error,"File changed during content search; skipped.");
    }
out:
    if (fd >= 0) close(fd);
    close(pin);
    return result;
}

gboolean cf_content_match_file(CfContentPattern *p, GFile *file, gboolean follow,
                               GCancellable *cancel, GError **error)
{
    return cf_content_match_checked(p,file,follow,cancel,NULL,NULL,error);
}
