Classic Find Plus 1.0.7
Created by MacGyverr / ChatGPT. GPL-3.0-or-later; see LICENSE and THIRD_PARTY.md.
Project: https://github.com/MacGyverr/classic-find-plus
Administrator search and the explicit root GUI work on the owner's Linux Mint 21.3 desktop.
Real desktop denial/cancellation remains unreported; see docs/admin-search.md.

Native Linux GTK3 file and folder search with optional Nemo/GNOME Files (Nautilus)/Dolphin/Caja/Thunar integration. See README.md for dependency
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

Nemo, GNOME Files (Nautilus) and Caja accept CFP drops into administrator-opened
folders on the owner's tested systems. GNOME Files is Nautilus, using the same adapter.
Thunar can open `admin:///opt`; CFP now adds its convenient **Open as Administrator**
action. New Thunar administrative transfers still need real-desktop acceptance.

1. Locate the protected destination directory in the file manager.
2. Open it as Administrator using its available context-menu action. In Thunar,
   enable CFP's Thunar integration first, then right-click one selected local folder
   and choose **Open as Administrator**. Where supported, Ctrl+L and `admin:///opt/`
   can open the destination directly; Nemo's F3 split panes are convenient.
3. Authenticate if requested by GVfs/Polkit.
4. Search in CFP as your normal user and select results.
5. Drag those results into the administrator-opened destination.
6. Let the receiving file manager perform the copy/move and any further authorization.

CFP's ordinary copy/move and drag-and-drop behavior is unchanged. Ctrl requests copy,
Shift requests move; the receiving manager decides. Administrative destinations depend
on the manager, distribution, installed GVfs admin backend and desktop Polkit agent.
A protected ordinary local folder can reject a drop. If `admin://` is unavailable,
check your distribution's GVfs package documentation (Ubuntu/Mint commonly provides
it in `gvfs-backends`) and that a desktop authentication agent is running. Install any
missing distribution package yourself; CFP never installs it automatically. A denied
or cancelled prompt grants no access. Thunar/GVfs report authentication and location
errors; the helper reports missing backend/program and launch failures.

GVfs administrative access does not run the whole file manager as root. Keep CFP and
Thunar running as your regular user. Allow admin search authorizes only CFP's read-only
search worker, not later file operations. Dolphin's ordinary integration is preserved;
Dolphin-specific administrative access/transfers are outside this change and unverified.


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
The worker accepts local/mounted folders and uses the full filters, including authorized text/Hex content search.
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

Find Plus More Filters: check File type, Extension, Date mode, Group, User or
Permissions to activate it. Group/User use numeric ownership and include service/root accounts.
Permissions matches exactly three/four octal digits, including special bits (644, 4755, 1777).
No Hidden files skips dot-prefixed entries and prunes hidden directory trees. The selected
root is exempt. Collapse resets every More Filters value/checkbox; they start off on launch,
even when the section opens expanded. Main filters and other tabs remain intact.
Follow symbolic links is separately saved, defaults off and prevents repeated/circular
traversal by directory identity. Stay on this filesystem checks resolved targets. Excluded or
hidden target trees cannot be entered via aliases. See docs/search.md for alias result policy.

Date Mode: Any date or Last N minutes, hours, days, months. Previous N enables
automatically for checked relative modes in expanded More Filters; positive ASCII
whole numbers only. Collapse resets Date Mode off/Any and N=7. Main After/Before
AND Last N use the same selected timestamp. One inclusive interval is captured when
Find is pressed, before administrator authentication. Days are 24 elapsed hours;
months use calendar subtraction with month-end clamping. Date Modified keeps all
existing range modes and adds minutes/hours. Accessed depends on relatime/noatime.
Created filtering falls back to Modified only if birth data is absent. See docs/search.md
for dynamic calendar limits, examples and nonoverlapping-bound validation.

Exclude (non-RegEx): use | for OR, e.g. /Movies|/Pictures|cache or /backup|*.tmp|*.bak.
A leading / targets directories; matching subtrees are skipped before enumeration.
Use \| for a literal pipe and \\ for a literal backslash. Other backslashes, spaces and
colons remain literal. Empty terms are ignored. RegEx uses the whole raw field as one
native expression, e.g. (?:Movies|Pictures)|cache; no custom splitting/unescaping.
Raw text is saved unchanged. See docs/exclusion.md for complete rules and examples.

CONTENT SEARCH
Find Plus > Containing text searches literal ASCII/UTF-8; Aa controls case.
Hex searches exact bytes, e.g. 48656C6C6F20776F726C6421 = Hello world!.
Spaces/tabs between complete byte pairs are allowed. Maximum pattern: 64 KiB.
Empty input disables content scanning. Hex starts unchecked and is not saved.
All eligible regular binary/text files stream without a source-size cutoff;
boundary matches and early exit are supported. Stop may wait for blocked I/O.
Devices and unsafe kernel pseudo-filesystems are skipped, including in root mode.
No document/archive extraction; see docs/content-search.md for UTF-8, privilege
and filesystem limitations. Protected matches require explicit authorization.
