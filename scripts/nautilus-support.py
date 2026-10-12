#!/usr/bin/env python3
"""Isolated, bounded Nautilus support probe; imports only CFP's supplied template."""
import ctypes
import importlib.util
import json
import os
from pathlib import Path
import sys


def probe(template, expected, provider_roots=None):
    import gi
    # Match the application/provider ABI; do not pick an unrelated installed typelib.
    choices = ('3.0',) if expected == '3' else ('4.1', '4.0') if expected == '4' else ('4.1', '4.0', '3.0')
    api = None
    for value in choices:
        try:
            gi.require_version('Nautilus', value)
            from gi.repository import Nautilus
            api = value
            break
        except (ValueError, ImportError):
            continue
    if api is None:
        raise RuntimeError('Matching Nautilus typelib unavailable')
    # Verify native library dependencies, not just a filename/package registration.
    candidates = []
    for root in (provider_roots if provider_roots is not None else
                 (Path('/usr/lib'), Path('/usr/lib64'), Path('/usr/local/lib'))):
        for base in (root, *root.glob('*-linux-gnu')):
            for path in base.glob('nautilus/extensions-*/libnautilus-python.so'):
                dirname = path.parent.name
                if (api == '3.0' and dirname == 'extensions-3.0') or (api != '3.0' and dirname.startswith('extensions-4')):
                    candidates.append(path)
    loaded = None
    failure = None
    for path in candidates:
        try:
            loaded = ctypes.CDLL(str(path)); break
        except OSError as error:
            failure = str(error)
            continue
    if loaded is None:
        raise RuntimeError('Matching python3-nautilus provider missing or native dependencies cannot load' +
                           (': ' + failure if failure else ''))
    spec = importlib.util.spec_from_file_location('cfp_nautilus_probe', template)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    module.ClassicFindPlusNautilus()  # Exercise actual GObject/interface registration.
    Nautilus.MenuItem(name='ClassicFindPlus::probe', label='CFP support probe')
    return {'ready': True, 'api': api, 'provider': str(path), 'admin': module._external_admin or '',
            'detail': 'API ' + api + ' import/provider checks pass; restart/menu verification required'}


if __name__ == '__main__':
    try:
        result = probe(sys.argv[1], sys.argv[2])
    except Exception as error:
        result = {'ready': False, 'api': '', 'admin': '', 'detail': str(error)}
    print(json.dumps(result))
