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

def atomic(path, data, previous, mode=0o600):
    safe(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, name = tempfile.mkstemp(prefix='.cfp-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(data); out.flush(); os.fsync(out.fileno())
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

def xml_action(data):
    if data is None: return None
    if b'<!DOCTYPE' in data.upper() or b'<!ENTITY' in data.upper():
        raise RuntimeError('Thunar XML entities/DTD refused')
    parser = expat.ParserCreate()
    depth, start, unique, text, spans = 0, None, False, '', []
    def begin(name, attrs):
        nonlocal depth, start, unique, text
        depth += 1
        if depth > 32: raise RuntimeError('Thunar XML nesting limit')
        if depth == 1 and name != 'actions': raise RuntimeError('Expected Thunar actions XML')
        if depth == 2 and name == 'action': start = parser.CurrentByteIndex; text = ''
        if depth == 3 and name == 'unique-id' and start is not None: unique = True
    def content(value):
        nonlocal text
        if unique: text += value
    def end(name):
        nonlocal depth, start, unique
        if depth == 3 and name == 'unique-id': unique = False
        if depth == 2 and name == 'action' and start is not None:
            if text.strip() == UID:
                stop = data.index(b'>', parser.CurrentByteIndex) + 1
                spans.append((start, stop, data[start:stop]))
            start = None
        depth -= 1
    def no_doctype(*args): raise RuntimeError('Thunar XML DTD refused')
    parser.StartDoctypeDeclHandler = no_doctype
    if '\0' in data.decode('utf-8'): raise RuntimeError('Thunar XML must use UTF-8')
    parser.StartElementHandler = begin; parser.EndElementHandler = end; parser.CharacterDataHandler = content
    parser.Parse(data, True)
    if len(spans) > 1: raise RuntimeError('Duplicate CFP Thunar actions; resolve manually')
    return spans[0] if spans else None

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
    return {'manager': manager, 'version': version(manager), 'detected': bool(shutil.which('Thunar' if manager == 'thunar' else manager)),
        'installed': bool(entry.get('installed') or active is not None or legacy or legacy_caja is not None), 'enabled': enabled,
        'missing': missing, 'reload': bool(entry.get('reload')), 'legacy_system': legacy,
        'path': str(path), 'method': ('Caja Python extension (switch available)' if legacy_caja is not None and active is None else 'Caja Scripts menu') if manager == 'caja' else '',
        'legacy_extension': legacy_caja is not None,
        'managed': digest(active if active is not None else legacy_caja) == entry.get('hash') if active is not None or legacy_caja is not None else False}

def validate_state(state):
    if not isinstance(state, dict) or not set(state) <= set(MANAGERS):
        raise RuntimeError('Invalid integration ownership state')
    for entry in state.values():
        if not isinstance(entry, dict) or not set(entry) <= {'installed', 'reload', 'legacy', 'hash', 'nemo_original_disabled', 'nemo_last_disabled', 'caja_script'}:
            raise RuntimeError('Invalid integration ownership entry')
        for key, value in entry.items():
            if key == 'hash':
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
                    for k, v in row.items() if k in ('detected', 'installed', 'enabled', 'reload', 'missing', 'version', 'method', 'legacy_extension')}
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
        if operation == 'install':
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
            if manager == 'thunar':
                xml = original or b'<?xml version="1.0" encoding="UTF-8"?>\n<actions>\n</actions>\n'
                if span: xml = xml[:span[0]] + xml[span[1]:]
                if new:
                    end = xml.rfind(b'</actions>')
                    if end < 0: raise RuntimeError('Thunar actions closing tag missing')
                    xml = xml[:end] + new + xml[end:]
                if original != xml: atomic(path, xml, original)
            elif new is not None:
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
            if manager == 'thunar':
                if span: atomic(path, original[:span[0]] + original[span[1]:], original)
            elif original is not None:
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
