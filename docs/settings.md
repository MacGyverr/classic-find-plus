# Settings, shared filters and history

Classic Find Plus 1.0.5 stores its configuration at
`$XDG_CONFIG_HOME/classic-find-plus/settings.ini`, or normally
`~/.config/classic-find-plus/settings.ini`. GLib supplies the XDG directory. The application creates
its directory with mode 0700 and atomically replaces the INI file with mode 0600 as the current user.
No configuration is stored beside the executable. Search results are never saved or restored.

## Shared values and effective searches

The existing per-window CfSearchForm is the authoritative editable model. Equivalent controls
bind to the same properties in both directions. Different Find windows keep independent drafts.
When Find starts, a separate effective request selects the active tab's filters before validation:

| Tab | Effective filters |
| --- | --- |
| Name & Location | Filename, filename mode and case option |
| Date Modified | Date field, time mode, expressions and relevant period/bounds |
| Advanced | Type, extension and size mode/values/units |
| Find Plus | Main filename, Exclude, size and enabled After/Before; eligible expanded More Filters |
| Help/About | Informational; Find and search shortcuts do not start work |

Location, Include Subfolders and Stay on filesystem and Follow symbolic links are global scope options for all four search
tabs. Stay on filesystem keeps its existing recursive traversal semantics even when its Find Plus
control is above More Filters. Case sensitivity remains available to extension matching.
Advanced retains its original size controls; a stored filename does not restrict Advanced results.

Find Plus alone has a compact layout. Its two-column More Filters has separate ephemeral
values and enable flags for File type, Extension, Date mode, Group, User and
Permissions; No Hidden files is its own checkbox. A filter participates only on Find Plus,
with the expander open and its checkbox checked. Collapsing immediately resets all these
values/flags (Any type, empty extension, Any date, N=7, current account/group if available,
empty permissions, unchecked hidden exclusion), clears optional validation feedback and does
not search. Reopening starts at defaults. Every new window starts with optional filters off,
even if Expanded is saved true; optional values/flags are never persisted or restored.
Older/stale optional keys are ignored. Advanced and Date tab drafts remain separate and saved.

Main filename, Exclude, size and enabled After/Before retain their values. With optional Date
mode off/Any date, the main After/Before bounds apply (exclusive, even if the Date tab stores
inclusive Between). An active optional Last N mode adds an AND interval using the same timestamp field.
Previous N has no independent enable flag; it enables automatically for the checked
relative mode on the active expanded Find Plus tab. Disabled malformed N is ignored.
Legacy plus-recent-enabled keys are ignored, never restored or written. Saved date-mode
numbers 0–6 keep their original meanings; minutes=7 and hours=8 are appended stable
IDs, mapped independently of dropdown ordering. Global
scope/traversal/case, admin choice and already captured searches are unaffected by collapse.
Validation applies only to the effective filters. For example, a saved invalid date cannot block
Name & Location, and an invalid RegEx cannot block Date Modified or Advanced.

The active request is copied into the existing immutable search criteria. Relative date/time
expressions stay as entered in the form and INI; each search evaluates them using its start clock.
Results synchronization continues using that execution's effective snapshot, not later draft edits.
Containing Text is persisted but remains a disabled placeholder without content searching.
Exclude is a separate FindPlus key, active only on Find Plus even with More Filters collapsed.
Other tabs ignore its validation and matching. It shares filename mode/case but never history.
See [exclusion rules](exclusion.md).

## Configuration structure and migration

- **Window:** Width, Height, Divider, Maximized and Tab (0–4, including Help/About).
- **Search:** location, name, name-mode, case-sensitive, recursive, stay-on-filesystem and follow-symlinks (defaults false).
- **Date:** date-mode, date-start, date-end and recent. Dates display as yyyy/mm/dd.
- **Advanced:** extension, extension-only, file-type, size-mode, size-start, size-end and size-unit.
- **FindPlus:** Expanded (defaults false), containing-text and exclude.
- **Columns:** Order, Sort, Direction, Visible_<id> and Width_<id>.
- **History:** Names, a GKeyFile escaped string list, newest first.

Stable column IDs are name, folder, size, type, modified, created, accessed, permissions, path,
mime, detailed-type, group, owner, bytes and bits. Missing Bytes/Bits visibility defaults to hidden;
nine/thirteen-column saved orders retain their layout with the new IDs appended.
Name always remains visible, including when migrating an old hidden-Name layout. Other columns may
be hidden; widths, ordering and raw numeric/date sorting survive restart. Saved explicit widths
use fixed GTK sizing; unresized columns retain automatic sizing. Width validation accepts 40–4096
pixels; missing or invalid widths use automatic sizing. Window sizes are checked against the
existing 640×420 minimum and 8192-pixel bounds. Absolute positions are not forced on Wayland.

On first use, if settings.ini is absent, the application imports the existing
`classic-find/window.ini` and `classic-find/columns.ini`. Original files remain untouched. Legacy
numeric column IDs and visibility lists remain readable. Once settings.ini exists, it is
preferred and legacy files are not repeatedly reimported. Unknown keys survive subsequent saves;
missing/invalid typed values use safe defaults. A malformed whole INI uses defaults and is replaced
on the next successful save. User configuration is not removed by a package upgrade or removal.

An explicit command-line/Nemo directory overrides the saved location. Independent startup uses
the saved location if it is an accessible directory, otherwise the user's home directory. Explicit
invalid arguments still report an error. Startup restores values/tab/layout but never searches;
switching tabs also never searches. An informational restored tab keeps Find disabled.

## History

The arrow at the right of either filename entry opens the same persistent list. A valid initiated
Name & Location or Find Plus search adds its nonempty filename, moving an exact duplicate to the
front and keeping at most 20 entries. Capitalization, spaces, wildcards and unusual characters are
preserved. Typing, Date Modified and Advanced searches do not add entries. Selecting an entry
updates both filename controls without searching. Clear History empties the list without erasing
the current filename, which is stored separately as the editable Search value.

## Saving and simultaneous windows

The application uses independent processes (`G_APPLICATION_NON_UNIQUE`) and independent per-window
drafts. Settings managers track changes by key, debounce them for 350ms, and flush on a valid search
and controller closure. Tab, expander, geometry, column and draft changes therefore save before
exit when idle. Callbacks own no GTK widgets and their timers are cancelled before manager teardown.
Loaded values are applied before tracking edits; initialization cannot overwrite them with defaults.
Absent form defaults are filled only if still absent when the save lock is held.

Each writer locks the adjacent settings.lock, reloads the latest INI, merges only its pending keys,
and performs an atomic replacement. Closing an unchanged stale window does not revert another
window's preferences. Different-key edits coexist. If two windows edit the same key, the last
successful locked save wins (the 350ms debounce means this is save order, not keystroke time).
Sorting's column/direction and column ordering are logical values; simultaneous edits to those
values likewise use the last writer. History add/clear operations merge against the newest disk
list under the same lock; concurrent additions are retained, with newest completed operations first.

Open windows are not automatically changed to match another window's draft or layout. New windows
load the latest saved preferences; history menus read the latest shared list when opened. Write
errors are reported to stderr, leave pending changes available for the next edit/search/close, and
do not change search or transfer behavior. Ordinary result synchronization never launches a privileged worker or requests credentials.

## Extended metadata and positions

Date.date-field is the shared Modified (0, default), Created (1), Accessed (2) selector.
It accompanies the existing date mode/expressions; tab eligibility remains unchanged.
Stay on this filesystem now sits above More Filters and participates regardless of expansion.

Columns add stable IDs mime, detailed-type, group, owner. Legacy nine-column order is retained
and the four hidden columns appended. Their Visible_<id>/Width_<id>, order and sort use the
same existing per-key persistence. Owner/group numeric fallbacks sort numerically; names collate.
Type shows filesystem kind, Detailed Type the reported GIO content description, MIME Type the
reported identifier. Worker-owned ownership caches are bounded to 4096 UIDs and GIDs per job.

On X11, Window.X/Y store the last changed mapped normal position. A separate controller ignores
temporary initial placement, iconified/maximized/fullscreen and transition positions.
The first settled normal position seeds missing X/Y defaults under the lock; it never replaces
an existing saved pair. It waits for stable
configuration events and flushes both coordinate keys as a changed pair under the central lock,
even if one coordinate equals that window's stale local value; closing an older unchanged
window cannot revert a newer position. A one-shot measured correction handles WM frame margins, including reducing oversized
client dimensions after measuring the real frame.
Restore uses current logical monitor workareas; invalid/offscreen positions are clamped to a
visible monitor and large remembered sizes reduced where the 640×420 app minimum permits.
Physical monitors smaller than that minimum cannot show the entire app. No global coordinates
are manipulated or saved on Wayland; normal size, tab, layout and maximization still restore.

Input limits count Unicode characters: text/pattern/extension/rename 8192, path 32768, date
expression 1024, number 32. Oversized stored form/history strings are ignored, not truncated into
new searches. INI input/output is bounded to 1 MiB and regular files; malformed, unreadable,
symlink/nonregular or oversized files are preserved on save failure. Fix the file or move it aside
to restore saving. Nothing deletes or rewrites an invalid user configuration automatically.

Allow admin search is transient and never persisted. Full --allow-root operation uses separate
administrator XDG roots beneath the root passwd home, ignoring inherited user HOME/XDG paths.
File-manager integration ownership state is separate from criteria preferences; the integration
manager locks and updates its own fixed-path/hash manifest. See admin-search.md/integrations.md.

Exclude persists its complete raw UTF-8 expression, including pipes, backslashes, leading/
trailing spaces and empty terms. Parsed rules are never stored. Mode changes alter only
interpretation; they do not edit text or start searches. Existing per-key locked saves and
other preferences/history remain unchanged.
