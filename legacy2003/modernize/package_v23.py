#!/usr/bin/env python3
"""Write a reproducible exact-v22-based v23 archive and hash-checked overlay."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, ZIP_DEFLATED, ZipInfo

ROOT=Path(__file__).resolve().parents[1]
MANIFEST='V23_OVERLAY_MANIFEST.json'

def sha(data):return hashlib.sha256(data).hexdigest()

def safe(name):
    p=PurePosixPath(name)
    return bool(name) and not name.startswith('/') and not name.startswith('\\') and \
        '\\' not in name and ':' not in name and all(x not in ('','.','..') for x in p.parts)

def package(base,source,overlay):
    base,source,overlay=Path(base),Path(source),Path(overlay)
    if source.exists() or overlay.exists():raise ValueError('Refusing to overwrite an existing archive')
    with ZipFile(base) as original:
        names=original.namelist()
        if len(names)!=len(set(names)) or not all(map(safe,names)):
            raise ValueError('Unsafe or duplicate base names')
        baseline={name:original.read(name) for name in names}
        members={}
        for f in ROOT.rglob('*'):
            if not f.is_file() or f.is_symlink() or '__pycache__' in f.parts or f.suffix in ('.pyc','.pyo'):
                continue
            key=f.relative_to(ROOT).as_posix()
            if not safe(key):raise ValueError('Unsafe local path: '+key)
            members[key]=f.read_bytes()
        removed=set(baseline)-set(members)
        if removed:raise ValueError('Missing original members: '+repr(sorted(removed)[:6]))
        changed={n:{'before_sha256':sha(baseline[n]),'after_sha256':sha(members[n])}
                 for n in sorted(set(baseline)&set(members)) if baseline[n]!=members[n]}
        added={n:sha(members[n]) for n in sorted(set(members)-set(baseline))}
        manifest={'base_zip_sha256':sha(base.read_bytes()), 'changed':changed,'added':added,
                  'expected_files':len(baseline)+len(added)}
        with ZipFile(overlay,'w',ZIP_DEFLATED,compresslevel=6,allowZip64=True) as oz:
            for name in sorted(set(changed)|set(added)):
                info=ZipInfo(name,(2026,10,22,12,0,0)); info.compress_type=ZIP_DEFLATED
                oz.writestr(info,members[name])
            oz.writestr(MANIFEST,json.dumps(manifest,indent=2,sort_keys=True)+'\n')
        with ZipFile(source,'w',ZIP_DEFLATED,compresslevel=6,allowZip64=True) as output:
            for item in original.infolist():
                output.writestr(item,members[item.filename])
            for name in sorted(added):
                info=ZipInfo(name,(2026,10,22,12,0,0)); info.compress_type=ZIP_DEFLATED
                output.writestr(info,members[name])
    with ZipFile(source) as s,ZipFile(overlay) as o:
        assert s.testzip() is None and o.testzip() is None
        assert set(s.namelist())==set(members)
        assert all(s.read(n)==data for n,data in members.items())
        assert len(o.namelist())==len(changed)+len(added)+1
    print('v23 source:',len(members),'files; updated:',len(changed),'added:',len(added))

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--base',required=True);ap.add_argument('--source',required=True)
    ap.add_argument('--overlay',required=True)
    args=ap.parse_args()
    package(args.base,args.source,args.overlay)
