# Exclusion and exact sizes (1.0.3)

Find Plus offers one Exclude expression below File name. It is restored without starting a
search, shares filename mode and case sensitivity, and remains active with More Filters
collapsed. Other search tabs retain but ignore it, including validation. Empty disables it.

In Partial / wildcard mode plain text is a substring; * or ? makes a whole-name glob. Exact
name matches a literal whole component. RegEx uses the existing bounded GLib Perl-compatible
matcher. An ordinary exclusion applies to both files and directories. A leading / instead
selects directories: it is neither an absolute path nor a regex delimiter. Simple literals
use directory prefixes, including Exact mode: /new matches newshows but not brandnew.
Wildcard and RegEx expressions retain their pattern semantics after removing the indicator.
Slash alone and invalid active RegEx report errors before replacing any results. Only one
expression is supported; semicolons have no special meaning. Extension-only inclusion does
not change exclusion: exclusion always uses the full component name.

Examples in Partial / wildcard mode:

| File name | Exclude | Effect |
| --- | --- | --- |
| *.mp4 | /movies | Skip movies and movies_old directories at any depth and their contents |
| *.txt | backup | Skip matching files and directories containing backup |

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
