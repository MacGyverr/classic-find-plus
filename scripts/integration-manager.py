#!/usr/bin/env python3
"""Opt-in, per-user adapters. No package installs, shell commands or manager restarts."""
import ast
import configparser
import io
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from xml.parsers import expat

MANAGERS = ('nemo', 'nautilus', 'dolphin', 'caja', 'thunar')
TEMPLATES = dict(zip(MANAGERS, ('classic-find-plus.nemo_action', 'classic-find-plus-nautilus.py',
    'classic-find-plus.desktop', 'classic-find-plus-caja.py', 'classic-find-plus-thunar.xml')))
RESOURCE = Path(__file__).resolve().parent / 'integrations'
if not RESOURCE.is_dir():
    RESOURCE = Path(__file__).resolve().parent.parent / 'data/integrations'
LIMIT = 1024 * 1024
UID = 'org.classicfind.Plus.find'

def safe(path):
    if not path.is_absolute() or '..' in path.parts:
        raise RuntimeError('Integration paths must be absolute without ..')
    for item in [*reversed(path.parents), path]:
        if item.is_symlink(): raise RuntimeError('Symlink path refused: ' + str(item))
        if item.exists() and item != path and not item.is_dir():
            raise RuntimeError('Non-directory ancestor: ' + str(item))

def read(path):
    safe(path)
    if not path.exists(): return None
    if not path.is_file() or path.stat().st_size > LIMIT:
        raise RuntimeError('Non-regular or oversized integration file: ' + str(path))
    return path.read_bytes()

def digest(data):
    return hashlib.sha256(data).hexdigest() if data is not None else None

def atomic(path, data, previous, mode=0o600, ownership=None):
    safe(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, name = tempfile.mkstemp(prefix='.cfp-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(data); out.flush(); os.fsync(out.fileno())
        if ownership is not None: os.chown(name, *ownership)
        os.chmod(name, mode)
        if read(path) != previous: raise RuntimeError('Configuration changed; retry after closing the manager settings dialog')
        os.replace(name, path)
    finally:
        if os.path.exists(name): os.unlink(name)

def version(manager):
    program = shutil.which('Thunar' if manager == 'thunar' else manager)
    if not program: return ''
    try:
        result = subprocess.run([program, '--version'], capture_output=True, text=True, timeout=2)
        match = re.search(r'\b(\d+)\.(\d+)(?:\.\d+)?', result.stdout + result.stderr)
        return match.group(0) if match else 'detected (version unknown)'
    except (OSError, subprocess.TimeoutExpired):
        return 'detected (version unavailable)'

def path_for(manager, data, config, entry):
    if manager == 'thunar': return config / 'Thunar/uca.xml'
    if manager == 'caja': return config / 'caja/scripts/Find with Classic Find Plus'
    if manager == 'dolphin':
        # Frameworks >=5.85 use kio/servicemenus. Dolphin 21.08+ normally
        # accompanies that stack. Unknown/custom stacks can choose the legacy
        # path explicitly with CFP_DOLPHIN_LEGACY=1 before installation.
        legacy = entry.get('legacy')
        if legacy is None:
            match = re.match(r'(\d+)\.(\d+)', version(manager))
            legacy = bool(match and tuple(map(int, match.groups())) < (21, 8)) or os.getenv('CFP_DOLPHIN_LEGACY') == '1'
        return data / ('kservices5/ServiceMenus' if legacy else 'kio/servicemenus') / 'classic-find-plus.desktop'
    directories = {'nemo': 'nemo/actions', 'nautilus': 'nautilus-python/extensions', 'caja': 'caja-python/extensions'}
    return data / directories[manager] / TEMPLATES[manager]

ADMIN_UID = 'org.classicfind.Plus.open-admin'
ADMIN_TEMPLATE = 'classic-find-plus-thunar-admin.xml'

def xml_inventory(data, root_span=None):
    """Structural spans only: never serialize unrelated user's XML."""
    if data is None: return []
    if len(data) > LIMIT or b'<!DOCTYPE' in data.upper() or b'<!ENTITY' in data.upper():
        raise RuntimeError('Thunar XML entities/DTD or oversized XML refused')
    if '\0' in data.decode('utf-8'): raise RuntimeError('Thunar XML must use UTF-8')
    parser = expat.ParserCreate()
    depth, start, field, fields, spans = 0, None, None, {}, []
    root_start = 0
    def begin(name, attrs):
        nonlocal depth, start, field, fields, root_start
        depth += 1
        if depth > 32: raise RuntimeError('Thunar XML nesting limit')
        if depth == 1:
            if name != 'actions': raise RuntimeError('Expected Thunar actions XML')
            root_start = parser.CurrentByteIndex
        if depth == 2 and name == 'action': start = parser.CurrentByteIndex; fields = {}
        if depth == 3 and start is not None:
            if name in fields and name in ('unique-id', 'command', 'patterns'):
                raise RuntimeError('Duplicate Thunar action field')
            fields.setdefault(name, ''); field = name
    def content(value):
        if field is not None: fields[field] += value
    def end(name):
        nonlocal depth, start, field
        if depth == 3: field = None
        if depth == 2 and name == 'action' and start is not None:
            opening_end = data.index(b'>', start) + 1
            stop = opening_end if data[start:opening_end].rstrip().endswith(b'/>') else data.index(b'>', parser.CurrentByteIndex) + 1
            spans.append((start, stop, data[start:stop], fields.copy()))
            start = None
        if depth == 1 and root_span is not None:
            root_span.extend((root_start, parser.CurrentByteIndex))
        depth -= 1
    def no_doctype(*args): raise RuntimeError('Thunar XML DTD refused')
    parser.StartDoctypeDeclHandler = no_doctype
    parser.StartElementHandler = begin; parser.EndElementHandler = end; parser.CharacterDataHandler = content
    parser.Parse(data, True)
    for uid in (UID, ADMIN_UID):
        if sum(p[3].get('unique-id', '').strip() == uid for p in spans) > 1:
            raise RuntimeError('Duplicate CFP Thunar actions; resolve manually')
    return spans

def xml_action(data):
    return next((p[:3] for p in xml_inventory(data) if p[3].get('unique-id', '').strip() == UID), None)

def independent_admin(parts):
    # Only exact argv semantics and directory-only, all-directory eligibility.
    # Similar names, shell snippets and sudo/root launchers are NOT equivalent.
    import shlex
    for part in parts:
        f = part[3]
        if f.get('unique-id', '').strip() in (UID, ADMIN_UID): continue
        if 'directories' not in f or f.get('patterns', '').strip() != '*': continue
        if any(k in f for k in ('other-files', 'text-files', 'audio-files', 'image-files', 'video-files')): continue
        try: args = shlex.split(f.get('command', ''))
        except ValueError: continue
        if args in (['classic-find-plus-thunar-admin', '--', '%f'],
                    ['classic-find-plus-thunar-admin', '--', '%u']): return True
        # Conservative known-safe native-directory command, not arbitrary shell.
        if args in (['Thunar', 'admin://%f'], ['thunar', 'admin://%f']): return True
    return False

def thunar_details(contents):
    parts = xml_inventory(contents)
    admin = next((p for p in parts if p[3].get('unique-id', '').strip() == ADMIN_UID), None)
    external = independent_admin(parts)
    return parts, admin, external

def thunar_update(operation, path, original, entry):
    parts, admin, external = thunar_details(original)
    templates = {UID: read(RESOURCE / TEMPLATES['thunar']).strip(),
                 ADMIN_UID: read(RESOURCE / ADMIN_TEMPLATE).strip()}
    owned = [p for p in parts if p[3].get('unique-id', '').strip() in templates]
    for p in owned:
        uid = p[3]['unique-id'].strip(); key = 'hash' if uid == UID else 'admin_hash'
        if digest(p[2]) != entry.get(key) and p[2] != templates[uid]:
            raise RuntimeError('Modified CFP Thunar action; preserve it and resolve manually: ' + uid)
        entry[key] = digest(p[2]); entry['installed'] = True
    if operation == 'install':
        entry['installed'] = True
        return entry
    if operation in ('enable', 'disable') and not entry.get('installed'):
        raise RuntimeError('Install this integration first')
    xml = original or b'<?xml version="1.0" encoding="UTF-8"?>\n<actions>\n</actions>\n'
    wanted = [UID] + ([] if external else [ADMIN_UID]) if operation == 'enable' else []
    # Preserve exact existing matching templates and their positions on re-enable.
    for p in reversed(owned):
        uid = p[3]['unique-id'].strip()
        if uid not in wanted: xml = xml[:p[0]] + xml[p[1]:]
        else: wanted.remove(uid)
    if wanted:
        root_span = []
        xml_inventory(xml, root_span)
        start, end = root_span
        opening_end = xml.index(b'>', start) + 1
        if xml[start:opening_end].rstrip().endswith(b'/>'):
            opening = re.sub(br'/\s*>$', b'>', xml[start:opening_end])
            xml = xml[:start] + opening + b'</actions>' + xml[opening_end:]
            end = start + len(opening)
        xml = xml[:end] + b''.join(templates[uid] for uid in wanted) + xml[end:]
    xml_inventory(xml)  # Validate final bytes before backup/write.
    if xml != original and (original is not None or operation == 'enable'):
        mode = 0o600
        if original is not None:
            info = path.stat()
            if info.st_uid != os.geteuid() or info.st_mode & 0o7022:
                raise RuntimeError('Unsafe Thunar configuration ownership/permissions')
            mode = stat.S_IMODE(info.st_mode)
            backup = path.with_name('uca.xml.cfp-backup-' + digest(original)[:16])
            saved = read(backup)
            if saved is None: atomic(backup, original, None)
            elif saved != original: raise RuntimeError('Thunar backup collision; preserve and resolve manually')
        atomic(path, xml, original, mode, (info.st_uid, info.st_gid) if original is not None else None)
    if operation == 'remove': return {'reload': bool(owned or entry.get('reload'))}
    final = xml_inventory(xml)
    for uid, key in ((UID, 'hash'), (ADMIN_UID, 'admin_hash')):
        p = next((p for p in final if p[3].get('unique-id', '').strip() == uid), None)
        entry[key] = digest(p[2]) if p else None
    entry.update(installed=True, reload=True)
    return entry

def optional(manager):
    if manager not in ('nautilus', 'caja'): return ''
    namespace = manager.capitalize() if manager == 'caja' else 'Nautilus'
    code = "import gi; from gi.repository import GIRepository; r=GIRepository.Repository.get_default(); assert r.enumerate_versions(%r)" % namespace
    result = subprocess.run([sys.executable, '-c', code], capture_output=True, timeout=3)
    roots = [Path('/usr/lib'), Path('/usr/lib64'), Path('/usr/local/lib')]
    # Avoid a recursive disk scan: only conventional extension directories.
    candidates = []
    for root in roots:
        for base in (root, *root.glob('*-linux-gnu')):
            candidates.extend(base.glob('nautilus/extensions-*/libnautilus-python.so' if manager == 'nautilus' else 'caja/extensions-*/libcaja-python.so'))
    return '' if result.returncode == 0 and candidates else ('python3-nautilus + Nautilus typelib' if manager == 'nautilus' else 'python3-caja + Caja typelib')

def nemo_disabled(value=None):
    result = subprocess.run(['gsettings', 'get', 'org.nemo.plugins', 'disabled-actions'],
        capture_output=True, text=True, timeout=3)
    if result.returncode: raise RuntimeError('Nemo settings unavailable: ' + result.stderr.strip())
    text = result.stdout.strip().removeprefix('@as ')
    entries = ast.literal_eval(text)
    if not isinstance(entries, list) or any(not isinstance(v, str) for v in entries):
        raise RuntimeError('Invalid Nemo disabled-actions preference')
    key = TEMPLATES['nemo']
    previous = key in entries
    if value is not None and previous != value:
        updated = entries + [key] if value else [v for v in entries if v != key]
        result = subprocess.run(['gsettings', 'set', 'org.nemo.plugins', 'disabled-actions', json.dumps(updated)],
            capture_output=True, text=True, timeout=3)
        if result.returncode: raise RuntimeError('Cannot update Nemo preference: ' + result.stderr.strip())
        if nemo_disabled() != value: raise RuntimeError('Nemo preference did not persist; check the desktop settings backend')
    return previous

def global_nemo(data):
    dirs = os.getenv('XDG_DATA_DIRS', '/usr/local/share:/usr/share').split(':')
    for base in dirs:
        if not base or not Path(base).is_absolute(): continue
        path = Path(base) / 'nemo/actions/classic-find-plus.nemo_action'
        if path == data / 'nemo/actions/classic-find-plus.nemo_action': continue
        contents = read(path)
        if contents and b'Exec=classic-find-plus -- %F' in contents and b'Active=false' not in contents:
            return True
    return False

def status(manager, entry, data, config):
    path = path_for(manager, data, config, entry)
    contents = read(path)
    part = xml_action(contents) if manager == 'thunar' else None
    active = part[2] if part else (None if manager == 'thunar' else contents)
    legacy = manager == 'nemo' and global_nemo(data)
    enabled = active is not None and not (manager == 'nemo' and b'Active=false' in active)
    enabled = enabled or (legacy and active is None)
    legacy_caja = read(data / 'caja-python/extensions' / TEMPLATES['caja']) if manager == 'caja' else None
    if legacy_caja is not None and active is None: enabled = True
    if manager == 'caja' and active is not None: enabled = os.access(path, os.X_OK)
    missing = '' if manager == 'caja' else optional(manager)
    if manager == 'nemo' and shutil.which('nemo'):
        try: enabled = enabled and not nemo_disabled()
        except (RuntimeError, OSError, ValueError): missing = 'gsettings / org.nemo.plugins settings schema'
    admin = external = None
    if manager == 'thunar': _, admin, external = thunar_details(contents)
    return {'needs_update': bool(manager == 'thunar' and enabled and not (admin or external)),
        'manager': manager, 'version': version(manager), 'detected': bool(shutil.which('Thunar' if manager == 'thunar' else manager)),
        'installed': bool(entry.get('installed') or active is not None or legacy or legacy_caja is not None), 'enabled': enabled,
        'missing': missing, 'reload': bool(entry.get('reload')), 'legacy_system': legacy,
        'path': str(path), 'method': ('Find + Open as Administrator' if admin else 'Find + independent administrator action' if external else 'Administrator action missing — use Repair actions' if enabled else 'Enabling installs Find + Open as Administrator') if manager == 'thunar' else ('Caja Python extension (switch available)' if legacy_caja is not None and active is None else 'Caja Scripts menu') if manager == 'caja' else '',
        'legacy_extension': legacy_caja is not None,
        'managed': digest(active if active is not None else legacy_caja) == entry.get('hash') if active is not None or legacy_caja is not None else False}

def validate_state(state):
    if not isinstance(state, dict) or not set(state) <= set(MANAGERS):
        raise RuntimeError('Invalid integration ownership state')
    for entry in state.values():
        if not isinstance(entry, dict) or not set(entry) <= {'installed', 'reload', 'legacy', 'hash', 'nemo_original_disabled', 'nemo_last_disabled', 'caja_script', 'admin_hash'}:
            raise RuntimeError('Invalid integration ownership entry')
        for key, value in entry.items():
            if key in ('hash', 'admin_hash'):
                if value is not None and (not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value)):
                    raise RuntimeError('Invalid integration fingerprint')
            elif not isinstance(value, bool): raise RuntimeError('Invalid integration preference')
    return state

def main():
    operation = sys.argv[1] if len(sys.argv) > 1 else 'status'
    manager = sys.argv[2] if len(sys.argv) > 2 else None
    if operation not in ('status', 'install', 'enable', 'disable', 'remove') or (operation != 'status' and manager not in MANAGERS):
        raise RuntimeError('Usage: integration-manager.py status | install/enable/disable/remove MANAGER')
    data = Path(os.getenv('XDG_DATA_HOME', str(Path.home() / '.local/share')))
    config = Path(os.getenv('XDG_CONFIG_HOME', str(Path.home() / '.config')))
    state_path = data / 'classic-find-plus/integration-state.json'
    safe(state_path)
    # Read-only status must not create an integration or preferences.
    state_bytes = read(state_path)
    state = validate_state(json.loads(state_bytes) if state_bytes else {})
    if operation == 'status':
        rows = [status(m, state.get(m, {}), data, config) for m in MANAGERS]
        if '--keyfile' in sys.argv:
            keys = configparser.ConfigParser(interpolation=None)
            for row in rows:
                keys[row['manager']] = {k: str(v).lower() if isinstance(v, bool) else str(v)
                    for k, v in row.items() if k in ('detected', 'installed', 'enabled', 'reload', 'missing', 'version', 'method', 'legacy_extension', 'needs_update')}
            out = io.StringIO(); keys.write(out, space_around_delimiters=False); print(out.getvalue())
        else: print(json.dumps(rows))
        return
    lock_path = state_path.with_suffix('.lock'); safe(lock_path)
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with lock_path.open('a') as lock:
        os.chmod(lock_path, 0o600); fcntl.flock(lock, fcntl.LOCK_EX)
        # Reload under lock; never save a stale process snapshot.
        state_bytes = read(state_path); state = validate_state(json.loads(state_bytes) if state_bytes else {})
        entry = state.get(manager, {})
        path = path_for(manager, data, config, entry); original = read(path)
        span = xml_action(original) if manager == 'thunar' else None
        active = span[2] if span else (None if manager == 'thunar' else original)
        template = read(RESOURCE / ('classic-find-plus-caja-script.py' if manager == 'caja' else TEMPLATES[manager]))
        if not template: raise RuntimeError('Integration template missing')
        legacy_path = data / 'caja-python/extensions' / TEMPLATES['caja']
        legacy_caja = read(legacy_path) if manager == 'caja' else None
        if legacy_caja is not None and not (
            (not entry.get('caja_script') and digest(legacy_caja) == entry.get('hash')) or
            legacy_caja == read(RESOURCE / TEMPLATES['caja'])):
            raise RuntimeError('Untracked or modified Caja extension; refusing replacement/removal: ' + str(legacy_path))
        if manager == 'thunar': template = template.strip()
        if active is not None and digest(active) != entry.get('hash'):
            # Adopt only byte-exact pre-existing CFP templates; preserve changes/unrelated files.
            if active != template: raise RuntimeError('Untracked or modified integration; refusing overwrite/removal: ' + str(path))
            entry['hash'] = digest(active); entry['installed'] = True
        if manager == 'thunar':
            entry = thunar_update(operation, path, original, entry)
        elif operation == 'install':
            entry['installed'] = True  # Resources are already installed; no activation.
        elif operation in ('enable', 'disable'):
            if not entry.get('installed') and not (manager == 'nemo' and global_nemo(data)):
                raise RuntimeError('Install this integration first')
            new = template if operation == 'enable' else None
            if manager == 'nemo':
                original_disabled = nemo_disabled()
                entry.setdefault('nemo_original_disabled', original_disabled)
                desired_disabled = operation == 'disable'
                nemo_disabled(desired_disabled)
                entry['nemo_last_disabled'] = desired_disabled
                if global_nemo(data): new = None  # Keep one global menu entry; no duplicate user copy.
            if new is not None:
                mode = 0o755 if manager in ('dolphin', 'caja') else 0o644
                if active != new or path.stat().st_mode & 0o7777 != mode: atomic(path, new, original, mode)
            elif original is not None:
                if read(path) != original: raise RuntimeError('Integration changed during removal')
                path.unlink()
            entry.update(installed=True, hash=digest(new), reload=True)
            if manager == 'dolphin': entry['legacy'] = 'kservices5' in path.parts
        else:
            if manager == 'nemo' and 'nemo_original_disabled' in entry:
                if nemo_disabled() == entry.get('nemo_last_disabled'):
                    nemo_disabled(entry['nemo_original_disabled'])
            if original is not None:
                if read(path) != original: raise RuntimeError('Integration changed during removal')
                path.unlink()
            entry = {'reload': bool(active is not None or entry.get('reload'))}
        if manager == 'caja' and operation != 'install':
            if legacy_caja is not None:
                if read(legacy_path) != legacy_caja: raise RuntimeError('Caja extension changed during replacement')
                legacy_path.unlink()
                entry['reload'] = True
            entry['caja_script'] = True
        state[manager] = entry
        atomic(state_path, (json.dumps(state, sort_keys=True, indent=2) + '\n').encode(), state_bytes)
    print(json.dumps(status(manager, entry, data, config)))

if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError, expat.ExpatError, subprocess.TimeoutExpired) as error:
        sys.exit('Integration update stopped: ' + str(error))
