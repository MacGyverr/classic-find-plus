#define _GNU_SOURCE
#include "read-only.h"
#include <gio/gio.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

#define DENY(call) BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_##call, 0, 1), BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM)
#define WRITE_FLAGS (O_WRONLY|O_RDWR|O_CREAT|O_TRUNC|O_APPEND)
/* Not a general sandbox API: installed once, process-wide, before traversal.
 * Socket writes remain necessary for D-Bus; filesystem mutation is prohibited.
 * Production has no inherited file descriptors besides stdio and its bus socket. */
gboolean cf_worker_read_only(GError **error)
{
#if defined(__x86_64__)
#define CF_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define CF_ARCH AUDIT_ARCH_AARCH64
#else
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Read-only worker requires Linux x86-64 or AArch64.");
    return FALSE;
#endif
#ifdef CF_ARCH
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, CF_ARCH, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, nr)),
#if defined(__x86_64__)
        /* Reject x32 syscall numbers rather than allowing an alternate ABI. */
        BPF_JUMP(BPF_JMP|BPF_JGE|BPF_K, 0x40000000, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_open, 0, 4),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, args[1])),
        BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, WRITE_FLAGS, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        DENY(creat), DENY(unlink), DENY(rename), DENY(mkdir), DENY(rmdir),
        DENY(chmod), DENY(chown), DENY(lchown), DENY(link), DENY(symlink), DENY(mknod),
        DENY(utime), DENY(utimes),
#endif
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_openat, 0, 4),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, args[2])),
        BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, WRITE_FLAGS, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        DENY(openat2), DENY(unlinkat), DENY(renameat), DENY(renameat2), DENY(mkdirat),
        DENY(open_by_handle_at),
#ifdef __NR_fchmodat2
        DENY(fchmodat2),
#endif
        DENY(fchmod), DENY(fchmodat), DENY(fchown), DENY(fchownat), DENY(linkat),
        DENY(symlinkat), DENY(mknodat), DENY(utimensat), DENY(truncate), DENY(ftruncate),
        DENY(execve), DENY(execveat), DENY(mount), DENY(umount2), DENY(pivot_root),
        DENY(ptrace), DENY(process_vm_writev), DENY(bpf), DENY(io_uring_setup),
        DENY(setxattr), DENY(lsetxattr), DENY(fsetxattr),
        DENY(removexattr), DENY(lremovexattr), DENY(fremovexattr),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW)
    };
    struct sock_fprog program = { .len = G_N_ELEMENTS(filter), .filter = filter };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0 ||
        syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &program) < 0) {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno), "Cannot install read-only syscall guard: %s", g_strerror(errno));
        return FALSE;
    }
    return TRUE;
#endif
}
