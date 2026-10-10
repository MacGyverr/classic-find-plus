#!/usr/bin/python3
"""dpkg maintscript: retain only the old package's existing system Nemo action.

Copied as preinst/postinst/postrm by build-deb.sh. DPKG_ROOT permits disposable
--force-script-chrootless tests; no host paths are used when it is set.
"""
import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import tempfile

root_text = os.environ.get('DPKG_ROOT', '')
root = Path(root_text or '/')
if not root.is_absolute() or '..' in root.parts:
    sys.exit('Unsafe DPKG_ROOT')
action = root / 'usr/share/nemo/actions/classic-find-plus.nemo_action'
state = root / 'var/lib/classic-find-plus/legacy-nemo.json'

def safe(path):
    for item in [*reversed(path.parents), path]:
        if item.is_symlink(): raise RuntimeError('Symlink migration path refused: ' + str(item))
        if item.exists():
            st = item.stat()
            # Ancestors such as /tmp are acceptable with sticky bit in fixtures;
            # state/action contents themselves must belong to this installer.
            if item.is_relative_to(root) and (st.st_uid != os.geteuid() or st.st_mode & 0o022):
                raise RuntimeError('Unsafe migration file ownership/mode: ' + str(item))

def read(path):
    safe(path)
    if not path.exists(): return None
    if not path.is_file() or path.stat().st_size > 65536: raise RuntimeError('Invalid migration file')
    return path.read_bytes()

def sha(data): return hashlib.sha256(data).hexdigest() if data is not None else None

def write(path, data, mode):
    safe(path)
    if len(data) > 65536: raise RuntimeError('Legacy migration record too large')
    # The backup is private; a restored system action must remain readable by Nemo users.
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700 if path == state else 0o755)
    fd, temp = tempfile.mkstemp(prefix='.cfp-migration-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as out: out.write(data)
        os.chmod(temp, mode); os.replace(temp, path)
    finally:
        if os.path.exists(temp): os.unlink(temp)

try:
    # dpkg uses preinst while unpacking, but PACKAGE.postinst/postrm from its database.
    stage = Path(sys.argv[0]).name.rsplit('.', 1)[-1]
    operation = sys.argv[1] if len(sys.argv) > 1 else ''
    stored = read(state)
    record = json.loads(stored) if stored else None
    if record is not None and (not isinstance(record, dict) or set(record) != {'action', 'hash'} or
        not isinstance(record['action'], str) or sha(record['action'].encode('utf-8')) != record['hash']):
        raise RuntimeError('Invalid legacy Nemo migration record')
    if stage == 'preinst' and operation in ('upgrade', 'install') and len(sys.argv) > 2 and sys.argv[2]:
        owned_list = read(root / 'var/lib/dpkg/info/classic-find-plus.list')
        current = read(action)
        if record is None and current and owned_list and b'/usr/share/nemo/actions/classic-find-plus.nemo_action' in owned_list.splitlines():
            # Preserve disabled/customized legacy actions as well as enabled ones.
            text = current.decode('utf-8')
            write(state, (json.dumps({'action': text, 'hash': sha(current)}) + '\n').encode(), 0o600)
    elif stage == 'postinst' and operation == 'configure' and record:
        current = read(action)
        if current is None: write(action, record['action'].encode('utf-8'), 0o644)
        elif sha(current) != record['hash']:
            print('CFP: preserving independently changed legacy Nemo action', file=sys.stderr)
    elif stage == 'postrm' and operation in ('remove', 'purge') and record:
        current = read(action)
        if current is not None and sha(current) == record['hash']: action.unlink()
        elif current is not None: print('CFP: retaining modified legacy Nemo action', file=sys.stderr)
        state.unlink()
        try: state.parent.rmdir()
        except OSError: pass
except (OSError, ValueError, RuntimeError) as error:
    sys.exit('Legacy Nemo migration stopped: ' + str(error))
