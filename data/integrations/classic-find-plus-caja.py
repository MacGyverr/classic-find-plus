"""Optional Caja 2.0 MenuProvider, loaded by python-caja."""
import gi
gi.require_version("Caja", "2.0")
from gi.repository import GObject, Gio, Caja

class ClassicFindPlusCaja(GObject.GObject, Caja.MenuProvider):
    def _item(self, folder):
        if not folder.is_directory():
            return []
        item = Caja.MenuItem(name="ClassicFindPlus::find", label="Find with Classic Find Plus",
                             tip="Search this folder", icon="classic-find-plus")
        item.connect("activate", self._launch, folder.get_uri())
        return [item]

    def _launch(self, item, uri):
        Gio.Subprocess.new(["classic-find-plus", "--", uri], Gio.SubprocessFlags.NONE)

    def get_file_items(self, window, files):
        return self._item(files[0]) if len(files) == 1 else []

    def get_background_items(self, window, folder):
        return self._item(folder)
