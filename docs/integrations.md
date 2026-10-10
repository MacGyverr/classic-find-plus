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
| Nautilus | native Python MenuProvider, 3.x/4.0/4.1 signatures | distro `python3-nautilus`, Python GI and Nautilus typelib; `$XDG_DATA_HOME/nautilus-python/extensions` |
| Dolphin | KDE service menu, directory MIME type | Dolphin/KIO; `$XDG_DATA_HOME/kio/servicemenus` (Frameworks ≥5.85) |
| Caja | executable Scripts menu launcher | Python 3; `$XDG_CONFIG_HOME/caja/scripts/Find with Classic Find Plus` |
| Thunar | one directory custom action | Thunar UCA plugin; `$XDG_CONFIG_HOME/Thunar/uca.xml` |

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
are refused. Thunar edits only CFP's action span; unrelated entries and bytes are retained.
Removing the application retains per-user integration choices/files; remove enabled adapters
through Help/About first if desired. Retained entries cannot launch a removed executable.

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

Ordinary URI drag-and-drop is separate from menu integration. Nemo/Caja ordinary transfers
have prior verification; those transfers do not verify privileged destinations.
Administrative destinations depend on the receiving manager/backend and session (GVfs/Polkit
for some GTK managers, KIO for KDE). Showing a protected search result grants no transfer
permission. GTK/KDE and Wayland drag interoperability varies. New adapter lifecycle/API/argv
fixtures are tested. Caja 1.26's background right-click Scripts entry and production CFP launch
were also verified with the Ubuntu 22.04 SDK in a disposable user/mount namespace, including
the correct special-character folder and Look in value. That GUI fixture used namespace UID 0
mapped to the ordinary host user, an explicit --allow-root override and a disposable root
profile; it does not verify host-root desktop authentication. Other managers' new menu GUIs
remain provisional.

Mechanisms follow upstream [Nautilus Python](https://gnome.pages.gitlab.gnome.org/nautilus-python/class-nautilus-python-menu-provider.html),
[Caja Python](https://github.com/mate-desktop/python-caja),
[Dolphin service menus](https://develop.kde.org/docs/apps/dolphin/service-menus/),
[Thunar custom actions](https://docs.xfce.org/xfce/thunar/custom-actions) and
[Nemo action manager](https://github.com/linuxmint/nemo/blob/master/libnemo-private/nemo-action-manager.c).
