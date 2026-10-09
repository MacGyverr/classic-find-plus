# Permissions and administrative transfers

Classic Find Plus runs as your regular user. Searching, file actions and export use your
normal permissions. Access failures are reported; CFP does not capture passwords, invoke
sudo for operations, modify Nemo or introduce a privileged helper.

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
administrative URI may simply reject a drop. CFP must stay a regular-user application.

Ctrl requests copy and Shift requests move; Nemo decides the operation. If authentication
takes longer than the bounded synchronization window, rerun Find after transfer. Exports
require a local/mounted writable destination: select another folder when Save reports denial.
