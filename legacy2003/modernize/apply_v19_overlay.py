#!/usr/bin/env python3
"""Apply a hash-checked v19 overlay to the exact v18 source ZIP.

Creates a new ZIP without extracting untrusted archive paths to disk.
The overlay never contains proprietary game-content assets.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo

MANIFEST='V19_OVERLAY_MANIFEST.json'

def sha(data): return hashlib.sha256(data).hexdigest()

def safe(name):
    p=PurePosixPath(name)
    return bool(name) and not name.startswith('/') and not name.startswith('\\') and \
        '\\' not in name and ':' not in name and all(x not in ('','.','..') for x in p.parts)

def apply(base, overlay, output):
    base,overlay,output=Path(base),Path(overlay),Path(output)
    if output.exists(): raise ValueError('Refusing overwrite of existing output')
    with ZipFile(base) as bz, ZipFile(overlay) as oz:
        if bz.testzip() or oz.testzip(): raise ValueError('ZIP CRC failed')
        bnames=bz.namelist(); onames=oz.namelist()
        if len(set(bnames))!=len(bnames) or len(set(onames))!=len(onames):
            raise ValueError('Duplicate archive member')
        if not all(map(safe,bnames+onames)): raise ValueError('Unsafe archive path')
        data=json.loads(oz.read(MANIFEST))
        if sha(base.read_bytes())!=data['base_zip_sha256']:
            raise ValueError('The overlay requires the exact v18 source ZIP')
        changed=data['changed']; added=data['added']; patched=set(changed)|set(added)
        if set(onames)!=(patched|{MANIFEST}):
            raise ValueError('Overlay members differ from manifest')
        if any(k not in bnames for k in changed) or any(k in bnames for k in added):
            raise ValueError('Unexpected baseline member set')
        for name, old in changed.items():
            if sha(bz.read(name))!=old['before_sha256']:
                raise ValueError('Base content mismatch: '+name)
        for name in patched:
            expected=changed[name]['after_sha256'] if name in changed else added[name]
            if sha(oz.read(name))!=expected:
                raise ValueError('Overlay content mismatch: '+name)
        with ZipFile(output, 'w', compression=ZIP_DEFLATED, compresslevel=6) as result:
            for item in bz.infolist():
                result.writestr(item, oz.read(item.filename) if item.filename in changed else bz.read(item.filename))
            for name in sorted(added):
                info=ZipInfo(name,(2026,10,22,12,0,0)); info.compress_type=ZIP_DEFLATED
                result.writestr(info,oz.read(name))
        with ZipFile(output) as rebuilt:
            if rebuilt.testzip(): raise ValueError('Output CRC failed')
            if len(rebuilt.namelist())!=len(bnames)+len(added):
                raise ValueError('Output member count mismatch')
    print('Reproduced v19 source members:',len(bnames)+len(added))

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--base',required=True)
    parser.add_argument('--overlay',required=True)
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    apply(args.base,args.overlay,args.output)
