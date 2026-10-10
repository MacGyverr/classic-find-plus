# Results synchronization

Dragging sends the original URI/icon-list payload to Nemo. The source receives gesture completion,
not a trustworthy notification that Nemo finished copying, moving or authenticating. No row is
removed because of a reported drag action or MOVE acknowledgement.

## Verification and scope

At drag start the existing selected GFile snapshot is retained by the drag context. A per-view
synchronizer captures stable GtkTreeRowReferences and file IDs for those displayed rows (and
displayed descendants of dragged folders). Subsequent selection changes do not alter it.
Up to 64 native source-parent directories are watched with G_FILE_MONITOR_WATCH_MOVES. Unsupported
monitors and extra directories fall back to delayed GIO queries. There is no recursive watcher,
indexer, transfer helper or automatic whole-search rerun.

Drag end arms asynchronous, read-only verification. Each window has at most one verification GTask
per search generation, with up to 128 affected files per batch. Metadata I/O and destination-scope
checks run off GTK; only completion callbacks update GTK. Events are coalesced with a short delay.
Fallback checks occur around 0, 0.25, 1, 3, 10, 30, 60 and 110 seconds; native monitors last up to
120 seconds after the drag ends. Continuous source activity does not extend that deadline.
Timers and watchers stop when their affected rows expire or disappear. Bounded folder hints also
verify descendant rows delivered later by an already running search, even after the original
folder row was removed; these hints expire at the same deadline.

- Confirmed NOT_FOUND or NOT_DIRECTORY removes the stale source row. A surviving source is
  refreshed/revalidated, so copies, failed moves and cancelled gestures keep matching originals.
- Permission errors, cancellation, unsupported mounts, other I/O errors and incomplete metadata
  retain the row. In particular, GIO can return a name-only GFileInfo without a stat error; that
  is not evidence of deletion or filter mismatch.
- A monitor-supplied new location is accepted only after the old path is confirmed absent, the
  destination ID matches the captured file ID, and scope/filters match. No filename guessing.
  Directory moves can correlate already displayed descendants through the directory event and
  each descendant's identity. Metadata, basename, folder, full path and sort values update together.
- Destination scope follows the captured root, recursion and filesystem restrictions. The root
  itself is not a result; descendant directory symlinks require captured Follow enabled.
  Destination ancestors obey captured Exclude/No Hidden rules and actual target filesystems;
  resolved link targets are checked against pruned subtrees. Inaccessible/unknown
  scope is not treated as a reliably correlated destination.
- Correlated moves outside scope or renames failing active filters remove the stale row. With an
  unknown destination only the stale source is removed; Find Now can rediscover an in-scope move.
- In-app rename uses the same captured filters, rather than the current editable form. Trash
  retains its existing confirmed-success behavior. No permanent-delete behavior was introduced.

Each Find invocation replaces the immutable synchronization criteria and increments a generation;
old timers, monitors and GTasks are cancelled. Row references follow sorting without retaining raw
iterators across changes. A callback validates its generation, row, current path and event revision
before updating. Selection on surviving/updated rows and all column preferences remain intact.
The final status counts displayed rows after removals. Closing a view cancels everything and weak
view references prevent pending completions from touching destroyed GTK objects.

## Limits and refresh

Only recently dragged results are watched, not every result directory in a recursive search.
External deletion, rename, move and metadata changes to those tracked rows are detected during the
window. Other external changes and newly copied destination files require a fresh Find Now search.
Backends without usable move events or file IDs cannot reliably update destinations.

Nemo remains responsible for transfer completion and authentication. Two minutes covers ordinary
short delays, not an unlimited password dialog or arbitrarily long operation. If authorization or
a transfer completes later, press Find Now after it finishes. There is no permanent polling loop
and no promise that an unmonitored remote filesystem reports events. Local mounted filesystems use
GIO; remote URI providers use the bounded query fallback. Network-volume behavior still needs
manual coverage on the intended desktop.

