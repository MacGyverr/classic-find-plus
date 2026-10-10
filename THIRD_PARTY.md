# License and asset notices

Classic Find Plus is distributed under **GPL-3.0-or-later**; the complete license is in
[LICENSE](LICENSE). Existing project credit is MacGyverr / ChatGPT (2026).
This release preserves that declaration and does not assign a different license.

The avatar `data/artwork/favicon.ico`, its pixel-equivalent PNG `icon.png`, and the portrait
`about.png` were supplied by the project owner. The owner explicitly authorized their public
redistribution under GPL-3.0-or-later on 2026-10-09. The original ICO and portrait are retained.
No bundled font binaries or additional third-party source libraries were identified in the
release inventory. Generated GResource C is produced at build time from the licensed images.

GTK, GLib/GIO and their dependencies are system libraries, not vendored source or binary
assets. Their respective licenses continue to apply. The application requests standard
theme icons from the desktop; system icon themes are not redistributed in this source tree.

Optional administrator search communicates with system D-Bus and Polkit under their own licenses.
Linux UAPI headers provide syscall/filter definitions; no third-party helper or authority is bundled.
