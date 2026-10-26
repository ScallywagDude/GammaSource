#!/usr/bin/env python3
"""Apply the v22 modernization overlay to the exact v21 source ZIP.

No original file is overwritten. Checks base archive SHA256 and each overlay member.
"""
import argparse
import hashlib
import json
import pathlib
import zipfile

MANIFEST='modernize/release_manifest_v22.json'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def safe(name):
    return bool(name) and not name.startswith('/') and '\\' not in name and all(
        part not in ('', '.', '..') for part in name.split('/'))

def apply(base, overlay, output):
    if output.exists():
        raise ValueError('refusing to overwrite '+str(output))
    with zipfile.ZipFile(base) as src, zipfile.ZipFile(overlay) as patch:
        names=src.namelist()
        overlay_names=patch.namelist()
        if len(names)!=len(set(names)) or len(overlay_names)!=len(set(overlay_names)):
            raise ValueError('duplicate names in release archives')
        manifest=json.loads(patch.read(MANIFEST))
        if manifest.get('base_sha256')!=sha(base.read_bytes()):
            raise ValueError('base archive SHA256 does not match v21')
        changed=manifest.get('changes',{})
        if set(overlay_names)!=(set(changed)|{MANIFEST}) or any(not safe(x) for x in changed):
            raise ValueError('invalid overlay manifest paths')
        payload={name:patch.read(name) for name in changed}
        if any(sha(blob)!=changed[name] for name,blob in payload.items()):
            raise ValueError('overlay file SHA256 mismatch')
        try:
            with zipfile.ZipFile(output,'w',allowZip64=True) as dest:
                for member in src.infolist():
                    dest.writestr(member,payload.pop(member.filename,src.read(member.filename)))
                for name,blob in sorted(payload.items()):
                    info=zipfile.ZipInfo(name,date_time=(2003,9,30,0,0,0))
                    info.compress_type=zipfile.ZIP_DEFLATED
                    info.external_attr=0o100644<<16
                    dest.writestr(info,blob,compresslevel=6)
        except BaseException:
            output.unlink(missing_ok=True)
            raise
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',required=True,type=pathlib.Path)
    p.add_argument('--overlay',required=True,type=pathlib.Path)
    p.add_argument('--output',required=True,type=pathlib.Path)
    args=p.parse_args()
    print(apply(args.base,args.overlay,args.output))
