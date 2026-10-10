# Public source manifest

`public-files.txt` is an explicit file-by-file allowlist. Unknown files are excluded until a
maintainer adds them. `sources.txt` is the single application C-source list used by GNU Make
and Meson; keep it in sync when adding/removing modules. VERSION controls both binary versions.
Debian packaging also checks the corresponding version in debian/changelog.

To make a public-only source copy, choose a new destination:

```sh
python3 scripts/export-source.py /absolute/path/to/new-public-source
```

The exporter refuses an existing destination and symlink paths. It copies only listed regular
files. It never reads Git metadata or commits/pushes. Before release, verify the exported tree
with fresh Make and Meson builds and DESTDIR installs, not just the development checkout.

The public tree intentionally excludes development tests, private project state, handoff files,
engineering audits, editor settings, build caches, binaries and package archives. They remain
in the development workspace. No test files are needed to build or install the public source.
Do not add generated resource C, config.h, object files, DEBs or credentials to the allowlist.

A separate maintainer PowerShell synchronization tool is kept in the development workspace;
it previews add/update/delete changes, requires explicit confirmation for a sync, and records
ownership/checksums before subsequent deletions. It is not needed to build this public source.
Publishing to a real Git repository, committing and pushing remain explicit maintainer actions.

During synchronization the approved source wins for every current allowlisted file. Local
tracked edits (including staged edits), changed previously published files, and existing
unowned destination files generate warnings but do not block copying. Preview still writes
nothing, and `-Sync` still requires typing `YES`. Back up any local edits you want to keep
before confirming. Unrelated destination files and Git metadata are untouched. Changed
obsolete files are retained with a warning; unchanged previously published obsolete files
are listed for deletion. Unsafe paths, junctions, unreadable inputs and files changing
after preview still stop synchronization.
