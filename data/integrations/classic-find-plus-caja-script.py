#!/usr/bin/python3
"""Caja Scripts launcher: one local folder or the current folder, never a shell."""
import os
from pathlib import Path
import sys
from urllib.parse import unquote_to_bytes, urlsplit


def local_folder(uri):
    parts = urlsplit(uri)
    if parts.scheme != 'file' or parts.netloc not in ('', 'localhost') or parts.query or parts.fragment:
        raise ValueError('Select a local folder to search.')
    path = os.fsdecode(unquote_to_bytes(parts.path))
    if '\0' in path or not os.path.isabs(path) or not os.path.isdir(path):
        raise ValueError('Select one local folder to search.')
    return path


def main():
    # URI separators are unambiguous even when a filename itself contains a newline.
    selected = os.environ.get('CAJA_SCRIPT_SELECTED_URIS', '')
    if selected:
        uris = selected.removesuffix('\n').split('\n')
        if len(uris) != 1: raise ValueError('Select one folder to search.')
        folder = local_folder(uris[0])
    elif sys.argv[1:]:
        if len(sys.argv) != 2: raise ValueError('Select one folder to search.')
        folder = str(Path(sys.argv[1]).absolute())
        if not os.path.isdir(folder): raise ValueError('Select one folder to search.')
    else:
        folder = local_folder(os.environ.get('CAJA_SCRIPT_CURRENT_URI', ''))
    os.execvp('classic-find-plus', ['classic-find-plus', '--', folder])


if __name__ == '__main__':
    try: main()
    except (OSError, ValueError) as error:
        sys.exit('Classic Find Plus: ' + str(error))
