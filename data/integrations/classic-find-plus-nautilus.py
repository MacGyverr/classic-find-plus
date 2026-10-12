"""CFP opt-in Nautilus 3.0/4.x menus. No Gtk imports or synchronous I/O in callbacks."""
import logging
import gi

# nautilus-python loads its own Nautilus ABI before loading extensions. Respect it.
_api = gi.get_required_version("Nautilus")
if _api is None:
    for _candidate in ("4.1", "4.0", "3.0"):
        try:
            gi.require_version("Nautilus", _candidate)
            _api = _candidate
            break
        except ValueError:
            continue
if _api not in ("3.0", "4.0", "4.1"):
    raise ImportError("CFP requires Nautilus API 3.0, 4.0 or 4.1 and python3-nautilus")
from gi.repository import GObject, Gio, GLib, Nautilus


def local_uri(folder):
    """Only native local directories; URI parsing must not discard query text."""
    if not folder or not folder.is_directory():
        return None
    uri = folder.get_uri()
    try:
        parsed = GLib.Uri.parse(uri, GLib.UriFlags.ENCODED)
        if (parsed.get_scheme() != "file" or parsed.get_query() is not None or
                parsed.get_fragment() is not None or parsed.get_userinfo() is not None or
                parsed.get_port() != -1):
            return None
        path, host = GLib.filename_from_uri(uri)
        if host and host.lower() != "localhost":
            return None
        return Gio.File.new_for_path(path).get_uri()
    except (GLib.Error, ValueError, TypeError):
        return None

def existing_admin(api):
    """Conservative source inspection, not execution of another user's extension.

    Recognize nautilus-admin's documented GVfs directory launcher and ABI-compatible
    callbacks, not a package/name alone. Actual menu loading still needs a restart.
    """
    import ast
    import os
    import stat
    from pathlib import Path
    roots = [GLib.get_user_data_dir(), "/usr/local/share", *GLib.get_system_data_dirs()]
    seen = set()
    for base in dict.fromkeys(roots):
        directory = Path(base) / "nautilus-python/extensions"
        try:
            files = sorted(directory.glob("*.py"))
        except OSError:
            continue
        # Match nautilus-python's basename shadowing, including an empty user override.
        for path in files[:128]:
            if path.name in seen:
                continue
            seen.add(path.name)
            try:
                descriptor = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
                with os.fdopen(descriptor, "rb") as stream:
                    if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                        continue
                    source = stream.read(128 * 1024 + 1)
                if len(source) > 128 * 1024:
                    continue
                tree = ast.parse(source)
                requirements = {}
                for node in ast.walk(tree):
                    if isinstance(node, ast.Call) and ((isinstance(node.func, ast.Name) and node.func.id == "require_version") or
                            (isinstance(node.func, ast.Attribute) and node.func.attr == "require_version")):
                        if len(node.args) == 2 and all(isinstance(a, ast.Constant) for a in node.args):
                            requirements[node.args[0].value] = node.args[1].value
                if requirements.get("Nautilus") != api or (api != "3.0" and requirements.get("Gtk") == "3.0"):
                    continue
                cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "NautilusAdmin"), None)
                if cls is None or cls.decorator_list:
                    continue
                methods = {n.name: n for n in cls.body if isinstance(n, ast.FunctionDef)}
                expected = 3 if api == "3.0" else 2
                if any(name not in methods or len(methods[name].args.args) != expected
                       for name in ("get_file_items", "get_background_items")):
                    continue
                if any(not any(isinstance(n, ast.Attribute) and n.attr == "_create_nautilus_item" for n in ast.walk(methods[name])) or
                       not any(isinstance(n, ast.Attribute) and n.attr == "is_directory" for n in ast.walk(methods[name]))
                       for name in ("get_file_items", "get_background_items")):
                    continue
                launch = methods.get("_nautilus_run")
                if launch is None or launch.decorator_list:
                    continue
                body = launch.body
                if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant):
                    body = body[1:]
                canonical = ast.parse('uri = file.get_uri()\nadmin_uri = uri.replace("file://", "admin://")\nsubprocess.Popen([NAUTILUS_PATH, admin_uri])').body
                if ast.dump(ast.Module(body=body, type_ignores=[])) != ast.dump(ast.Module(body=canonical, type_ignores=[])):
                    continue
                program = next((n.value.value for n in tree.body if isinstance(n, ast.Assign) and
                    any(isinstance(t, ast.Name) and t.id == "NAUTILUS_PATH" for t in n.targets) and
                    isinstance(n.value, ast.Constant) and isinstance(n.value.value, str)), None)
                if not program or not os.path.isabs(program) or not os.access(program, os.X_OK):
                    continue
                create = methods.get("_create_nautilus_item")
                if create and any(isinstance(n, ast.Constant) and n.value == "NautilusAdmin::Nautilus" for n in ast.walk(create)) and any(
                        isinstance(n, ast.Attribute) and n.attr == "_nautilus_run" for n in ast.walk(create)):
                    return str(path)
            except (OSError, SyntaxError, ValueError, TypeError, RecursionError):
                continue
    return None


# Evaluate once when Nautilus loads/reloads extensions, never during menu rendering.
_external_admin = existing_admin(_api)


class ClassicFindPlusNautilus(GObject.GObject, Nautilus.MenuProvider):
    def _items(self, folder, context):
        uri = local_uri(folder)
        if uri is None:
            return []
        # Nautilus4 registers both menus together; distinct IDs prevent background
        # actions from replacing selected-directory callbacks in its action map.
        item = Nautilus.MenuItem(name="ClassicFindPlus::find-" + context, label="Find with Classic Find Plus",
                                 tip="Search this local folder")
        item.connect("activate", self._launch, ("classic-find-plus", "--", uri))
        items = [item]
        if not _external_admin:
            admin = Nautilus.MenuItem(name="ClassicFindPlus::admin-" + context, label="Open as Administrator",
                                      tip="Open this local folder through GVfs/Polkit")
            admin.connect("activate", self._launch, ("classic-find-plus-nautilus-admin", "--", uri))
            items.append(admin)
        return items

    def _launch(self, item, argv):
        try:
            Gio.Subprocess.new(list(argv), Gio.SubprocessFlags.NONE)
        except GLib.Error as error:
            logging.getLogger("classic-find-plus").warning("Could not launch CFP action: %s", error)

    def get_file_items(self, *args):
        # API 3.0: (window, files); 4.x: (files). Reject malformed callbacks safely.
        if len(args) not in (1, 2):
            return []
        files = args[-1]
        return self._items(files[0], "selected") if files and len(files) == 1 else []

    def get_background_items(self, *args):
        return self._items(args[-1], "background") if len(args) in (1, 2) else []
