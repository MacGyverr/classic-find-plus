# Results and file actions

Results support native Ctrl-click, Shift-click ranges, Ctrl+A, arrow-key navigation and column
sorting. A press on a selected row retains the selection for dragging; releasing a simple click
selects that row. Double-click or Return opens the activated result with its default application.
Right-click retains a selected group, or selects the row clicked. The keyboard menu key/Shift+F10
opens the same menu.

Open works for multiple selected files. Open With uses GTK's application chooser for one file.
Open Containing Folder opens each distinct parent through its default folder handler. Properties
shows fresh metadata for one item, including selectable full path, type, size, local modification
time and Unix permissions where available. All launch arguments use GFile URIs and GIO; there is
no shell command constructed from a filename.

Ctrl+C/Ctrl+X publish a selection snapshot to the GTK clipboard using
`x-special/gnome-copied-files`, `text/uri-list` and text targets. Cut marks the clipboard data; it
changes no source file until a receiver pastes it. Clipboard ownership survives closing a window
while the app remains running; persistence after app exit depends on a desktop clipboard manager.

Copy Name to Clipboard, Copy Name.EXT to Clipboard and Copy Full Path to Clipboard use
ordinary desktop CLIPBOARD text. Right-click within an existing selection to copy **all
highlighted rows**; right-click outside it to select and copy only the clicked row. Keyboard
context invocation uses the current selection, including multiple rows. Each menu item owns
an immutable text snapshot in current displayed model order, respecting sorting/filter
wrappers. Selection changes, sorting and clearing results cannot change that snapshot.
Entries have single newline separators, no added trailing separator, bullets, quotes or
deduplication of matching basenames. Empty-space menus disable all three commands and Rename….
Copy Full Path uses the actual native filesystem path, without file:// or URI escaping;
literal %20 stays %20. Non-native Full Path copying is disabled with an explanation.
Existing Ctrl+C/Ctrl+X file payloads and drag-and-drop continue using URIs.

Copy Name strips only a nonempty final extension: `Annual.Report.pdf` → `Annual.Report`,
`archive.tar.gz` → `archive.tar`, `.config.json` → `.config`. `README`, `.bashrc` and `file.`
remain whole. Directories always keep their whole basename, including periods. Names convert
losslessly from filesystem encoding to UTF-8 and round-trip; unsupported names are disabled
with an explanatory tooltip. Clipboard persistence after exit depends on the desktop manager.

F2 with one selected row or **Rename…** on a clicked row opens the native inline Name editor.
The filename stem is selected using UTF-8 character offsets; directories select the whole name.
Extensions can be changed deliberately. Opening the editor uses cached result data only:
no filesystem query, permission check or rename, even for protected/inaccessible files.
The native entry supports selecting/copying part of a name with Ctrl+C; normal text shortcuts
stay in the entry. Enter with unchanged byte-equal text closes without any filesystem task.
Only changed text requests validation/rename. Missing cached identity blocks a changed commit,
not text editing. Escape and focus loss cancel without committing incomplete input. Errors
close the editor, keep the real source/model unchanged and preserve selection where possible. Only one editor/mutation is allowed per view. Rename is
unavailable during a search; starting a search cancels editing, and an outstanding rename
must finish before another search starts. The results view briefly becomes unavailable during
mutation/metadata refresh to prevent actions on the former path.

A second single click on the same selected Name cell starts editing only after the configured
GTK double-click interval and within 500 ms of the first press. Timing uses the monotonic clock;
a release idle lets existing selection handling finish. Different rows/columns, modifiers,
selection changes, drag thresholds, row deletion, model replacement and new searches cancel
pending gestures. Fast double-click still opens. At GTK intervals of 500 ms or more, no valid
slow-click interval exists; F2 and Rename… remain available. No desktop setting is changed.

Rename changes the real local filesystem entry using normal GUI permissions. Names must be
nonempty, valid UTF-8 representable in the filesystem encoding, and contain no slash or NUL;
the whole basename cannot be `.` or `..`. Spaces, Unicode and other legal Linux punctuation are allowed.
Unchanged editor acceptance performs no filesystem operation or permission check. Existing destinations (files, directories or symlinks)
are never overwritten, even when another process creates them concurrently: Linux
`renameat2(RENAME_NOREPLACE)` enforces the collision rule atomically. Unsupported locations,
kernels and filesystems fail safely, without check-then-rename fallback.

The worker checks captured `id::file`, pins parent/source using `O_PATH` descriptors, and
rechecks device/inode and parent identity immediately before renaming. Symlinks are renamed
as entries without following their targets. **Residual limitation explicitly accepted by the
owner:** Linux provides no conditional source-inode rename; another process can still replace
the source name (or parent path) after the last check. Descriptors reduce this race but cannot
eliminate it. This limitation does not weaken atomic destination non-overwrite.

On success the cached row gets refreshed name/path/GFile/metadata, preserving selection and
reapplying sorting; URI clipboard/DnD and export use that new row. Nonmatching renames leave
the results. Descendant rows and pending sync entries beneath a renamed directory are
invalidated rather than rebased or rescanned; Find Now repopulates eligible children. If
metadata refresh fails after success, the old pathname is removed instead of retained.
Missing/replaced sources, collisions, invalid names, permissions, read-only filesystems and
unsupported locations report errors. Administrator search never grants rename privileges.

Delete requests Move to Trash with a Cancel-default confirmation and no permanent-delete
fallback. Trash retains the existing GIO checks and cancellation semantics. Closing cancels
pending work; completed filesystem mutations remain completed. Multi-file errors do not undo
earlier successes. Recently dragged files retain bounded asynchronous verification; see
[results synchronization](results-sync.md). Other external changes require Find Now.

# Drag-and-drop

The GTK source advertises COPY and MOVE with both `text/uri-list` and Nemo's
`x-special/gnome-icon-list`. URI escaping preserves spaces, Unicode, percent/hash, newline and
other unusual filename bytes. The icon-list supplies neutral geometry. Drag data holds references
to the selected original GFiles at drag start, independent of later selection/model changes. No
temporary file copies are made. A MOVE acknowledgement cannot invoke GtkListStore's row deletion;
the destination owns the transfer. Ctrl/Shift and the receiver decide the selected action.

Some Nemo versions default to COPY for uncached external files even when source
and destination share a filesystem. Shift requests MOVE; Ctrl requests COPY. This is receiver
behavior, not a source filesystem decision. Upstream Nemo's icon-list handler checks its cached
source filesystem and deletion metadata and otherwise conservatively copies:
https://github.com/linuxmint/nemo/blob/master/libnemo-private/nemo-dnd.c
Other GTK receivers may choose different defaults. Cross-filesystem, unavailable-source and
permission errors are handled by the receiving file manager.


Administrator search results retain ordinary file-operation permissions; worker authorization
is never forwarded to actions or receiving managers. Explicit --allow-root instead runs the
whole GUI with root authority; see [permissions](admin-search.md).

For administrator-opened receiving folders in Nemo, GNOME Files (Nautilus), Caja and
Thunar, see [transfer steps](permissions.md). Thunar Enable includes Open as Administrator.
Dolphin administrative transfers remain unverified; ordinary drag-and-drop is unchanged.

Optional GTK PCManFM and XFE Find menus do not imply administrative receiving support.
Their protected-destination transfers remain unverified; see permissions.md.
