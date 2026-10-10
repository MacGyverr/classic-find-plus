# Search behavior

## Criteria
- Blank Named matches everything. Default matching is a case-insensitive Unicode casefolded
  substring unless `*` or `?` is present, in which case the complete name is glob matched.
  `*` matches zero or more characters and `?` matches one character. Exact name treats wildcard
  characters literally. Case sensitive disables folding in these modes.
- RegEx uses GLib's Perl-compatible regular expressions. The original pattern is compiled once
  per immutable search criteria snapshot, with `G_REGEX_CASELESS` unless Case sensitive / Aa is
  enabled. Neither the regex nor its target is Unicode casefolded first, so escapes such as
  `\D` retain their meaning. Matching can occur anywhere; use anchors to require the whole name.
  For example, `^report-[0-9]{4}\.txt$` matches `report-2026.txt`; `\.txt$` matches that suffix.
  `*.txt` is not valid regex syntax; it remains supported in Partial / wildcard mode.
  Malformed expressions show a validation error before clearing results or starting a worker.
  Match Named against extension only also applies in RegEx mode. Other filters remain ANDed.
- Named uses filesystem names converted to valid display text. Actual GFile identities retain
  original path bytes. Invalid-byte filename search/display semantics require further coverage;
  such names are never used as shell commands.
- Extension accepts one suffix: `jpg`, `.jpg`, `*.jpg`, or multi-part `tar.gz`; leading syntax is
  normalized, and the preceding dot is required. It excludes folders and treats a leading-dot-only
  name such as `.jpg` as extensionless. Case setting also applies to extensions.
- Match Named against extension only changes Named's target to the suffix after the last dot;
  other filters still apply. A dotfile with no later dot has no extension.
- File type can be any, regular files, folders, or image/audio/video/text MIME families. GIO
  supplies content types. These are classifications, not guaranteed content validation.
- Size values are unsigned decimal digits only. Signs, whitespace, fractions and exponents are
  rejected. Bytes/KiB/MiB/GiB multiply by 1/1024/1048576/1073741824 with checked uint64 overflow.
  There is no artificial file-size cap. Size comparisons apply only to regular files, since folder
  metadata size is not a recursive folder size. Greater/less are strict; between means
  `lower < size < upper`, with upper greater than lower. Equal to compares exact byte size after
  unit conversion. Zero is valid.
- All criteria are ANDed. Invalid active inputs leave existing results untouched. Inactive fields
  do not restrict or invalidate a search.

## Find Plus exclusions

One saved Exclude expression shares filename mode and case, but applies only on Find Plus.
Ordinary expressions match files and directories; a leading / targets directory components.
Literal /new uses a prefix; wildcard/RegEx patterns retain their semantics. Matching directories
are skipped before enumeration, with the selected root exempt and descendant symlinks not
followed. Other enabled filters remain ANDed. See [full rules and examples](exclusion.md).

## Local-time dates
Editable YYYY/MM/DD entries and calendar selectors use the Linux desktop's local time zone.
Typing eight digits inserts separators; legacy YYYY-MM-DD engine input is also accepted.
- All files imposes no date constraint.
- After D excludes that date: `mtime >= local midnight of D+1`.
- Before D excludes that date: `mtime < local midnight of D`.
- Between A and B includes both calendar dates: `[midnight A, midnight B+1)`.
- Previous N days/months uses a positive whole calendar interval subtracted from the time
  criteria are captured. The lower and capture-time bounds are inclusive; future timestamps
  are excluded. Calendar days respect DST; months clamp to the last valid day when needed.
- End before start, impossible dates, invalid numbers and calendar overflow are errors. Modified
  time includes microseconds supplied by GIO. Missing modification metadata cannot match a date filter.

The Find Plus After and Before switches enable the corresponding shared bounds. Enabling both
uses an exclusive interval: `[midnight After+1, midnight Before)`. It must contain at least one
calendar day. The Date Modified mode selector (also in Find Plus More filters) preserves the
inclusive Between option and Previous N days/months. Before reads the upper date field.

## Advanced date/time expressions
Calendar buttons open the native Date/Time Selection dialog. Choose a fixed calendar date or
Today, Yesterday, Tomorrow, Start of week (Monday), Start of month, or Start of year. Optional
signed adjustments apply in years, months, weeks, days order; month/year adjustments clamp the
day to the last valid day. Specify Time enables fixed `HH:mm:ss` (24-hour local time), or Now,
Midnight or Noon. Optional time adjustments apply as elapsed hours/minutes/seconds. Previews
show the retained expression and its current local evaluation, including the time zone.

Examples accepted in either date field:

```text
2026/03/01
Today -7 days
StartOfMonth +1 months -1 days
Yesterday @ Noon
Today @ Now -2 hours
2026/03/01 +1 weeks @ 12:30:00 +30 minutes
```

Relative expressions are stored as text in the shared draft. Both bounds resolve against one
captured clock when Find starts; reopening the dialog preserves their settings. Changing the
system date later does not alter an already running search. With Specify Time, After means strictly
after that instant and Before strictly before it; inclusive Between includes both instants at
microsecond precision. Without time, the existing whole-day boundaries apply. Impossible dates,
nonexistent local times during a DST gap, adjustments outside ±1,000,000 units, calendar overflow
and empty/reversed intervals are validation errors. Cancel leaves the draft unchanged.

## Shared layout and result columns
All four tabs edit one per-window draft. Execution selects the active tab's filters before validation:
Name uses filename/options; Date uses date/time; Advanced uses type/extension/size; Find Plus combines
eligible main controls and expanded More Filters. Collapsed hidden criteria are ignored without
clearing values. Scope/recursion/filesystem are global; Help/About cannot search. Drafts are
independent between windows. Containing Text remains disabled and has no effect on matching.
See [settings and effective search behavior](settings.md), including date-mode/After/Before details.

Right-click any results header to select Name, In Folder, Size, Type, Modified, Created, Accessed,
Permissions and Full Path. The first five are initially visible; Name always remains visible.
Headers can be resized, dragged to reorder, and clicked to sort. Size and timestamp columns sort
by raw numeric values; Permissions shows Unix mode in octal (including special bits) and sorts
numerically. Missing metadata is blank and sorts before known values ascending. Created is the
filesystem's creation time, never Unix change time. Full Path displays the original GFile parse
name, including its URI for nonlocal files. Selection and external URI drags retain file identity
through column changes and sorting.

Visibility, order, widths and sort persist with window/form state in private, atomically written
`$XDG_CONFIG_HOME/classic-find-plus/settings.ini`. Legacy preference files migrate on first use.
Windows load saved configuration when created and merge changed keys when saving; unchanged
windows do not overwrite newer state on close. Filename history persists its newest 20 unique searches.

## Traversal and lifetime
- Search only the specified root; recursive traversal is optional and on by default. Root directory
  validation and all search filesystem I/O run on workers, not GTK's event thread.
- The explicitly selected root may itself resolve through a symlink. Child symlinks (including
  broken links and folder links) can be results, using link metadata, but are never intentionally
  followed during traversal. Symlink loops therefore do not cause recursive descent.
- Stay on this filesystem defaults on. Different GIO filesystem IDs prevent descent; the mount
  directory itself may still be a result. If the root backend cannot provide IDs for a recursive
  restricted search, the job reports an error. An unknown child ID skips descent and reports an error.
  Same-filesystem bind mounts are not excluded by this option. Direct scans are not security sandboxes;
  a concurrently modified directory tree can change between metadata lookup and enumeration.
- Iterative depth-first traversal keeps an enumerator per active directory depth; it does not
  collect every directory or file before searching. Very deep trees can hit OS descriptor limits;
  affected directories report errors and the job continues where possible.
- One process-wide exclusive pool runs at most two workers. Additional window jobs queue in
  that pool. Each search has its own criteria snapshot, cancellation token, counters and ownership.
- The result queue has at most 256 records. Producers wait for space and cancellation wakes them.
  GTK drains at most 128 records or about 8 ms per 30 ms timer tick, leaving event processing time.
  Full GFileInfo records are released after insertion. The GTK model holds O(number of displayed
  results) compact metadata and original file objects; there is no silent truncation. Millions of
  visible rows still consume substantial memory and require Phase 8 performance evaluation.
- Stop cancels the queued/running job, wakes a blocked producer and retains results already published.
  Final status appears after resource closure and queue drainage. Backend I/O cancellation latency
  depends on the GIO provider; desktop responsiveness does not depend on that latency.
- Closing a window removes its timer, cancels its job and releases UI ownership immediately;
  worker-owned state lives until the job releases its enumerators and returns. No worker touches GTK.
- Permission/enumeration errors increment a count; the first path and error appear in the status
  tooltip. Empty results are normal. Disappearing directories report errors where detected; a result
  is a metadata snapshot, so untracked files deleted after enumeration can remain listed. Recently
  dragged results are verified separately against a copy of the immutable criteria; see
  [results synchronization](results-sync.md). No temporary copies
  or file modifications are performed by search.

## Metadata and date fields

Modified, Created and Accessed select the existing microsecond date-range filter; fixed/relative,
local-zone, time and exclusive/inclusive rules above are unchanged. Created uses real birth data
when the attribute exists, even when it does not match. Only an absent birth attribute falls back
to Modified. Corrupt present timestamps are rejected rather than substituted. Created result
cells always display actual birth data or blank; Accessed depends on relatime/noatime mount policy.
Attribute seconds/fractions/types and calendar bounds are checked before arithmetic. Timestamp
sorting uses raw microseconds, never formatted strings.

Thirteen configurable columns include MIME Type (reported GIO content-type identifier), Detailed
Type (translated GIO description), Group and Owner. No full-file sniffing is added. Type now shows
filesystem kind. Workers cache NSS ownership resolution; missing names use numeric IDs, missing
ownership attributes display blank. Extra columns start hidden and legacy order/visibility survive.

Text is limited to 8192 Unicode characters, paths 32768, date expressions 1024 and numeric inputs
32. Integer sizes and adjustments are checked for conversion overflow. No general punctuation
blacklist is applied to Linux names or regex. Extension expects one suffix and reserves * ? /
for its existing syntax; a backslash may be a literal extension character. Numeric date bases
reject alphabetic insertion while relative bases and optional time/adjustment expressions remain
editable. GTK calendar adjustment text is validated as typed, not silently replaced by an old value.
GLib regex has PCRE match/depth limits of 100000/1000; patterns exceeding work limits can return
nonmatches. This is a practical bound, not a whole-search deadline. Simplify expensive patterns.

Remote GIO backends are asked for provider owner/group names. Remote numeric IDs are never
resolved against the local machine's NSS database; missing remote names remain numeric.

Find Plus Allow admin search uses the same effective filters and captures relative expressions
before authentication. Other tabs ignore it; see [administrator searches](admin-search.md).
