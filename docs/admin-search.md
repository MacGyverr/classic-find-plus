# Administrator searches

Find Plus has an **Allow admin search** checkbox above More Filters. It starts unchecked
on every launch and is never saved. Checking it does not authenticate. Find
captures one reference instant, validates the effective criteria and resolves relative dates,
then requests Polkit authentication. Main date bounds AND Last N minutes/hours/days/months
are intersected before IPC; protocol v4 transmits exact absolute microsecond cutoffs, so
authentication delays never shift the interval. No new date-mode enum is sent to the worker.
Other search tabs ignore the checkbox. Uncheck it for ordinary searches. A denied, canceled
or unavailable authorization ends that search with an error; there is no ordinary fallback.
The status distinguishes administrator searches. Stop/Esc and closing the window cancel.

The GTK application stays your regular user. A temporary system D-Bus worker runs as root,
using the same scanner and all functional filename, RegEx, exclusion/pruning, type/MIME,
extension, size, date, numeric UID/GID, exact Unix permissions, hidden pruning, safe symlink
following, recursion and filesystem filters. Requests use schema version 4; mismatched GUI/worker
versions fail rather than dropping filters. Restart CFP after package upgrades. It accepts local `file:///` locations,
including mounted filesystems, rather than remote/admin/GVfs URI credentials. Mount a remote
filesystem normally first if appropriate. The selected root may be a directory symlink;
descendant directory symlinks require Follow enabled and obey identity, pruning and actual
target filesystem restrictions.

Results feed the existing columns, sorting, selection, history, export and URI drag-and-drop.
Visibility does not grant permission to open or transfer a protected file. GUI file actions,
export and synchronization still run as your ordinary user; inaccessible operations fail
normally. An administrative destination authenticates through the receiving file manager.
Containing Text supports authorized streaming literal UTF-8 and Hex matching; see [content search](content-search.md).

## Authorization boundary

`org.classicfind.Plus.Search` is a root-owned system-bus service, not a session-bus service.
Its only methods are Start, Read and Cancel. Start validates a UUID, bounded local URI and
typed compiled criteria, identifies the actual bus sender, and asks Polkit to authorize
`org.classicfind.plus.search` for that **system-bus-name** subject. The supplied policy denies
inactive/other sessions and requires administrator authentication for active sessions without
`auth_admin_keep`. Local administrator rules can override Polkit policy. CFP never sees passwords.

The single request is bound to that unique sender and UUID. Other callers cannot read or
cancel it; concurrent Start requests receive Busy and can retry after the worker exits.
Replies are metadata-only, pulled in batches of at most 32 with a 512 KiB message bound.
The client validates types, lengths and result locations. Queues are bounded. Disconnects,
authorization timeout (120 seconds), no reads (30 seconds), cancellation and a maximum
15-minute request lifetime end the worker. Cancellation preserves already delivered rows;
the worker exits within five seconds even if a filesystem/NSS call is stuck.

The native worker has 512 MiB address-space, 64-descriptor and 120 CPU-second limits.
Very deep trees can report descriptor-limit errors. It closes inherited extra descriptors
and uses the local GIO VFS. A Linux seccomp filter applies across its threads and rejects
write opens, filesystem mutations and program execution. It is an additional guard around
the narrow API, not a general-purpose containment service. No setuid binary, shell interface,
sudo subprocess, file mutation/export method or credential cache is provided.

MIME detection retains existing GIO behavior: when a name is ambiguous, the verified GLib
2.72 implementation reads at most 4096 initial bytes (2.84.4 reads up to 16384 bytes) to classify the file; bytes are never
returned over IPC. It avoids sniffing zero-size pseudo-files and known special file kinds.
See the [Mint-target GLib implementation](https://github.com/GNOME/glib/blob/2.72.1/gio/glocalfileinfo.c)
and [development GLib implementation](https://github.com/GNOME/glib/blob/2.84.4/gio/glocalfileinfo.c).
This bounded metadata sniff can affect access times on filesystems where O_NOATIME is unavailable.

## Installation and troubleshooting

Use the distribution-matched DEB or a root-owned `/usr` source installation. Both build
systems install the worker, system-bus activation/config and Polkit action. Installing into
`~/.local` alone cannot register a root system service. `/usr/local` or custom prefixes may
not be searched by the system Polkit installation; use a separately installed `/usr` worker.
Never point a root service at an executable or parent directory writable by ordinary users.

The desktop needs a functioning system bus, Polkit authority, active login session and
authentication agent. A headless/WSL session may have none. Missing service, inactive session,
denial, worker crash and timeout are reported through the search error/status tooltip.
Do not loosen bus/policy or display access controls to suppress an error. After service
installation, log out/in or use the distribution's supported bus/policy reload procedure.
The helper's syscall guard supports Linux x86-64/AArch64; other architectures refuse worker
startup. AArch64 is not verified. Ordinary searching is available without this service.

The owner verified real Mint Polkit prompting, authorized protected searches and explicit
full-root GUI operation with version 1.0.1. Authentication denial/cancellation on that desktop
has not been separately reported; private tests cover those protocol paths.
Private tests verified actual bus identities, authorization allow/deny/cancel against a
mock authority, malformed input, owner-only requests, resource cleanup and read-only guards.
The production worker also passed protected-directory traversal and mounted-filesystem
tests as UID 0 in isolated user/mount namespaces; a restricted client still could not open
the returned protected file. These checks do not replace a real active-login authentication
agent or verify host-root desktop operation.

## Explicit full-root GUI

```sh
classic-find-plus                      # ordinary user
sudo classic-find-plus                 # refused, nonzero exit
sudo classic-find-plus --allow-root    # explicit full Administrator Mode
```

The override shows a startup warning and Administrator Mode title. All features, including
opening files, exporting and file operations, run as root and can change system files.
Use it only when you intend that authority. It does not grant X11/Wayland session access;
if the session refuses root, CFP reports that limitation without weakening access controls.

Root uses its passwd home, ignoring inherited user HOME/XDG paths. Configuration, cache and
data have separate `classic-find-plus-administrator` XDG roots (for example
`/root/.config/classic-find-plus-administrator/classic-find-plus/settings.ini`). Existing
symlink, wrong-owner or group/world-writable profile directories are refused. Ordinary-user
preferences and cache are not reused. This override is separate from the safer search-only
checkbox, which keeps file operations under ordinary permissions.

Protocol v3 retains the existing tuple shape and sends raw Exclude text/matching mode.
The shared criteria parser supports non-RegEx OR rules identically in both processes;
RegEx remains one expression. The version change prevents silent mixed-version exclusion
behavior. Restart CFP after upgrading; v2 peers report a protocol mismatch.
