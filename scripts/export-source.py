#!/usr/bin/env python3
"""Export the maintained public allowlist into a new directory, never an existing tree."""
from pathlib import Path, PurePosixPath
import shutil, sys
ROOT=Path(__file__).resolve().parent.parent

def entries():
    lines=(ROOT/'public-files.txt').read_text(encoding='utf-8').splitlines()
    seen=set(); result=[]
    for line in lines:
        if not line or line.startswith('#'): continue
        p=PurePosixPath(line)
        if str(p)!=line or p.is_absolute() or any(x in ('..','.git') for x in p.parts) or '\\' in line or ':' in line:
            raise ValueError('Invalid public path: '+line)
        if line.casefold() in seen: raise ValueError('Duplicate public path: '+line)
        seen.add(line.casefold())
        source=ROOT/line
        if not source.is_file(): raise ValueError('Missing source: '+line)
        for part in [source,*source.parents]:
            if part==ROOT: break
            if part.is_symlink(): raise ValueError('Source symlink: '+str(part))
        result.append(line)
    return result

def main():
    if len(sys.argv)!=2: raise ValueError('Usage: python3 scripts/export-source.py NEW_DIRECTORY')
    files=entries();dest=Path(sys.argv[1]).absolute()
    if dest.exists() or dest.is_symlink(): raise ValueError('Destination must not exist')
    for p in dest.parents:
        if p.is_symlink(): raise ValueError('Destination ancestor is a symlink')
    dest.mkdir(parents=True)
    for relative in files:
        target=dest/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/relative,target)
    print('Exported',len(files),'public source files to',dest)

if __name__=='__main__':
    try:main()
    except (OSError,ValueError) as e:sys.exit(str(e))
