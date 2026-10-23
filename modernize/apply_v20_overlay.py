#!/usr/bin/env python3
"""Rebuild v20 ZIP member-for-member from the exact v19 full source and v20 overlay.

No external requests. Validates hashes, archive paths and member-set integrity.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo

MANIFEST = 'V20_OVERLAY_MANIFEST.json'

def sha(data): return hashlib.sha256(data).hexdigest()

def safe(name):
    p=PurePosixPath(name)
    return bool(name) and not name.startswith('/') and not name.startswith('\\') and \
        '\\' not in name and ':' not in name and all(x not in ('','.','..') for x in p.parts)

def apply(base,overlay,output):
    base,overlay,output = Path(base),Path(overlay),Path(output)
    if output.exists(): raise ValueError('Destination exists; refusing overwrite')
    with ZipFile(base) as bz, ZipFile(overlay) as oz:
        if bz.testzip() or oz.testzip(): raise ValueError('Archive CRC failure')
        bnames=bz.namelist(); onames=oz.namelist()
        if len(set(bnames))!=len(bnames) or len(set(onames))!=len(onames):
            raise ValueError('Duplicate archive entry')
        if not all(map(safe,bnames+onames)): raise ValueError('Unsafe archive path')
        manifest=json.loads(oz.read(MANIFEST))
        if sha(base.read_bytes())!=manifest['base_zip_sha256']:
            raise ValueError('This overlay requires the exact v19 source ZIP')
        changed=manifest['changed']; added=manifest['added']
        files=set(changed)|set(added)
        if set(onames)!=files|{MANIFEST}: raise ValueError('Unexpected overlay member set')
        if any(n not in bnames for n in changed) or any(n in bnames for n in added):
            raise ValueError('Unexpected base member set')
        for name in changed:
            if sha(bz.read(name))!=changed[name]['before_sha256']:
                raise ValueError('Base mismatch: '+name)
        for name in files:
            expected=changed[name]['after_sha256'] if name in changed else added[name]
            if sha(oz.read(name))!=expected: raise ValueError('Overlay mismatch: '+name)
        with ZipFile(output,'w',compression=ZIP_DEFLATED,compresslevel=6,allowZip64=True) as built:
            for item in bz.infolist():
                built.writestr(item,oz.read(item.filename) if item.filename in changed else bz.read(item.filename))
            for name in sorted(added):
                item=ZipInfo(name,(2026,10,22,12,0,0)); item.compress_type=ZIP_DEFLATED
                built.writestr(item,oz.read(name))
        with ZipFile(output) as result:
            if result.testzip(): raise ValueError('Output ZIP CRC failure')
            if len(result.namelist())!=len(bnames)+len(added):
                raise ValueError('Output member count mismatch')
            for n in files:
                if sha(result.read(n)) != (changed[n]['after_sha256'] if n in changed else added[n]):
                    raise ValueError('Output member differs: '+n)
        print('Successfully reproduced',len(bnames)+len(added),'v20 source files')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--base',required=True); p.add_argument('--overlay',required=True)
    p.add_argument('--output',required=True)
    a=p.parse_args()
    apply(a.base,a.overlay,a.output)
