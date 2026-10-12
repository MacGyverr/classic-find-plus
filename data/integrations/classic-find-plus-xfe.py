#!/usr/bin/python3
"""XFE Scripts: search a directory, a selected file's parent, or the panel cwd."""
import os
from pathlib import Path
import sys
from urllib.parse import urlsplit, unquote_to_bytes


def folder_for(arguments):
    if len(arguments) > 1:
        raise ValueError('Select one folder or file, or use the current folder background.')
    value = arguments[0] if arguments else os.getcwd()
    if value.startswith('file:') and not os.path.isabs(value) and not Path(value).exists():
        uri = urlsplit(value)
        if (uri.scheme != 'file' or uri.netloc.lower() not in ('', 'localhost') or
                uri.query or uri.fragment):
            raise ValueError('Only local file locations are supported.')
        value = os.fsdecode(unquote_to_bytes(uri.path))
        if not os.path.isabs(value): raise ValueError('An absolute local file URI is required.')
    if '\0' in value: raise ValueError('Invalid local path.')
    path = Path(value).absolute()
    if path.is_dir(): return str(path)
    if path.is_file(): return str(path.parent)
    raise ValueError('Select an existing local folder or regular file.')


def main():
    folder = folder_for(sys.argv[1:])
    os.execvp('classic-find-plus', ['classic-find-plus', '--', folder])


if __name__ == '__main__':
    try: main()
    except (OSError, ValueError) as error: sys.exit('Classic Find Plus: ' + str(error))
