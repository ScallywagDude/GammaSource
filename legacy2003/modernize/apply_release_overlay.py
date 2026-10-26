#!/usr/bin/env python3
"""Reproduce release from exact base ZIP and overlay into a new directory.

Safe paths/hash checks; no patch generators or network dependencies are run.
This is the canonical v17-to-v18 reproduction route.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from package_release_v18 import safe_name


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_zip(path):
    with zipfile.ZipFile(path) as archive:
        names=[i.filename for i in archive.infolist() if not i.is_dir()]
        if len(names)!=len(set(names)) or not all(safe_name(n) for n in names):
            raise ValueError('Unsafe/duplicate archive members')
        if archive.testzip() is not None:
            raise ValueError('Archive CRC failure')
        return {n:archive.read(n) for n in names}


def apply(base,overlay,destination):
    destination=Path(destination).resolve()
    if destination.exists():
        raise ValueError('Destination must be a new directory')
    original=read_zip(base)
    patch=read_zip(overlay)
    manifest=json.loads(patch['modernize/source_change_manifest.json'].decode('utf-8'))
    if sha(Path(base).read_bytes())!=manifest['base_zip_sha256']:
        raise ValueError('Base ZIP SHA-256 does not match this release')
    for name,item in manifest['changes'].items():
        if not safe_name(name) or name not in patch:
            raise ValueError('Manifest member missing/unsafe')
        if sha(patch[name])!=item['modified_sha256']:
            raise ValueError(f'Modified SHA-256 mismatch: {name}')
        before=original.get(name)
        if item['original_sha256'] is not None and (before is None or sha(before)!=item['original_sha256']):
            raise ValueError(f'Original SHA-256 mismatch: {name}')
    permitted=set(manifest['changes'])|set(manifest['generated_metadata'])
    if set(manifest['generated_metadata'])-set(patch):
        raise ValueError('Generated release metadata missing from overlay')
    if set(patch)-permitted:
        raise ValueError('Unexpected overlay members outside manifest')
    original.update(patch)
    for name,data in sorted(original.items()):
        target=destination/name
        target.resolve().relative_to(destination)
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
    return len(original)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original',type=Path,required=True)
    parser.add_argument('--overlay',type=Path,required=True)
    parser.add_argument('--destination',type=Path,required=True)
    args=parser.parse_args()
    print('Reproduced source members:',apply(args.original,args.overlay,args.destination))
