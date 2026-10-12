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

F2 renames a single selection. Rename requires a nonempty UTF-8 filename without slash, `.` or
`..`, and fails if the destination exists. Delete requests Move to Trash, with a Cancel-default
confirmation. There is no permanent-delete fallback. Operations query current availability and
compare the search result's `id::file` where the backend provides it. Replaced files are rejected.
These are best-effort GIO checks, not an atomic defense against a replacement racing the final
filesystem call. Symlinks are renamed/trashed themselves, rather than their targets. Mutations
run in GTask workers, sequentially within each request, and report errors. Closing cancels pending
work; a mutation already completed remains completed. Errors for a multi-file request do not
undo successful earlier items. Successful rename refreshes an existing row; Trash removes it.
Successful renames also revalidate the captured search filters; a nonmatching rename removes the
result. Recently dragged files have bounded monitoring and asynchronous verification, described in
[results synchronization](results-sync.md). Other external changes require Find Now to refresh. Permission-denied and
read-only errors include access guidance; [Phase 5 findings](permissions.md) describe authentication
prerequisites and the conditional GVfs route.

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
