# Build, installation and removal details

Use one installation method per prefix. Package-manager installs normally use `/usr`; source
installs default to `/usr/local` or `~/.local`. A different prefix can coexist but may shadow
another version through PATH and desktop action lookup. Do not overwrite packaged files.

## Installed paths

Both build systems install the same functional files under PREFIX:

- `bin/classic-find-plus`, plus compatibility symlink `bin/classic-find`.
- `share/applications/org.classicfind.Plus.desktop`.
- `share/nemo/actions/classic-find-plus.nemo_action`.
- `share/icons/hicolor/16x16/apps/classic-find-plus.png`.
- `share/doc/classic-find-plus/`: license, README/readme, changelog, asset notices and public docs.

Artwork is also embedded as GResources. No runtime source/build directory is needed.
No schemas or translations are currently generated. Make additionally stores its private
ownership manifest at `PREFIX/share/classic-find-plus/source-install.json`.

Nemo reads `$XDG_DATA_HOME/nemo/actions` (normally `~/.local/share/nemo/actions`) and
`nemo/actions` beneath XDG_DATA_DIRS. For a custom prefix, add its `bin` to PATH and `share`
to XDG_DATA_DIRS before starting your desktop/Nemo session. If XDG_DATA_HOME overrides
`~/.local/share`, a default user-local install is not automatically visible there. Use a
matching data root or configure the session; the Make installer deliberately does not
write action files outside PREFIX. Launchers resolve the executable through PATH.

## Make safety

Make compilation needs GNU Make, a C11 compiler, pkg-config and GTK/GLib tools only. Its
conservative install helper additionally requires Python 3, available on the target systems.
The helper uses an explicit path plan; paths must remain under an absolute non-root PREFIX.
DESTDIR must be an absolute non-root staging directory. Both may contain spaces/Unicode; package-query glob characters (* ? [ ]) are refused.
Existing symlink ancestors, unknown files and modified installed files cause refusal.

Before install or uninstall, every planned path is checked against current dpkg and/or RPM
databases when present. An unreadable database/checking failure also refuses the operation.
DESTDIR checks only package databases within the staging root, never the host database for
staged logical paths. This permits packaging without writing host manifests or state.
Keep DESTDIR consistent during removal. Native package installation/removal is always the
package manager's job. Source ownership manifests never override package-manager ownership.

The manifest records hashes or the exact compatibility-symlink target. Uninstall removes
only unchanged tracked files, after rechecking ownership/content. It never recursively removes
a directory or touches user preferences. Unrelated files remain. Empty immediate install
directories may be removed; shared parent directories are retained.

After source changes, rerun `make`, then install using the same PREFIX to upgrade a tracked
unchanged installation. Do not copy another binary over it manually: changed-file detection
will refuse later removal. If a file was intentionally customized, back it up and restore
the previously recorded content, or review and remove that specific file manually. Never
edit the manifest to bypass ownership checks. If a package owns any path, use its manager.

Temporary copies are prepared beside destinations and individually renamed into place.
Progress is recorded as files are installed. A multi-file installation is not globally atomic;
a power failure/interrupt can leave a partial installation. Stop concurrent installers, inspect
the manifest and affected files, and rerun the same installer to recover tracked unchanged
files. A kill between rename and manifest save can require manual review of one untracked
file. Filesystem races by another process with the same write authority cannot be eliminated;
do not install into a tree writable by untrusted users. Package and source operations must not
run concurrently. No password or privileged helper is implemented.

`make clean` removes only known Make outputs, not Meson builds, sources, settings or unrelated
files in build-make. Standard compiler flags and parallel builds are supported. Changing flags
requires `make clean` before rebuilding; header changes are detected through generated .d files.

## Meson behavior

Meson >=0.61 supports the used resource/install APIs. Fresh debug and release configurations
are supported; use a separate build directory for each. Python/Ninja are part of Meson's own
toolchain. Development test definitions are loaded only when the private tests directory
exists; a public clone compiles the same application without tests.

`meson install` uses native Meson installation and supports DESTDIR and prefix configuration.
It does not create a Make ownership manifest. Meson's uninstall target is not CFP's safe source
uninstaller: it can remove files without checking current package ownership. Do not use it on
a live installation. For tracked source removal, choose Make from the start; for Meson, stage
into DESTDIR and create a native package. CFP does not promise safe automatic removal of a
previous untracked Meson installation; review its install log and package ownership manually.

Native DEB builds: `sh scripts/build-deb.sh` creates a release archive under dist without host
installation. Its system-library requirements match the build machine. Build on the intended
distribution for compatibility; see [packaging](packaging.md). RPM packaging is not supplied;
Fedora/openSUSE users can build natively with Make/Meson or create a distro-native package.
