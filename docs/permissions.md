# Permissions and administrative transfers

Classic Find Plus runs as your regular user. Searching, file actions and export use your
normal permissions. The Find Plus Allow admin search checkbox authorizes a separate read-only
worker through Polkit only when Find is pressed. It never elevates subsequent actions. CFP
does not capture passwords or invoke sudo for operations. See [administrator search and
explicit root override](admin-search.md) for authorization, limits and setup.

Linux Mint workflow:

Nemo, GNOME Files (Nautilus) and Caja accept CFP drops into administrator-opened
folders on the owner's tested systems. GNOME Files is Nautilus. Its CFP integration adds Find and an Open as Administrator
fallback when no recognized compatible independent action is found.
Thunar can open `admin:///opt`; CFP now adds its convenient **Open as Administrator**
action. New Thunar administrative transfers still need real-desktop acceptance.

1. Locate the protected destination directory in the file manager.
2. Open it as Administrator using its available context-menu action. In GNOME Files or Thunar,
   enable that manager's CFP integration first, then right-click one selected local folder
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
or cancelled prompt grants no access. Nautilus/Thunar/GVfs report authentication and location
errors; the helper reports missing backend/program and launch failures.

GVfs administrative access does not run the whole file manager as root. Keep CFP and
GNOME Files and Thunar running as your regular user. Allow admin search authorizes only CFP's read-only
search worker, not later file operations. Dolphin's ordinary integration is preserved;
Dolphin-specific administrative access/transfers are outside this change and unverified.

Ctrl requests copy and Shift requests move; the receiving manager decides the operation. If authentication
takes longer than the bounded synchronization window, rerun Find after transfer. Exports
require a local/mounted writable destination: select another folder when Save reports denial.

GTK PCManFM and XFE now have optional Find integrations. PCManFM's isolated admin URI
probe did not verify authenticated enumeration; XFE's safe GVfs administrative workflow
is unverified. CFP installs no administrator action for either and makes no administrative
transfer claim. Use the already supported, separately configured receiving manager above.


PCManFM-Qt has a separate optional integration. Enabling it supplies Find and Open as
Administrator together; the latter opens a GVfs admin URI in normal-user PCManFM-Qt,
using the desktop Polkit agent. Its action may also appear in GTK PCManFM because the
managers share the custom-action directory; it still opens Qt. Ensure gvfs-backends
and the authentication agent are available, then restart affected managers.

The native Qt1.4.1 selected-directory administrator launch and authorization prompt
were exercised and cancelled. Authenticated directory enumeration and administrative
copy/move acceptance were not verified. The transfer procedure above is a workflow to
test on the receiving desktop, not a claim of verified Qt transfer support. CFP remains
unprivileged and the receiving manager/GVfs owns authorization and transfer results.
