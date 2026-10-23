#!/usr/bin/env python3
"""Deterministic archives with byte-exact original-plus-overlay verification.

Retains every original file, fetches nothing, never modifies the input archive.
"""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

ROOT=Path(__file__).resolve().parents[1]
META=('modernize/source_change_manifest.json','modernize/source_changes.patch',
      'modernize/release_manifest.json','modernize/reports/CHANGELOG_V18.md')
STAMP=(2003,9,30,0,0,0)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(value):
    p=PurePosixPath(value)
    return bool(value and not value.startswith('/') and '\\' not in value and ':' not in value
                and all(x not in ('','.','..') for x in value.split('/')) and not p.is_absolute())


def iter_working_files(root=ROOT):
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'Symlink cannot be packaged: {path}')
        if path.is_file() and '__pycache__' not in path.parts and path.suffix not in ('.pyc','.pyo'):
            yield path.relative_to(root).as_posix(),path


def write_zip(target, files):
    with zipfile.ZipFile(target,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
        for name,data in sorted(files.items()):
            if not safe_name(name):
                raise ValueError(f'Unsafe archive path: {name}')
            info=zipfile.ZipInfo(name,STAMP)
            info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=0o100644<<16
            archive.writestr(info,data,compress_type=zipfile.ZIP_DEFLATED,compresslevel=6)
    with zipfile.ZipFile(target) as archive:
        if archive.testzip() is not None:
            raise ValueError('Archive CRC failure')


def run(source,label='v18',root=ROOT,out=None):
    source=Path(source).resolve()
    root=Path(root).resolve()
    out=Path(out).resolve() if out else root.parent
    if out==root or root in out.parents:
        raise ValueError('Output must be outside the source tree to avoid packaging earlier release outputs')
    if not re.fullmatch(r'v[0-9]+',label):
        raise ValueError('Label must be v followed by digits')
    with zipfile.ZipFile(source) as archive:
        names=[i.filename for i in archive.infolist() if not i.is_dir()]
        if len(names)!=len(set(names)) or not all(safe_name(n) for n in names):
            raise ValueError('Unsafe or duplicate original archive members')
        originals={name:archive.read(name) for name in names}
    current={name:path.read_bytes() for name,path in iter_working_files(root)}
    removed=sorted(set(originals)-set(current))
    if removed:
        raise ValueError('Original files were removed: '+', '.join(removed))
    changed={}
    for name,data in current.items():
        if name in META:
            continue
        old=originals.get(name)
        if old!=data:
            changed[name]={'action':'modified' if old is not None else 'added',
                'original_sha256':digest(old) if old is not None else None,
                'modified_sha256':digest(data),'modified_bytes':len(data)}
    manifest={'base_zip':source.name,'base_zip_sha256':digest(source.read_bytes()),
        'label':label,'changes':dict(sorted(changed.items())),
        'generated_metadata':list(META),'removed_paths':[],
        'reproduction':'Read original ZIP and replace/add every overlay member; result matches full archive byte-for-byte per member.'}
    (root/'modernize/source_change_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    patches=[]
    for name,item in sorted(changed.items()):
        if item['action']!='modified' or not name.endswith(('.cpp','.h','.c','.py','.md','.json','.dsp','.vcxproj','.filters','.fgd')):
            continue
        old=originals[name].decode('latin1')
        new=current[name].decode('latin1')
        patches.extend(difflib.unified_diff(old.splitlines(keepends=True),new.splitlines(keepends=True),
            fromfile='a/'+name,tofile='b/'+name))
    (root/'modernize/source_changes.patch').write_bytes(''.join(patches).encode('latin1'))
    log=['# v18 complete file change log','',
        'Generated against the exact attached v17 archive. Every original member remains present. No new proprietary model, texture, SDK binary or sound is bundled.', '',
        '| Path | Action | Modified bytes | SHA-256 |','|---|---|---:|---|']
    for name,item in sorted(changed.items()):
        log.append(f"| `{name}` | {item['action']} | {item['modified_bytes']} | `{item['modified_sha256']}` |")
    log += ['', 'Four generated release metadata files are regenerated after this comparison and included in both archives. Features and test limits are in RELEASE_NOTES_V18.md and VALIDATION_V18.md.', '']
    (root/'modernize/reports').mkdir(parents=True,exist_ok=True)
    (root/'modernize/reports/CHANGELOG_V18.md').write_text('\n'.join(log),encoding='utf-8')
    release={'original_zip_name':source.name,'original_zip_sha256':manifest['base_zip_sha256'],
        'label':label,'changed_original_files':sum(x['action']=='modified' for x in changed.values()),
        'new_files':sum(x['action']=='added' for x in changed.values()),
        'changed_original_paths':sorted(n for n,x in changed.items() if x['action']=='modified'),
        'overlay_includes_new_and_modified':True,'original_member_count':len(originals),
        'removed_original_files':0,'external_game_assets_packaged':False,
        'verified_windows_compile':False,'verified_source_engine_runtime':False,
        'archive_member_timestamp':list(STAMP),'overlay_reproduction_verified':True}
    (root/'modernize/release_manifest.json').write_text(json.dumps(release,indent=2)+'\n',encoding='utf-8')
    current={name:path.read_bytes() for name,path in iter_working_files(root)}
    overlay={name:data for name,data in current.items() if originals.get(name)!=data}
    reproduced=dict(originals)
    reproduced.update(overlay)
    if reproduced!=current:
        raise ValueError('Original plus overlay does not reproduce working tree')
    out.mkdir(parents=True,exist_ok=True)
    full=out/f'HL2_2003_Modernization_Source_{label}.zip'
    delta=out/f'HL2_2003_Modernization_Overlay_{label}.zip'
    if source in (full,delta):
        raise ValueError('Cannot overwrite original input archive')
    write_zip(full,current)
    write_zip(delta,overlay)
    with zipfile.ZipFile(full) as complete,zipfile.ZipFile(delta) as patch:
        target={n:complete.read(n) for n in complete.namelist()}
        applied=dict(originals)
        applied.update({n:patch.read(n) for n in patch.namelist()})
        if target!=applied:
            raise ValueError('Packaged overlay reproduction failure')
    artifacts={'base_sha256':manifest['base_zip_sha256'],'overlay_reproduction_verified':True,
        'files':[{'name':p.name,'bytes':p.stat().st_size,'sha256':digest(p.read_bytes())} for p in (full,delta)]}
    (out/f'HL2_2003_Modernization_Checksums_{label}.json').write_text(json.dumps(artifacts,indent=2)+'\n',encoding='utf-8')
    for entry in artifacts['files']:
        print(f"{entry['name']}: {entry['bytes']:,} bytes; sha256 {entry['sha256']}")
    print(f'Verified original + {len(overlay)} overlay members = {len(current)} full-source members.')
    return artifacts


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original',type=Path,required=True)
    parser.add_argument('--label',default='v18')
    parser.add_argument('--out-dir',type=Path)
    args=parser.parse_args()
    run(args.original,args.label,out=args.out_dir)
