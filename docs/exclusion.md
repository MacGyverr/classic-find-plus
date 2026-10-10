# Exclusion and exact sizes (1.0.5)

Find Plus offers one Exclude field below File name. It is restored without starting a
search, shares filename mode and case sensitivity, and remains active with More Filters
collapsed. Other search tabs retain but ignore it, including validation.

In Partial / wildcard and Exact name modes, unescaped `|` separates independent rules.
Rules combine with OR: any matching rule excludes the entry. Empty terms are ignored:
`/Movies||/Pictures|` equals `/Movies|/Pictures`, `|cache` equals `cache`, and `|||`
imposes no exclusion. Spaces are significant, including leading/trailing spaces; a
space-only rule is not empty. Colons and semicolons are ordinary filename characters.

Non-RegEx escapes:

- `|` separates rules.
- `\|` matches a literal pipe.
- `\\` matches a literal backslash.
- `\x` for unrelated x retains both characters, without arbitrary escape conversion.

Consecutive backslashes are decoded in pairs. An odd run escapes the following pipe;
an even run leaves the pipe as a separator. A trailing unrelated backslash is retained.
UTF-8 is preserved. Parsing and matcher compilation occur once per captured search.
The existing 8192-character bound permits at most 4096 nonempty rules (each needs a
character plus separators); there is no smaller arbitrary rule-count cap. Oversized or
invalid UTF-8 input is rejected before search. Copies share immutable compiled rules.

Plain text in Partial / wildcard is a substring; * or ? makes a whole-name glob. Exact
name matches a literal whole component. An ordinary rule applies to files and directories.
A leading / on each rule independently selects directories: it is neither an absolute
path nor a regex delimiter. Simple directory literals use prefixes, including Exact mode:
/new matches newshows but not brandnew. Wildcard patterns retain whole-name semantics
after the slash. Slash alone remains an error. Extension-only inclusion never changes
exclusion: exclusion always uses the full component name. Filename inclusion does not
use the new OR grammar.

RegEx mode bypasses this parser entirely. The whole field is one bounded GLib
Perl-compatible expression, with its original escapes, colons, spaces and native alternation.
Examples: `Movies|Pictures|cache`, `(?:Movies|Pictures)`, `[a:z]+`, `file:old`, `file\|old`.
The existing optional leading / applies to that whole regex, making it directory-only;
patterns continue matching component basenames, not full paths. Invalid active RegEx
reports an error without replacing results. Empty regex alternation branches retain their
native meaning (unlike empty non-RegEx terms); `|||` can match every candidate in RegEx.

Examples in Partial / wildcard mode:

- `/Movies|/Pictures|cache`: prune Movies/Pictures directory prefixes and any file or
  directory containing cache.
- `/backup|*.tmp|*.bak`: prune backup directory prefixes and exclude tmp/bak names.
- `/My Movies|old files`: preserve spaces while matching directories and names.
- `file\|old|cache`: match literal file|old or cache substrings.
- `folder\\name|cache`: match a literal backslash in folder\name or a cache substring.

The original complete expression is saved exactly as entered; switching modes never
rewrites it. Worker protocol v3 transports that raw text and mode to the same parser,
with existing length/schema validation and read-only restrictions. Older peers fail with
a protocol-version error instead of silently dropping the new semantics.

The worker compiles matching criteria once, copies their ownership safely, and checks each
entry before descent, enumeration, or result publication. Other enabled filters combine by
logical AND. The selected root is exempt even if its basename matches. Nonrecursive searches
filter immediate entries without descending. Follow symbolic links defaults off; when enabled,
pruning checks both lexical directories and final resolved target ancestors before descent.
A directory-only expression treats a traversed link target as a directory; the link row still
uses its original metadata. No Hidden files applies the same subtree policy to dot-names.
The selected root remains exempt. See [alias ordering and target scope](search.md).
Synchronization checks captured exclusions/hidden/Follow rules on moved destinations and
ancestors without rescanning the tree.

Traversal instrumentation verifies two directory enumeration attempts with /movies versus
six without it, two pruned directories, and no examination of files inside excluded subtrees.

Right-click a result header to enable initially hidden Bytes and Bits. Size remains the
existing human-readable column. Bytes and Bits are exact, comma-grouped integers without
units; Bits is bytes multiplied by eight. Portable decimal-digit arithmetic supports all
unsigned 64-bit byte values through 147,573,952,589,676,412,920 bits. No floating point or extra
filesystem calls are used. Both sort by existing unsigned raw byte size, in either direction.
Directories and missing sizes have blank exact cells. Fifteen-column layouts save visibility,
order, width and sort in the existing settings; nine/thirteen-column layouts migrate with
Bytes/Bits hidden. Per-key locked merging preserves unrelated settings across windows.

Visible Bytes/Bits export exactly as displayed; quoted CSV protects embedded commas and TXT
retains literal tab delimiters. Bits means logical file size, not transfer traffic or time.
Protocol overhead, compression and transfer speed affect real transfers.
