#!/usr/bin/env python3
"""Apply the exact v23-based v24 overlay into a new archive (never overwrite)."""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

MANIFEST='V24_OVERLAY_MANIFEST.json'
def sha(data): return hashlib.sha256(data).hexdigest()

def apply(base, overlay, destination):
    base,overlay,destination=map(Path,(base,overlay,destination))
    if destination.exists():raise ValueError('Destination already exists')
    with ZipFile(base) as origin,ZipFile(overlay) as patch:
        record=json.loads(patch.read(MANIFEST))
        if sha(base.read_bytes())!=record['base_zip_sha256']:
            raise ValueError('Base archive checksum mismatch')
        changed=record['changed']; added=record['added']
        expected=set(changed)|set(added)
        if set(patch.namelist())!=expected|{MANIFEST}:
            raise ValueError('Overlay member mismatch')
        base_members=set(origin.namelist())
        if not set(changed)<=base_members or set(added)&base_members:
            raise ValueError('Changed/new paths do not match baseline')
        if any(not name or name.startswith('/') or '\\' in name or ':' in name or '..' in name.split('/') for name in expected):
            raise ValueError('Unsafe overlay path')
        for name,meta in changed.items():
            if sha(origin.read(name))!=meta['before_sha256'] or sha(patch.read(name))!=meta['after_sha256']:
                raise ValueError('Changed checksum mismatch: '+name)
        for name,checksum in added.items():
            if sha(patch.read(name))!=checksum:raise ValueError('Added checksum mismatch: '+name)
        with ZipFile(destination,'w',ZIP_DEFLATED,compresslevel=6,allowZip64=True) as out:
            for member in origin.infolist():
                name=member.filename
                out.writestr(member,patch.read(name) if name in changed else origin.read(name))
            for name in sorted(added):
                info=ZipInfo(name,(2026,10,22,12,0,0));info.compress_type=ZIP_DEFLATED
                out.writestr(info,patch.read(name))
    with ZipFile(destination) as result:
        if len(result.namelist())!=record['expected_files'] or result.testzip() is not None:
            destination.unlink(missing_ok=True)
            raise ValueError('Replayed archive integrity failure')
    return destination

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',required=True);ap.add_argument('--overlay',required=True);ap.add_argument('--output',required=True)
    args=ap.parse_args()
    print(apply(args.base,args.overlay,args.output))
