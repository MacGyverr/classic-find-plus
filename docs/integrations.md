# Optional file-manager menus

Fresh DEB and source installations enable **none**. Open Help/About → File Manager
Integrations, then Refresh. Status shows detection/version, configuration installed/enabled,
missing optional components and whether changes need a manager restart/reload. It does not
prove a context menu has appeared. **Install** registers a disabled adapter; **Enable** activates
it. **Disable** retains its registration. **Remove** deletes only unchanged CFP-owned entries.
CFP never installs dependencies or additional file managers, nor restarts one automatically.

| Manager | Supported adapter | Optional dependency / user location |
| --- | --- | --- |
| Nemo | single-directory `.nemo_action` | Nemo and `gsettings`; `$XDG_DATA_HOME/nemo/actions` |
| GNOME Files (Nautilus) | native Python MenuProvider, 3.0/4.0/4.1 Find + administrator menus | distro `python3-nautilus`, Python GI and Nautilus typelib; `$XDG_DATA_HOME/nautilus-python/extensions` |
| Dolphin | KDE service menu, directory MIME type | Dolphin/KIO; `$XDG_DATA_HOME/kio/servicemenus` (Frameworks ≥5.85) |
| Caja | executable Scripts menu launcher | Python 3; `$XDG_CONFIG_HOME/caja/scripts/Find with Classic Find Plus` |
| Thunar | Find + Open as Administrator directory actions | Thunar UCA plugin; `$XDG_CONFIG_HOME/Thunar/uca.xml` |
| PCManFM (GTK) | libfm Find custom action | GTK PCManFM, `libfm-modules`; `$XDG_DATA_HOME/file-manager/actions/classic-find-plus-pcmanfm.desktop` |
| XFE | executable Scripts menu launcher | XFE, Python3; `~/.config/xfe/scripts/Find with Classic Find Plus` |

XDG_DATA_HOME normally means `~/.local/share`, XDG_CONFIG_HOME `~/.config`. All adapters
launch the same `classic-find-plus -- LOCATION`. Python adapters use an argv array; native
action/service field codes are expanded by the manager. Locations with spaces, Unicode,
quotes and shell characters do not become shell commands. The chosen folder overrides saved
Look in. Nautilus/Caja also offer an action on a folder background where their API supports it.

Dolphin versions before 21.08 normally use `kservices5/ServiceMenus`; the manager remembers
the selected path. Unknown/custom KDE stacks default to the modern location. If a custom
Frameworks <5.85 stack needs the legacy path, remove that disabled adapter, then run the CLI
Install/Enable with `CFP_DOLPHIN_LEGACY=1`. A manager restart may be needed after any change.
Nautilus Python extensions generally require closing all manager processes or logging
out/in; finish transfers first. Thunar configuration dialogs should be closed during changes.
No entire desktop stack is required merely to prepare an adapter.

For Caja, choose **Scripts → Find with Classic Find Plus** after enabling its integration.
Select one local folder, or invoke it on the background to search the current local folder.
The executable script is installed only when Enable is pressed, normally at
`~/.config/caja/scripts/Find with Classic Find Plus`; no python-caja package is needed.
Its Scripts entry also appears for unsupported selections, which the launcher refuses.
Version 1.0.1 instead installed a Python MenuProvider under `~/.local/share/caja-python/extensions`.
For that existing registration, Help/About provides **Switch to Scripts**: it enables the
script and removes only the unchanged CFP extension. Modified/unrelated files are preserved
by refusing the switch. Restart Caja after switching to unload any old Python provider.
Disable and Remove operate only on CFP's owned entry, leaving other Caja scripts untouched.

Existing system Nemo actions from previous CFP packages are retained on upgrade, including
disabled/customized content. The manager changes only CFP's basename in Nemo's supported
`org.nemo.plugins disabled-actions` preference, preserving other disabled actions. It avoids
creating a duplicate user action while a legacy system action exists. Remove restores the
previous CFP preference when it has not been independently changed; the legacy global action
may therefore appear again. Package removal cleans only unchanged migrated global content.
Source upgrades preserve their tracked legacy Nemo file. No per-user choices are reset.

User-local ownership state is `$XDG_DATA_HOME/classic-find-plus/integration-state.json`.
Updates use a lock, fixed paths, content hashes and atomic replacement. Modified/untracked
adapters are refused; back up custom work and review manually. A byte-exact supplied template
can be adopted. Symlink paths, oversized files, duplicate CFP Thunar IDs and XML entities/DTD
are refused. Thunar edits only CFP's action spans; unrelated entries and bytes are retained.
Removing the application retains per-user integration choices/files; remove enabled adapters
through Help/About first if desired. Retained entries cannot launch a removed executable.

## GNOME Files (Nautilus)

One user-local Python extension provides **Find with Classic Find Plus** and, when needed,
**Open as Administrator** for a single selected local directory or its background. Find
opens CFP at that folder without starting a search. API 3.0 covers pre43 Nautilus; API 4.0/4.1
covers newer compatible versions, including Nautilus 46.4/API 4.0. The extension honors the
loader's GI version and never imports GTK3 into Nautilus's GTK4 process.

Enable the single **GNOME Files (Nautilus)** entry in Help/About. Refresh probes the matching
GI typelib, native Python provider dependencies and CFP GObject/menu initialization in a
separate bounded process. **Configured (unavailable)** means the user extension is enabled
on disk but required support cannot load; it does not promise a working menu. Disable
remains available. A successful probe still requires a Nautilus restart and menu check.
Custom provider library prefixes may need distribution-specific setup; the probe checks
standard distro library locations. Status never installs packages or edits extensions.

Ubuntu/Mint/Debian normally need `python3-nautilus`, `python3-gi` and the matching Nautilus
introspection package (`gir1.2-nautilus-3.0` or `gir1.2-nautilus-4.0`). Use your distribution's
package manager to install reported missing dependencies. CFP does not request credentials
or silently install them. Python support remains optional for every other CFP function.

Older unchanged CFP extensions show **Repair actions**. CLI `enable nautilus` performs the
same atomic reconciliation, including recovery of the exact old template if ownership state
was lost. Modified, unsafe or unrecognized files require manual review. Disable removes the
active CFP extension but retains registration; Remove also removes that registration. Neither
operation touches independent administrator extensions or shared packages. File mode is 0644.

Before adding its administrator item, CFP inspects bounded source of known `NautilusAdmin`
implementations without executing them. It checks matching GI API, callback signatures,
directory-menu semantics and direct normal-user Nautilus launch; renamed equivalents can
be recognized. The Ubuntu legacy nautilus-admin 1.1.9 implementation uses API 3.0/GTK3 and
is not treated as compatible in an API4 process. A recognized compatible candidate suppresses
only CFP's admin item; Find remains. This conservative inspection cannot prove arbitrary
third-party extensions work or recognize every equivalent. Unknown extensions remain
untouched and may still provide their own item; actual loading/restart must be checked.

The separate normal-user `classic-find-plus-nautilus-admin -- LOCATION` helper reuses the
Thunar helper's checked native-path/file-URI conversion to escaped `admin:///` locations.
Remote locations are not offered either action. Missing/disappeared directories, invalid
URIs, absent Nautilus/backend and launch errors are reported without a shell or root file
manager. Inaccessible local ancestors are delegated to GVfs. Nautilus/GVfs/Polkit own
mounting, agent/authentication, cancellation/denial and destination errors; process launch
success is not proof of authorization. CFP never handles passwords.

After finishing transfers, close all Nautilus windows/processes and reopen it, or log out/in.
A deliberate `nautilus -q` can close its processes; do not use it during transfers. If menus
remain absent, collect `nautilus --version`, integration-manager `status`, and launch diagnostics
from `NAUTILUS_PYTHON_DEBUG=all nautilus` after a full restart, plus relevant session journal
messages. Check for other extensions shadowing the same basename and matching distro API
packages. Do not delete or patch system extensions to try to make CFP load.

Native Mint Nautilus46.4/API4.0 diagnosis found missing `python3-nautilus` and
`gir1.2-nautilus-4.0`. After their installation, actual provider loading, selected-folder
and background menus show both CFP actions; Find launches CFP. Earlier API3.0 library
checks and API4.1 callback fixtures remain compatibility coverage, not live-version
claims. Distinct selected/background action IDs prevent Nautilus4’s background action from
replacing the selected-directory callback. Current tests do not revalidate administrator
authentication or transfers.

See [administrator transfer workflow](permissions.md) for protected destinations.

## Thunar administrator action

Enable installs **Find with Classic Find Plus** and **Open as Administrator** together.
Older enabled find-only installations show **Repair actions**; CLI `enable thunar` also
reconciles them idempotently. Status/Refresh never changes your configuration. Disable
and Remove remove both unchanged CFP-owned actions, not unrelated custom actions.
Stable IDs are `org.classicfind.Plus.find` and `org.classicfind.Plus.open-admin`.
Before changing existing XML, CFP retains a private `uca.xml.cfp-backup-<hash>` sibling;
original XML mode/UID/GID and unrelated bytes survive the atomic update. Modified CFP
actions, unsafe ownership/write permissions, symlinks and malformed XML are refused.
Backups remain for manual recovery. Close Thunar's custom-action editor before changes.

Right-click **one selected local folder** (including the filesystem root). Background
context eligibility is determined by Thunar's UCA provider; selected-folder context is
supported. The action is directory-only. A clearly equivalent independent action is
retained without adding a second admin action or taking ownership. Detection deliberately
accepts only exact directory/all-pattern argv forms: this helper with `-- %f`/`-- %u`, or
`Thunar admin://%f` (also lowercase thunar). Similar labels and arbitrary shell wrappers
are preserved and cannot reliably be identified as equivalent.

`classic-find-plus-thunar-admin -- LOCATION` accepts an absolute local path or local
`file:` URI. GIO encodes spaces, Unicode, quotes, #, %, &, and other filename bytes into
an `admin:///` URI. Remote schemes/hosts, query/fragment/credential URIs, non-directories
and ordinary missing paths are refused. Inaccessible local ancestors are delegated to
GVfs. The helper refuses root execution and starts Thunar via an argv array; it never
runs sudo, captures passwords or changes ownership/permissions of the selected folder.
Thunar's supported `%f` field expansion quotes the single local path, including in
Thunar 4.16; do not add shell quoting around `%f` in the template. Remote selections
without a native path report that a local directory is required. Thunar/GVfs own mounting, authentication and location errors;
a successful launch is not proof that authorization succeeded.

See [administrative transfer steps and troubleshooting](permissions.md). No backend,
authentication agent or file manager is installed automatically. Custom source prefixes
must put their bin directory on the desktop session's PATH.

CLI equivalent (substitute the installed PREFIX; never use sudo for user integrations):

```sh
python3 /usr/share/classic-find-plus/integration-manager.py status
python3 /usr/share/classic-find-plus/integration-manager.py install nemo
python3 /usr/share/classic-find-plus/integration-manager.py enable nemo
python3 /usr/share/classic-find-plus/integration-manager.py disable nemo
python3 /usr/share/classic-find-plus/integration-manager.py remove nemo
```

Use `nautilus`, `dolphin`, `caja` or `thunar` in place of `nemo`. Source builds can run
`python3 scripts/integration-manager.py ...`; this still modifies only the current user's
chosen integration and requires CFP on the desktop session's PATH.

The owner reports successful real context-menu launch and ordinary copy/move transfers,
including destination replacement, in Nemo, Nautilus, Dolphin, Caja and Thunar with the
preceding release. One intermittent Nemo replacement-copy observation remains unconfirmed;
Transfer negotiation remains unchanged.

Ordinary URI drag-and-drop is separate from menu integration; those transfers do not verify privileged destinations.
Administrative destinations depend on the receiving manager/backend and session (GVfs/Polkit
for some GTK managers, KIO for KDE). Showing a protected search result grants no transfer
permission. GTK/KDE and Wayland drag interoperability varies. New adapter lifecycle/API/argv
fixtures are tested. Caja 1.26's background right-click Scripts entry and production CFP launch
were also verified with the Ubuntu 22.04 SDK in a disposable user/mount namespace, including
the correct special-character folder and Look in value. That GUI fixture used namespace UID 0
mapped to the ordinary host user, an explicit --allow-root override and a disposable root
profile; it does not verify host-root desktop authentication. The owner's five-manager confirmation is separate from this namespace fixture.

Mechanisms follow upstream [Nautilus Python](https://gnome.pages.gitlab.gnome.org/nautilus-python/class-nautilus-python-menu-provider.html),
[Caja Python](https://github.com/mate-desktop/python-caja),
[Dolphin service menus](https://develop.kde.org/docs/apps/dolphin/service-menus/),
[Thunar custom actions](https://docs.xfce.org/xfce/thunar/custom-actions) and
[Nemo action manager](https://github.com/linuxmint/nemo/blob/master/libnemo-private/nemo-action-manager.c).

## PCManFM (GTK)

Enable the separate PCManFM (GTK) row. CFP installs a libfm custom action, preserving
unrelated files; Install alone keeps it disabled. Restart PCManFM after changes.
One selected local directory shows Find with Classic Find Plus and opens CFP there.
TargetLocation also requests the current-folder background action where libfm supports it.
The template uses libfm's absolute-local-folder glob, directory MIME and single-selection
conditions. It avoids libfm1.3.2's crashing Schemes condition; no file-manager files are
patched. Native GTK PCManFM1.3.2/libfm1.3.2 selected-folder and restarted background menus
and exact child/current-folder CFP URI launches were executed.
PCManFM-Qt has a different integration mechanism and is outside this adapter.

The authorized read-only `admin:///opt` probe did not demonstrate authenticated directory
enumeration in the isolated session. CFP consequently supplies no Open as Administrator
PCManFM action. Administrative transfers are unverified. This is not a claim that every
PCManFM/GVfs build lacks the capability; confirm it separately before extending support.

## XFE

Enable XFE, then restart it. Right-click → Scripts → Find with Classic Find Plus uses
XFE's documented executable-script directory `~/.config/xfe/scripts` (the literal HOME
location used by XFE1.45). One selected folder searches itself, one regular file searches
its parent, XFE1.45's empty background has no Scripts submenu: select one item first. When invoked
separately without arguments, the launcher uses its working directory.
Multiple, nonexistent and remote selections are refused. Paths are separate argv values;
spaces, Unicode, quotes and shell punctuation cannot become commands. Disable/Remove
only remove the exact unchanged CFP script; unrelated scripts/configuration stay intact.
Native XFE1.45's Scripts menu and actual selected-folder CFP launch were executed.
XFE's local-filesystem/root-launch features do not demonstrate a safe GVfs administrative
workflow. CFP offers no XFE administrator action and does not launch XFE as root.
