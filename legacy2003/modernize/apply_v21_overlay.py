#!/usr/bin/env python3
"""Reproducibly apply a v21 overlay to the exact v20 source archive.

Output is a new ZIP; the original archive is never modified.
"""
import argparse, hashlib, json, pathlib, zipfile

def sha(b):return hashlib.sha256(b).hexdigest()
def safe(name):
    return (name and not name.startswith('/') and '\\' not in name and
            all(part not in ('','.', '..') for part in name.split('/')))
def apply(base,overlay,output):
    if output.exists(): raise ValueError('refusing to overwrite '+str(output))
    with zipfile.ZipFile(overlay) as patch, zipfile.ZipFile(base) as original:
        manifest=json.loads(patch.read('modernize/release_manifest_v21.json'))
        base_hash=hashlib.sha256(base.read_bytes()).hexdigest()
        if base_hash!=manifest['base_sha256']: raise ValueError('wrong v20 base ZIP checksum')
        changes=manifest['changes']
        source=set(original.namelist())
        if len(source)!=len(original.namelist()):raise ValueError('duplicate paths in base')
        if len(changes)!=len(set(changes)) or any(not safe(n) for n in changes):raise ValueError('unsafe change paths')
        if set(patch.namelist())-{'modernize/release_manifest_v21.json'}!=set(changes):
            raise ValueError('overlay path list mismatch')
        content={n:patch.read(n) for n in changes}
        for n,data in content.items():
            if sha(data)!=changes[n]:raise ValueError('changed file hash mismatch: '+n)
        info_by_name={i.filename:i for i in original.infolist()}
        try:
            with zipfile.ZipFile(output,'w',allowZip64=True) as dest:
                for entry in original.infolist():
                    dest.writestr(entry,content.pop(entry.filename,original.read(entry.filename)))
                for n,data in sorted(content.items()):
                    entry=zipfile.ZipInfo(n,date_time=(2003,9,30,0,0,0))
                    entry.compress_type=zipfile.ZIP_DEFLATED
                    entry.external_attr=(0o100644 << 16)
                    dest.writestr(entry,data,compresslevel=6)
        except BaseException:
            output.unlink(missing_ok=True)
            raise
    return output
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',required=True,type=pathlib.Path)
    p.add_argument('--overlay',required=True,type=pathlib.Path)
    p.add_argument('--output',required=True,type=pathlib.Path)
    a=p.parse_args()
    print(apply(a.base,a.overlay,a.output))
