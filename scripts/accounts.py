#!/usr/bin/python3
"""Read-only NSS account catalog, isolated from GTK and bounded by its caller."""
import base64
import grp
import pwd
import resource
import sys

resource.setrlimit(resource.RLIMIT_AS, (256 * 1024 * 1024,) * 2)
resource.setrlimit(resource.RLIMIT_CPU, (5, 5))
output = bytearray(b'CFP-ACCOUNTS-1\n')
try:
    for kind, enumerate_accounts, id_field, name_field in (
        ('u', pwd.getpwall, 'pw_uid', 'pw_name'),
        ('g', grp.getgrall, 'gr_gid', 'gr_name'),
    ):
        # Python's pwd/grp enumeration uses the configured NSS database.
        entries = enumerate_accounts()
        if len(entries) > 65536:
            raise ValueError("Account catalog exceeds 65536 entries.")
        for entry in sorted(entries, key=lambda e: (getattr(e, id_field), getattr(e, name_field))):
            identifier = getattr(entry, id_field)
            name = getattr(entry, name_field).encode('utf-8', errors='replace')
            if not 0 <= identifier < 4294967295 or not name or len(name) > 4096:
                continue
            line = f'{kind}\t{identifier}\t'.encode() + base64.b64encode(name) + b'\n'
            if len(output) + len(line) > 4 * 1024 * 1024:
                raise ValueError('Account catalog exceeds the supported size.')
            output.extend(line)
    sys.stdout.buffer.write(output)
except (OSError, MemoryError, ValueError) as error:
    sys.exit('Account enumeration unavailable: ' + str(error))
