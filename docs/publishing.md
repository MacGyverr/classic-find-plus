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

## Native Linux maintainer workflow

The private development workspace provides `scripts/publish-to-linux.py`; keep it there,
alongside `publish-to-windows.ps1`. Both read the same `public-files.txt` and compatible
`.cfp-publication.json` ownership hashes. Neither tool is installed with CFP or required
to build the public source. Native Linux needs Python3 and Git; Windows uses PowerShell.

```sh
python3 scripts/publish-to-linux.py --preview
python3 scripts/publish-to-linux.py --sync
```

Native defaults use the script's development root and `~/Documents/GitHub/classic-find-plus`.
Use `--source` and `--destination` for separate disposable/workspace paths. The destination
must already be the root of a normal Git working tree. Missing roots, filesystem roots,
nested/same roots, symlinks and unsafe/private manifest entries are refused. No Git metadata,
branch, remotes, index, commits or pushes are changed by the tool. Preview creates no lock,
staging or state files. Warnings identify current source files that would overwrite local
or staged edits; exact `YES` confirmation is required by `--sync`. As with the Windows
publisher, approved source wins after confirmation. Preserve local edits separately first.
Unrelated files and modified obsolete files stay; only unchanged previously owned obsolete
files can be deleted. Paths are passed directly, without shell evaluation.

New/updated bytes are staged and hash-checked before applying files. Each replacement is
atomic and per-file progress is saved privately. A failure can leave a partly completed
publication; review reported applied files and rerun Preview before confirming a resume.
Normal failures clean their staging/lock; forced termination may leave a lock or staging
folder, which is deliberately refused/preserved for manual review. This is not a global
rollback transaction. Do not remove a lock while a publisher is still active. Builds never
invoke either publisher. Synchronizing a real working repository remains a separate explicit
maintainer action; review changes in GitHub Desktop before deciding to commit/push.
