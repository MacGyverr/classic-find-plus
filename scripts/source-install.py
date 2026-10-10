#!/usr/bin/env python3
"""Conservative Make source installation; never use a manifest as package ownership proof."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
DOCS = ['admin-search.md', 'integrations.md', 'search.md', 'file-actions.md', 'permissions.md', 'settings.md', 'results-sync.md',
        'export.md', 'exclusion.md', 'install.md', 'publishing.md', 'packaging.md']
STATE = 'share/classic-find-plus/source-install.json'

def fail(message):
    raise RuntimeError(message)

def absolute(value, label):
    p = Path(value)
    if not p.is_absolute() or '..' in p.parts or str(p) == '/' or any(c in str(p) for c in '*?[]\n\r'):
        fail(label + ' must be an absolute non-root path without .. or package-query glob characters')
    return p

def safe_path(path):
    """Reject all existing symlink components, including dangling links."""
    for part in [*reversed(path.parents), path]:
        if part.is_symlink():
            fail('Symlink installation path refused: ' + str(part))
        if part.exists() and part != path and not part.is_dir():
            fail('Non-directory ancestor: ' + str(part))

def plan():
    entries = {'bin/classic-find-plus': 'build-make/classic-find-plus',
               'bin/classic-find': '@classic-find-plus',
               'share/applications/org.classicfind.Plus.desktop': 'data/desktop/org.classicfind.Plus.desktop',
               'share/icons/hicolor/16x16/apps/classic-find-plus.png': 'data/artwork/icon.png'}
    entries.update({
        'libexec/classic-find-plus-search-worker': 'build-make/classic-find-plus-search-worker',
        'share/dbus-1/system-services/org.classicfind.Plus.Search.service': 'build-make/org.classicfind.Plus.Search.service',
        'share/dbus-1/system.d/org.classicfind.Plus.Search.conf': 'data/privileged/org.classicfind.Plus.Search.conf',
        'share/polkit-1/actions/org.classicfind.plus.search.policy': 'data/privileged/org.classicfind.plus.search.policy',
        'share/classic-find-plus/integration-manager.py': 'scripts/integration-manager.py'})
    for name in ['classic-find-plus.nemo_action', 'classic-find-plus-nautilus.py', 'classic-find-plus-caja.py',
                 'classic-find-plus.desktop', 'classic-find-plus-thunar.xml', 'classic-find-plus-caja-script.py']:
        entries['share/classic-find-plus/integrations/' + name] = 'data/integrations/' + name
    for name in ['LICENSE', 'README.md', 'readme.txt', 'CHANGELOG.md', 'THIRD_PARTY.md']:
        entries['share/doc/classic-find-plus/' + name] = name
    for name in DOCS:
        entries['share/doc/classic-find-plus/docs/' + name] = 'docs/' + name
    return entries

def fingerprint(path):
    if path.is_symlink():
        return 'link:' + os.readlink(path)
    if not path.exists():
        return None
    if not path.is_file():
        fail('Not a regular file: ' + str(path))
    return 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest()

def package_owned(logical, stage):
    """Check the live package database, or only the staged database with DESTDIR."""
    checks = []
    dpkg_db = stage / 'var/lib/dpkg' if stage else Path('/var/lib/dpkg')
    if dpkg_db.is_dir():
        if not shutil.which('dpkg-query'):
            fail('Cannot check dpkg ownership: dpkg-query missing')
        checks.append(['dpkg-query', '--admindir=' + str(dpkg_db), '--search', str(logical)])
    rpm_db = next((p for p in ([stage / 'usr/lib/sysimage/rpm', stage / 'var/lib/rpm'] if stage else
                              [Path('/usr/lib/sysimage/rpm'), Path('/var/lib/rpm')]) if p.is_dir()), None)
    if rpm_db:
        if not shutil.which('rpm'):
            fail('Cannot check RPM ownership: rpm missing')
        checks.append(['rpm', '--dbpath', str(rpm_db), '-qf', '--', str(logical)])
    for command in checks:
        result = subprocess.run(command, text=True, capture_output=True, env=dict(os.environ, LC_ALL='C'))
        if result.returncode == 0:
            fail('Package-manager-owned path: ' + str(logical) + '. Remove using apt/dpkg or dnf/rpm, not source uninstall/install.')
        # Distinguish normal not-owned status from unreadable/corrupt database failures.
        message = result.stderr + result.stdout
        normal = ('no path found matching pattern' in message if command[0] == 'dpkg-query'
                  else 'not owned by any package' in message)
        if result.returncode != 1 or not normal:
            fail('Package ownership check failed: ' + message.strip())

def write_state(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix='.cfp-manifest-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as out:
            json.dump(data, out, sort_keys=True, indent=2)
            out.write('\n')
        os.replace(temp, path)
    finally:
        if os.path.exists(temp): os.unlink(temp)

def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ('install', 'uninstall'):
        fail('Usage: source-install.py install|uninstall (PREFIX and DESTDIR environment variables)')
    prefix = absolute(os.environ.get('PREFIX', '/usr/local'), 'PREFIX')
    stage_text = os.environ.get('DESTDIR', '')
    stage = absolute(stage_text, 'DESTDIR') if stage_text else None
    target = stage / str(prefix).lstrip('/') if stage else prefix
    safe_path(target)
    paths = plan()
    legacy_nemo = 'share/nemo/actions/classic-find-plus.nemo_action'
    manifest = target / STATE
    safe_path(manifest)
    previous = {}
    if manifest.exists():
        if manifest.stat().st_size > 65536: fail('Oversized installation manifest')
        data = json.loads(manifest.read_text())
        if data.get('format') != 1 or data.get('prefix') != str(prefix): fail('Invalid installation manifest')
        previous = data.get('files', {})
        if not isinstance(previous, dict) or not set(previous) <= set(paths) | {legacy_nemo}: fail('Unapproved manifest path')
        if any(not isinstance(v, str) for v in previous.values()): fail('Invalid manifest hashes')
    if legacy_nemo in previous:
        # Keep tracked legacy integration/choice on source upgrades; fresh installs omit it.
        paths[legacy_nemo] = '!keep'
    if sys.argv[1] == 'uninstall' and not previous:
        fail('No Make installation manifest for this prefix. Use the original installer/package manager.')
    # Preflight ALL paths before modifying any file. A stale manifest is never sufficient.
    for relative in set(paths) | {STATE}:
        logical = prefix / relative
        dest = target / relative
        safe_path(dest.parent)
        package_owned(logical, stage)
        if relative == STATE:
            safe_path(dest)
            continue
        current = fingerprint(dest)
        if current is not None and (relative not in previous or current != previous[relative]):
            fail('Untracked or changed installed file; refusing overwrite/removal: ' + str(dest))
    if sys.argv[1] == 'uninstall':
        for relative, expected in previous.items():
            dest = target / relative
            if fingerprint(dest) is not None:
                # Recheck ownership and content immediately before each unlink.
                package_owned(prefix / relative, stage)
                if fingerprint(dest) != expected: fail('File changed during uninstall: ' + str(dest))
                dest.unlink()
                print('Removed', dest)
        manifest.unlink()
        # Never recursively remove directories; unrelated data/preferences are retained.
        for directory in sorted({(target / p).parent for p in previous} | {manifest.parent}, key=lambda p: len(p.parts), reverse=True):
            try: directory.rmdir()
            except OSError: pass
        return
    prepared = []
    try:
        for relative, source in paths.items():
            dest = target / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            fd, temp_name = tempfile.mkstemp(prefix='.cfp-install-', dir=dest.parent)
            os.close(fd)
            temp = Path(temp_name)
            prepared.append((relative, dest, temp))
            if source == '!keep':
                shutil.copyfile(dest, temp); temp.chmod(0o644)
            elif source.startswith('@'):
                temp.unlink(); temp.symlink_to(source[1:])
            else:
                src = ROOT / source
                safe_path(src)
                shutil.copyfile(src, temp)
                temp.chmod(0o755 if relative.startswith(('bin/', 'libexec/')) else 0o644)
        updated = dict(previous)
        for relative, dest, temp in prepared:
            package_owned(prefix / relative, stage)
            if fingerprint(dest) != previous.get(relative): fail('File changed during install: ' + str(dest))
            expected = fingerprint(temp)
            os.replace(temp, dest)
            updated[relative] = expected
            write_state(manifest, {'format': 1, 'prefix': str(prefix), 'version': (ROOT/'VERSION').read_text().strip(), 'files': updated})
            print('Installed', dest)
    finally:
        for _, _, temp in prepared:
            if temp.exists() or temp.is_symlink(): temp.unlink()

if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError) as error:
        sys.exit('Source installation stopped: ' + str(error))
