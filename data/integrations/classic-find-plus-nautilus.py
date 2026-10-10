"""Optional Nautilus 3.x/4.x MenuProvider. No search or privilege logic here."""
import gi
for _version in ("4.1", "4.0", "3.0"):
    try:
        gi.require_version("Nautilus", _version)
        break
    except ValueError:
        continue
from gi.repository import GObject, Gio, Nautilus

class ClassicFindPlusNautilus(GObject.GObject, Nautilus.MenuProvider):
    def _item(self, folder):
        if not folder.is_directory():
            return []
        item = Nautilus.MenuItem(name="ClassicFindPlus::find", label="Find with Classic Find Plus",
                                 tip="Search this folder", icon="classic-find-plus")
        item.connect("activate", self._launch, folder.get_uri())
        return [item]

    def _launch(self, item, uri):
        Gio.Subprocess.new(["classic-find-plus", "--", uri], Gio.SubprocessFlags.NONE)

    def get_file_items(self, *args):
        files = args[-1]  # Nautilus 3 passes window, files; 4 passes files.
        return self._item(files[0]) if len(files) == 1 else []

    def get_background_items(self, *args):
        return self._item(args[-1])
