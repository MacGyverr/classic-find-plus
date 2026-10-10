# Classic Find Plus

<img width="1518" height="1049" alt="find-plus" src="https://github.com/user-attachments/assets/421cba10-afb0-402b-a8c0-1b21582c639b" />

<img width="1521" height="1053" alt="find-plus2" src="https://github.com/user-attachments/assets/93e56d66-e61b-400e-97b8-0c0035f0e6ef" />

<img width="755" height="624" alt="name" src="https://github.com/user-attachments/assets/77b30b42-4f96-40ed-984f-ea4b44a5df83" />

<img width="759" height="621" alt="date" src="https://github.com/user-attachments/assets/730cb725-4ed4-4699-b77a-8d680503376d" />

<img width="758" height="624" alt="advanced" src="https://github.com/user-attachments/assets/695cbb1c-bb1f-4c93-b938-6910afbc4408" />

**Version 1.0.4** — a native Linux file and folder search application, inspired by the classic
Find dialog and styled by your GTK3 desktop theme. Created by MacGyverr / ChatGPT.
[GitHub project](https://github.com/MacGyverr/classic-find-plus) · GPL-3.0-or-later.

Search by filename, extension, type, size and modified/created/accessed date. Four search tabs
share saved values, and Help/About explains patterns and shortcuts. Find Plus combines filters
and offers one Exclude expression that skips matching directories before entering them.
Results support fifteen configurable columns, exact Bytes/Bits, sorting, file actions,
multi-selection, drag-and-drop into Nemo, and CSV or tab-delimited TXT export.

Linux Mint with Nemo is the primary platform. GTK3 runs on other Linux desktops with an X11
or Wayland session; optional menus support Nemo, Nautilus, Dolphin, Caja and Thunar. Debian WSL2 is also used for
development verification. Fedora, Arch and openSUSE native builds are documented but have not
been tested on those distributions. No Windows-native application is provided.

## Install a Debian package

Download the DEB for **your distribution and architecture** from the project's Releases page
when available. Building 1.0.4 does not imply its DEB has been published.
Mint 21.3 uses Ubuntu 22.04 (jammy) libraries; a DEB built on newer Debian may not work there.
Double-click the downloaded DEB to use your graphical package installer, or run:

```sh
sudo apt install ./classic-find-plus_VERSION_ARCH.deb
```

Replace that example filename with the actual downloaded file. Install a newer matching DEB
the same way to upgrade. In Synaptic, find **classic-find-plus** and mark it for upgrade/removal;
Synaptic can upgrade only when the newer package is available through a configured repository.
For a downloaded DEB, use the graphical installer or apt command above.

```sh
sudo apt remove classic-find-plus
# Remove package-managed configuration too:
sudo apt purge classic-find-plus
```

Both retain per-user preferences. To reset them, remove your own
`~/.config/classic-find-plus` separately after backing it up. Avoid mixing DEB/RPM and source
installs at the same prefix. Plain root GUI launches are refused; the explicit administrator override is documented below.

## Install build prerequisites

Choose the traditional or modern toolchain below. Both need a C11 compiler, pkg-config,
GTK3 >=3.22, GLib/GIO >=2.70 and `glib-compile-resources`. Python 3 supports the conservative
Make install/uninstall helper; it is not required to compile with Make. Meson >=0.61 and
Ninja are needed only for the modern build. No dependencies are downloaded during compilation.

| Distribution | Traditional Make prerequisites | Add for Meson/Ninja |
| --- | --- | --- |
| Debian / Ubuntu / Mint | `sudo apt install build-essential pkg-config libgtk-3-dev libglib2.0-dev libglib2.0-dev-bin python3` | `sudo apt install meson ninja-build` |
| Fedora | `sudo dnf install gcc make pkgconf-pkg-config gtk3-devel glib2-devel python3` | `sudo dnf install meson ninja-build` |
| Arch Linux | `sudo pacman -S --needed base-devel pkgconf gtk3 glib2 python` | `sudo pacman -S --needed meson ninja` |
| openSUSE | `sudo zypper install gcc make pkg-config gtk3-devel glib2-devel python3` | `sudo zypper install meson ninja` |

Package names were checked against installed Debian metadata and official
[Fedora](https://packages.fedoraproject.org/pkgs/gtk3/gtk3-devel/) and
[Arch](https://archlinux.org/packages/extra/x86_64/gtk3/) package indexes. openSUSE package
availability varies with release; use `zypper search` or capability `pkgconfig(gtk+-3.0)` if
necessary. Install a release providing the minimum library versions; older distributions
cannot be supported just by adding a compiler.

Runtime: GTK3/GLib and their normal system dependencies, a desktop icon theme and PNG loader.
SVG icons in system themes also require the distribution's SVG pixbuf loader (for example
`librsvg2-common` on Debian/Mint). Python 3 supports integration management and asynchronous NSS account enumeration. D-Bus, Polkit and an active desktop authentication
agent enable administrator searching; GVfs/KIO backend support governs administrative transfers.
Nemo and other managers are optional. Ordinary local searching does not need the privileged service.
See [administrator setup](docs/admin-search.md) and [optional adapter dependencies](docs/integrations.md).

## Build with GNU Make

```sh
make -j4
./build-make/classic-find-plus
sudo make install                 # default PREFIX=/usr/local
make clean                        # Make outputs only
sudo make uninstall               # same PREFIX/DESTDIR as installation
```

User-local installation:

```sh
make install PREFIX="$HOME/.local"
# Ensure ~/.local/bin is on PATH, then launch:
classic-find-plus
make uninstall PREFIX="$HOME/.local"
```

Run `make` again after source changes. Header dependencies are tracked automatically.
`CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `PKG_CONFIG`, `PREFIX` and `DESTDIR` are supported.
For packaging: `make install PREFIX=/usr DESTDIR="/absolute/staging/path"`.
Make directly compiles C and resources; it does not call Meson or Ninja.

Make removal uses its installation manifest **and current dpkg/RPM ownership checks**. It
refuses package-owned, untracked or changed files, and does not remove preferences. Keep the
source directory for removal. See [installation safety and recovery](docs/install.md).

## Build with Meson/Ninja

```sh
meson setup build --buildtype=release --prefix=/usr/local
meson compile -C build
./build/classic-find-plus
sudo meson install -C build
meson compile -C build --clean
```

For a user installation configure `--prefix="$HOME/.local"`, then install without sudo.
For staging use `DESTDIR="/absolute/staging/path" meson install -C build --no-rebuild`.
Run `meson compile -C build` after changes. Debug builds use `--buildtype=debug`.
The public tree does not contain or require development tests.

Meson's generated uninstall target does not implement CFP's package ownership checks. It is
**not a supported safe removal route** for a live installation. Prefer Make for a source
installation that needs tracked uninstall; use Meson for building/staging, or build a native
package with `sh scripts/build-deb.sh` and remove through the package manager. Never use Make
uninstall on a Meson installation. See [install paths and removal](docs/install.md).

## Administrator searches and root launch

Find Plus offers **Allow admin search**, initially unchecked on every launch and never saved.
Checking does not authenticate. Find validates/captures filters and requests Polkit permission;
other tabs stay ordinary. The separate temporary worker returns matching metadata while the
GTK application remains your ordinary user. Failure/denial/cancel never silently falls back.
Stop/Esc or window closure cancels; status identifies administrator searches.

Protected results may be visible but still inaccessible for ordinary opening, copying, moving
or export. Those operations do not inherit worker privileges. All existing functional filters,
columns, sorting, history, selection and drag-and-drop use the same application code.

```sh
classic-find-plus                      # ordinary user
sudo classic-find-plus                 # refused
sudo classic-find-plus --allow-root    # entire GUI intentionally runs as root
```

The override displays Administrator Mode and a startup warning, uses a separate root
configuration/cache/data profile and leaves every feature available with root permissions.
It can change system files. It requires authorized graphical-session access and never weakens
X11/Wayland access controls. Prefer the checkbox when only searching needs elevation.
[Authorization, worker limits and setup](docs/admin-search.md).

## Optional menus and administrative transfers

Fresh installations enable no file-manager menus. Open **Help/About → File Manager Integrations**,
Refresh, Install the desired adapter, then Enable it. Disable/Remove affects only owned CFP
entries. Optional packages are reported and never installed silently. Existing Nemo integration
and saved user choices survive upgrades. Menus pass the selected folder ahead of saved Look in.
Nautilus uses a native Python extension, Caja its Scripts menu, Dolphin a service menu, Thunar a safe custom action.
Restart/reload requirements and CLI equivalents are in [integration instructions](docs/integrations.md).

Linux Mint workflow:

1. Open Nemo and press F3 for split panes.
2. Open a normal folder in one pane.
3. In the destination pane press Ctrl+L and enter `admin:///opt/` or another protected folder.
4. Authenticate when prompted.
5. Launch Classic Find Plus from the normal folder and drag selected results into the administrative pane.

Nemo and the installed GVfs/Polkit components manage authentication and transfers. Support
varies by distribution/session; ordinary drops onto protected local paths may fail. Keep CFP
running as your regular user. Ctrl requests copy, Shift requests move; the receiver decides.
[Permissions and limitations](docs/permissions.md) · [File actions](docs/file-actions.md).

## Searching, results and settings

Partial / wildcard uses substring matching until `*` or `?` appears, then matches the whole
filename: `*.txt`, `report*`, `h?ll0.txt`. Exact name treats wildcards literally. RegEx uses
GLib Perl-compatible expressions; `^report-[0-9]{4}\.txt$` matches `report-2026.txt`.
Aa controls case sensitivity. Invalid active criteria leave existing results intact.
Find Plus Exclude shares that mode/case: `*.mp4` with `/movies` skips matching directory
subtrees; `*.txt` with `backup` excludes matching files and directories. Other tabs ignore it.
Dates accept fixed/relative calendar expressions; Created falls back to Modified only when
birth metadata is unavailable. Containing Text is a disabled placeholder, not content search.

Find Plus has a compact two-column **More Filters** section. Check each optional filter to
activate it: File type, Extension, Date mode, Group, User and exact Permissions.
**No Hidden files** skips dot-prefixed files and prunes hidden directory subtrees. Group/User
lists include root and service accounts; matching uses numeric ownership. Permissions accepts
three/four octal digits, including special bits (for example `644`, `4755`, `1777`).
Collapsing More Filters resets all its values and checkboxes; every new window starts with
them off, even if the section opens expanded. Main criteria, other tabs and running searches
retain their values. Extension combines with File name and needs no second checkbox.

**Date Mode** offers Any date or Last N minutes/hours/days/months. Previous N enables
itself for an active relative mode; enter a positive whole number. Main After/Before and
Last N combine with AND, using the same Modified/Created/Accessed field. One reference
instant is captured when Find is pressed, before authentication, and both interval edges
are inclusive. Minutes/hours/days are elapsed time (a day is 24 hours); months use calendar
subtraction with month-end clamping. Date Modified retains Any, After, Before, Between,
After and before, plus all four Last N modes. Inactive N is ignored; disjoint date bounds
show a validation error. [Limits and examples](docs/search.md#rolling-relative-time).
Accessed depends on relatime/noatime mount options.

**Follow symbolic links** is saved independently of More Filters and defaults off. When on,
directory identities prevent cycles/duplicate descent; Stay on this filesystem checks actual
targets. Excluded/hidden target subtrees remain pruned. Link rows retain link metadata and
children retain the chosen alias path. [Traversal and filter details](docs/search.md).

Right-click a result header to show columns, drag headers to reorder, and click to sort.
Bytes and Bits are initially hidden, exact comma-grouped integers; Bits is bytes times eight,
not network traffic or transfer time. Select with Ctrl/Shift or Ctrl+A. Results update after
verified recent drags; rerun Find for uncorrelated changes or long authentication delays.

Export results is enabled with displayed rows and disabled while writing. It saves all
click-time rows and visible columns in current order/sorting, regardless of selection.
CSV quotes every field; TXT uses tabs without field quotes and normalizes embedded tabs/newlines.
Both are UTF-8, protect spreadsheet formula prefixes, and use a native overwrite prompt.
Only local or mounted output destinations are supported. See [export](docs/export.md).

Preferences, criteria, history, column layout and window geometry persist in
`$XDG_CONFIG_HOME/classic-find-plus/settings.ini` (default `~/.config/classic-find-plus/settings.ini`).
Startup and tab changes do not search automatically. Separate windows merge changed settings.
X11 restores normal window position; Wayland leaves placement to the compositor.
[Search details](docs/search.md) · [Settings](docs/settings.md) · [Exclusions](docs/exclusion.md).

## Troubleshooting and limits

- Missing `gtk/gtk.h` or pkg-config dependency errors: install the development packages, not
  just runtime libraries. Check `pkg-config --modversion gtk+-3.0 glib-2.0`.
- Missing `pkg-config` or `glib-compile-resources`: install pkg-config/pkgconf and GLib tools.
- Context menu absent: Refresh integrations in Help/About; check enabled status, optional
  dependencies, PATH and the documented restart requirement.
- Desktop launcher absent: ensure `PREFIX/share` is in XDG_DATA_DIRS; refresh the session.
- Permission denied: choose a writable location or an authenticated Nemo destination.
- Wayland position not restored: global placement is controlled by the compositor.
- Local source removal refused: preserve changed files, check package ownership, and follow
  [recovery instructions](docs/install.md); do not bypass the checks by deleting a prefix.

This is Linux/POSIX software. No hardcoded CPU architecture is required for native builds;
only amd64 Mint-compatible DEB tooling with an extracted SDK has been verified. Other CPU
architectures are untested. The admin syscall guard supports Linux x86-64/AArch64; AArch64
is unverified and other worker architectures refuse startup. No content search, recursive
folder-size calculation or guaranteed remote-backend authentication is provided. Searches keep matching rows
in memory; exports briefly snapshot visible data. Backend calls can delay cancellation.
Directory symlinks are followed only when Follow symbolic links is enabled; safe following
requires native local/mounted paths with stable directory identities. See docs/search.md.

No screenshots are bundled in this release; application artwork is included with permission.
The complete license is [LICENSE](LICENSE); artwork/system-library notices are in
[THIRD_PARTY.md](THIRD_PARTY.md). See [CHANGELOG.md](CHANGELOG.md) for release history.
