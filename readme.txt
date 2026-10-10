Classic Find Plus 1.0.2 release candidate
Created by MacGyverr / ChatGPT. GPL-3.0-or-later; see LICENSE and THIRD_PARTY.md.
Project: https://github.com/MacGyverr/classic-find-plus
Administrator search and the explicit root GUI work on the owner's Linux Mint 21.3 desktop.
Real desktop denial/cancellation remains unreported; see docs/admin-search.md.

Native Linux GTK3 file and folder search with optional Nemo/Nautilus/Dolphin/Caja/Thunar integration. See README.md for dependency
commands, GNU Make and Meson builds, features and troubleshooting.

DEB installation/update: download the matching package and double-click it, or run
  sudo apt install ./ACTUAL_DOWNLOADED_FILENAME.deb
Use the same command with a newer DEB to upgrade. In Synaptic, search classic-find-plus,
mark for removal or upgrade, and apply. Synaptic upgrades require a configured repository
providing that version; downloaded local DEBs use the graphical installer or apt.

CLI removal:
  sudo apt remove classic-find-plus
or, to purge package-managed configuration:
  sudo apt purge classic-find-plus
Per-user settings remain at ~/.config/classic-find-plus (or XDG_CONFIG_HOME).

Source build/install:
  make -j4
  sudo make install
  make clean
  sudo make uninstall
Default PREFIX is /usr/local. User-local: make install PREFIX="$HOME/.local"; remove with
make uninstall PREFIX="$HOME/.local". Keep the source and use the same prefix. Python 3 is
required for Make install/uninstall. Removal refuses modified, untracked and package-owned
files. Do not mix package-manager and source installations. See docs/install.md.
Meson builds/staging are supported; its generic uninstall target is not a safe live removal
route with package ownership checks. Prefer Make or a native package for removable installs.

Linux Mint workflow:
Open Nemo, F3 split panes, keep a normal source folder, Ctrl+L in the destination pane,
enter admin:///opt/, authenticate, open CFP from the normal folder and drag results into
the administrative pane. CFP stays a regular-user app. Support depends on Nemo, GVfs/Polkit
and the desktop session. Plain sudo GUI launch is refused.

Find Plus Exclude is one saved expression sharing filename mode/case. A leading slash
selects directory components; matching subtrees are skipped before enumeration. Example:
File name *.mp4, Exclude /movies. Other tabs ignore Exclude. Bytes/Bits show exact grouped
integers and sort numerically. Bits is logical file size, not network traffic or transfer time.
Export results saves visible columns and all click-time rows in current order/sorting as
UTF-8 CSV or tab-delimited TXT. It is disabled with no rows or while writing. Help/About
explains wildcard/RegEx usage, dates, shortcuts and other features.

Administrator searching: Find Plus > Allow admin search starts unchecked on every launch.
Polkit authentication happens only on Find. Other tabs remain ordinary searches; cancellation
or denial does not fall back. Results do not elevate file actions, exports or drag-and-drop.
The worker accepts local/mounted folders and uses the full existing filters, not content search.
See docs/admin-search.md for installation, authorization and resource limits.

Root CLI modes:
  classic-find-plus                     (ordinary user)
  sudo classic-find-plus                (refused)
  sudo classic-find-plus --allow-root   (explicit full-root GUI)
Administrator Mode shows a warning/title and isolates root settings/cache/data. Every feature
has root authority, including exports and file changes. Graphical session access may be refused;
CFP never changes X11/Wayland access controls. Prefer search-only elevation when possible.

Fresh installs enable no context menus. Help/About > File Manager Integrations > Refresh,
Install, then Enable the desired adapter. Disable/Remove preserves unrelated configuration.
Existing legacy Nemo choices survive upgrades. Optional dependencies are not installed by CFP.
Restart the affected manager after changes; see docs/integrations.md for details and CLI controls.
For the old Caja extension, click Switch to Scripts, restart Caja, then right-click a local
folder or its background and choose Scripts > Find with Classic Find Plus.
