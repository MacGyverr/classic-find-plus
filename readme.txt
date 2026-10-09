Classic Find Plus 1.0.0
Created by MacGyverr / ChatGPT. GPL-3.0-or-later; see LICENSE and THIRD_PARTY.md.
Project: https://github.com/MacGyverr/classic-find-plus

Native Linux GTK3 file and folder search with Nemo integration. See README.md for dependency
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
and the desktop session. Do not launch the GUI with sudo.

Find Plus Exclude is one saved expression sharing filename mode/case. A leading slash
selects directory components; matching subtrees are skipped before enumeration. Example:
File name *.mp4, Exclude /movies. Other tabs ignore Exclude. Bytes/Bits show exact grouped
integers and sort numerically. Bits is logical file size, not network traffic or transfer time.
Export results saves visible columns and all click-time rows in current order/sorting as
UTF-8 CSV or tab-delimited TXT. It is disabled with no rows or while writing. Help/About
explains wildcard/RegEx usage, dates, shortcuts and other features.
