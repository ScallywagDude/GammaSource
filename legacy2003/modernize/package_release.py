#!/usr/bin/env python3
"""Rebuild the modified-source snapshot and overlay relative to an input archive.

No Steamworks SDK, binary dependency, or missing game asset is fetched.
The user-provided original archive is read only and never overwritten.
"""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT.parent / 'source_2003_leak.zip'
OUT = ROOT.parent


def skip(path):
    return '__pycache__' in path.parts or path.suffix in ('.pyc', '.pyo')


def iter_working_files():
    for f in sorted(ROOT.rglob('*')):
        if f.is_file() and not skip(f.relative_to(ROOT)):
            yield f.relative_to(ROOT).as_posix(), f


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(source, label):
    if not source.exists():
        raise FileNotFoundError(f'Original source ZIP not found: {source}')
    originals = {}
    with zipfile.ZipFile(source) as z:
        original_names = {i.filename for i in z.infolist() if not i.is_dir()}
        changed_sources = {}
        overlay = []
        for rel, path in iter_working_files():
            if rel in ('modernize/source_change_manifest.json','modernize/source_changes.patch',
                       'modernize/release_manifest.json'):
                continue
            data = path.read_bytes()
            old = z.read(rel) if rel in original_names else None
            if old != data:
                overlay.append(rel)
                if old is not None:
                    changed_sources[rel] = {
                        'original_sha256': digest(old),
                        'modified_sha256': digest(data),
                        'modified_bytes': len(data)
                    }
                    originals[rel] = old

    (ROOT/'modernize/source_change_manifest.json').write_text(json.dumps(changed_sources, indent=2)+'\n')
    patch = []
    for rel, old in sorted(originals.items()):
        if not rel.endswith(('.cpp','.h','.c')): continue
        new = (ROOT/rel).read_bytes()
        patch.extend(difflib.unified_diff(
            old.decode('latin-1').splitlines(keepends=True),
            new.decode('latin-1').splitlines(keepends=True),
            fromfile='a/'+rel, tofile='b/'+rel, lineterm='\n'))
    (ROOT/'modernize/source_changes.patch').write_text(''.join(patch), encoding='latin-1')
    overlay += ['modernize/source_change_manifest.json','modernize/source_changes.patch']
    # Add release metadata before selecting all files, so it can be packaged.
    manifest = {
        'original_zip_name': source.name,
        'label': label,
        'changed_original_files': len(changed_sources),
        'changed_original_paths': sorted(changed_sources),
        'overlay_includes_new_and_modified': True,
        'verified_windows_compile': False,
        'verified_source_engine_runtime': False,
    }
    (ROOT/'modernize/release_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    overlay.append('modernize/release_manifest.json')
    overlay_path = OUT / f'HL2_2003_Modernization_Overlay_{label}.zip'
    full_path = OUT / f'HL2_2003_Modernization_Source_{label}.zip'
    for p, files in ((overlay_path, sorted(overlay)),
                     (full_path, [name for name, _ in iter_working_files()])):
        with zipfile.ZipFile(p, 'w', compression=zipfile.ZIP_DEFLATED,
                             compresslevel=6, allowZip64=True) as zip_out:
            for name in files:
                zip_out.write(ROOT/name, arcname=name)
        with zipfile.ZipFile(p) as verify:
            bad = verify.testzip()
            if bad is not None:
                raise RuntimeError(f'CRC failure in {p}: {bad}')
        print(f'{p.name}: {p.stat().st_size:,} bytes, {len(files)} files')
    print(f'Original source file modifications: {len(changed_sources)}')
    for name in sorted(changed_sources): print('  ', name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--original',type=Path,default=DEFAULT_SOURCE)
    parser.add_argument('--label',default='v4')
    args = parser.parse_args()
    run(args.original,args.label)
