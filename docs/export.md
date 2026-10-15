# Exporting Results

The Export results button sits below Find/Stop. It is enabled only when the displayed model
contains rows, including stopped partial searches, and disabled while writing. First insertion,
last removal, model replacement and new-search clearing update it immediately. Empty snapshots
do not open a chooser or write files; errors restore sensitivity from the current model. It captures all current rows, even when
only some are selected, and visible columns in their current left-to-right order. Headers
and cells come from the GTK view and its effective model, including sorting/filter wrappers.
The existing column registry supplies stable identifiers and display-string mappings for
all fifteen columns. No new metadata lookup, search, independent sorting or selection
change occurs. Search and synchronization continue; their subsequent changes cannot alter
the export. One chooser per window retains one immutable click-time snapshot for retries.

Full Path and In Folder values derive directly from each row's GFile, rather than display
strings or URI decoding. Native locations use g_file_get_path with lossless filesystem-to-
UTF-8 conversion: spaces, Unicode, #, percent signs and literal %20 remain actual characters.
CSV and TXT share this conversion; there is no `%20` replacement. Non-native locations keep
URIs. Native paths with bytes unrepresentable as UTF-8 also retain a lossless URI fallback
instead of silently changing their identity. Clipboard Full Path refuses that unrepresentable
case. Export performs no extra metadata lookup and does not change URI drag-and-drop.

CSV is the default: UTF-8, commas, every field and header quoted, embedded quotes doubled,
CRLF record endings. Embedded newlines remain within quotes. TXT uses literal tabs, no
added quotes or quote doubling, headers and CRLF. Each embedded TAB, CR and LF becomes
one space in TXT only; ordinary quotes, commas, Unicode and other spaces remain unchanged.

Both formats prefix one ASCII apostrophe to any field beginning with TAB, CR or LF, or
whose first character after Unicode whitespace, control characters and BOM is =, +, - or @.
This includes headings. This deterministic spreadsheet safeguard may appear as a literal
apostrophe in text readers. Ordinary non-formula fields keep their spaces and punctuation.
Invalid UTF-8 in other display-only fields becomes U+FFFD in the snapshot. Stored values and original files are untouched.

The native GTK Save chooser starts in the captured root of the last successful search,
independent of later Look in edits. Non-writable roots stay selected; inaccessible/missing
roots fall back to home. CSV/TXT switching preserves custom basenames and strips recognized
suffix chains case-insensitively before appending the chosen extension. Unknown suffixes
are retained as part of the basename. If an entered extension changes at Save, the chooser
updates it and asks you to press Save again, ensuring confirmation concerns the final path.

GTK supplies one standard overwrite prompt. Declining leaves the chooser open. Existing
regular files have their version captured at confirmation and checked before replacement;
changed files require choosing/confirming again. Symbolic links and nonlocal URI outputs
are rejected. Use a local or mounted filesystem, ordinary user permissions and another
folder when access is denied. No automatic elevation or password handling is involved; search-worker authorization does not
grant export permissions. Explicit full-root Administrator Mode exports with root permissions.

The worker holds immutable strings only and streams one record at a time into a private
0600 temporary file in the destination directory. It closes successfully before commit.
Existing files use an atomic same-directory rename; new files use Linux rename-no-replace
(with an exclusive link fallback on older kernels). Failures/cancellation remove temporary
output without committing partial data. The chooser retains the snapshot for retry after
write/close/commit errors. Closing the chooser/window cancels pending work; a commit already
completed remains completed. An application hold keeps the main loop alive after the last
window closes until the cancelled worker has completed cleanup. Success closes the chooser
only after commit. No host package
installation occurs during development verification.

Limits: snapshot memory grows with visible data and capture briefly occupies the GTK thread.
Streaming does not create a second whole-file buffer. Cancellation is cooperative; kernel or
mounted-filesystem calls can delay it. Atomic rename is not a power-loss durability guarantee.
Version checks cannot eliminate same-user replacement/ancestor races between check and rename;
this is ordinary-user export, not a hostile-directory isolation boundary. Actual disk-full
serialization is tested by an injected NO_SPACE stream, not by filling the host filesystem.
Administrative support depends on the receiving desktop and backend.

Bytes and Bits are initially hidden. When visible, their exact comma-grouped values are
exported unchanged: CSV quotes the commas; TXT uses literal tabs. See [exclusion and exact sizes](exclusion.md).
