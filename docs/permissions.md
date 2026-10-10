# Permissions and administrative transfers

Classic Find Plus runs as your regular user. Searching, file actions and export use your
normal permissions. The Find Plus Allow admin search checkbox authorizes a separate read-only
worker through Polkit only when Find is pressed. It never elevates subsequent actions. CFP
does not capture passwords or invoke sudo for operations. See [administrator search and
explicit root override](admin-search.md) for authorization, limits and setup.

Linux Mint workflow:

1. Open Nemo and press F3 for split panes.
2. Keep a normal source folder in one pane.
3. In the destination pane press Ctrl+L and enter `admin:///opt/` or another protected directory.
4. Authenticate when prompted.
5. Open CFP from the normal folder and drag selected results into the administrative pane.
6. Authenticate again if the receiving backend requests it.

Nemo and GVfs/Polkit handle the transfer and authorization. This workflow depends on the
installed admin backend, a desktop authentication agent and session configuration. It is not
a guarantee for every distribution or backend. A local protected folder without an authenticated
administrative URI may simply reject a drop. The checkbox keeps CFP a regular-user application. Explicit --allow-root instead grants
root permissions to every GUI feature, with a warning and separate root profile.

Ctrl requests copy and Shift requests move; Nemo decides the operation. If authentication
takes longer than the bounded synchronization window, rerun Find after transfer. Exports
require a local/mounted writable destination: select another folder when Save reports denial.
