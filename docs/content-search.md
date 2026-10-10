# Text and hexadecimal content search

Find Plus adds **Containing text** as an AND filter after filename, Exclude and
metadata filters. The other three search tabs retain but ignore this value.
An empty entry disables content scanning, including with Hex checked. Excluded
and hidden subtrees are pruned before enumeration. Press Find to validate/start;
editing or toggling Hex does not start a search or rewrite the entry.

With **Hex off**, enter literal ASCII or UTF-8 text, for example `connection refused`
or `error.*failed`. Wildcard and RegEx punctuation is literal here, independently
of filename matching mode. Aa on matches exact UTF-8 bytes. Aa off uses Unicode
case folding (for example STRASSE matches Straße), preserving characters split
across read buffers. Malformed UTF-8 bytes and NUL delimit valid spans and prevent
case-insensitive matches bridging those bytes. Case-sensitive text and Hex scan
all bytes without decoding. Unicode normalization, UTF-16/32 and legacy encodings
are not supported. Accents must have the same Unicode representation.

With **Hex on**, `48656C6C6F20776F726C6421` matches the 12 bytes of `Hello world!`.
`48 65 6c 6c 6f`, tab-separated complete pairs, mixed-case digits and contiguous
pairs are accepted. `00 FF 10 A2` includes NUL and high-bit bytes; `7F 45 4C 46`
can find an ELF executable signature. Aa does not affect Hex. Odd digits, invalid
characters, `0x` prefixes, newlines, spaces inside a byte pair and nonempty
whitespace-only patterns are rejected when Find is pressed. Partial edits are allowed.

The pattern limit is **64 KiB**, measured as decoded Hex bytes or UTF-8 text bytes
(including case-folded bytes in insensitive mode). Raw Hex input is limited to
256 KiB including spaces/tabs. The entry value uses normal saved search settings;
Hex is never saved and starts unchecked in every new window/application launch.
Review a restored Hex expression before searching, because it starts in text mode.
Collapsing More Filters does not clear content text or Hex.

## Streaming and limitations

All eligible regular files are searched regardless of MIME type, extension or
reported size, including binary files, executables, images, archives and videos.
This searches raw stored bytes; CFP does not extract visible text from PDFs, Office
files, archives or compressed data. There is **no automatic source-file size cutoff**.
Use Size filters to limit candidates if desired. A missing pattern in a large file
can require reading the whole file. A growing file can extend the scan.

The background scanner uses fixed 32 KiB buffers and a compiled streaming KMP table.
Patterns up to 64 KiB, including embedded NUL and patterns longer than a buffer,
match across buffer boundaries. Unicode folding uses bounded valid spans and at
most three carried bytes. Memory is bounded independently of file size. Each file
closes immediately after its first match; no matching snippets are stored. Stop/Esc
checks cancellation before opening, between reads and bounded processing batches,
closes descriptors and preserves partial results. The existing two-thread search
pool is retained. A kernel/network/FUSE call blocked in I/O may delay cancellation;
CFP never forcibly terminates ordinary threads. Errors use the existing status tooltip.
Observed truncation, modification or unlink during a read causes that file to be skipped;
results are snapshots, not transactional guarantees about concurrently changing files.
Reading can update access times according to filesystem mount settings.

## File and privilege safety

Content scanning permits only verified regular files. Devices, FIFOs, sockets,
directories and unknown nonregular types are skipped. Linux O_PATH pins the inode
without opening a device data stream. fstat/fstatfs checks precede data opening
through the pinned descriptor; the data descriptor is checked again for identity,
type and filesystem. Resolved pinned parents are checked against captured pruning
rules and the selected root filesystem before data opening. This avoids reopening a mutable source path. If /proc/self/fd
is unavailable, the content read fails closed. Follow symbolic links defaults off;
when enabled, regular targets are eligible, subject to traversal restrictions.
Aliases to devices and kernel interfaces remain ineligible, even in full-root mode.

Actual filesystem types exclude procfs, sysfs, debugfs, tracefs, securityfs, configfs,
cgroup v1/v2, BPF, efivarfs, pstore, nsfs and other kernel control interfaces. This
recognizes alternate mount locations. No directory-name blacklist is used: an
ordinary file in a directory named proc and a regular /dev/shm tmpfs file remain
eligible. Filename/metadata searching of /proc, /sys, /dev and /run remains available
with empty content. Mounted network and FUSE regular files remain eligible; their
implementations may block or behave unusually. Classification cannot guarantee
harmless or immediately interruptible reads on every filesystem. Non-native GVfs
URIs cannot use content scanning; mount them normally first.

Ordinary searches use your permissions. Allow admin search explicitly authorizes
the read-only Polkit worker before protected content is scanned. Full-root
`--allow-root` uses the same content protections. Protocol v4 validates content
mode, encoding and limits and returns paths, existing metadata, match counts and
errors only; no content, snippets or arbitrary byte-reading endpoint is exposed.
Even matching presence in protected files reveals information, so this capability
requires administrator authorization. The worker retains its existing 512 MiB
memory, 64-descriptor, 120 CPU-second and 15-minute lifetime limits; these resource
limits can stop exceptionally expensive scans and are not file-size cutoffs.
Result synchronization runs in the ordinary GUI's background task; permission
errors preserve protected rows rather than silently elevating or deleting them.
