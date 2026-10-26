#!/usr/bin/env python3
"""Inspect unpacked game assets for cut-NPC model references.

This audits only raw files under developer-supplied directories, not Source
VPK/GCF archives, animation correctness, precache scripts or NPC AI. A model
being present does not mean the NPC can spawn or work in game.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path, PurePosixPath
from source_asset_formats import inspect_mdl

ROOT=Path(__file__).resolve().parents[1]
INVENTORY=ROOT/'modernize'/'cut_npc_inventory.json'


def normalize_reference(value):
    if not isinstance(value,str) or not value.strip(): return None
    value=value.replace('\\','/')
    if value.startswith('/') or ':' in value: return None
    components=PurePosixPath(value).parts
    if not components or '..' in components or components[0].casefold()!='models': return None
    if not value.casefold().endswith('.mdl'): return None
    return '/'.join(components).casefold()


def unpacked_model_files(root):
    """Return normalized paths within game directory or a `models` directory."""
    found={}
    root=Path(root)
    if not root.is_dir(): return found
    for directory,dirs,files in os.walk(root,followlinks=False):
        dirs[:]=[d for d in dirs if not (Path(directory)/d).is_symlink()]
        prefix=Path(directory).relative_to(root)
        for filename in files:
            if Path(filename).suffix.casefold()!='.mdl': continue
            if (Path(directory)/filename).is_symlink(): continue
            rel=(prefix/filename).as_posix()
            if root.name.casefold()=='models': rel='models/'+rel
            clean=normalize_reference(rel)
            if clean: found.setdefault(clean, []).append((rel, Path(directory)/filename))
    return found


def unpacked_model_paths(root):
    return set(unpacked_model_files(root))


def audit(entries, asset_roots):
    roots=[str(Path(x)) for x in asset_roots]
    present={}
    for root in roots:
        for key, values in unpacked_model_files(root).items():
            present.setdefault(key, []).extend(values)
    result=[]
    for entry in entries:
        ref=entry.get('model_reference')
        normalized=normalize_reference(ref)
        if normalized is None:
            status='unknown_reference' if ref is None else 'invalid_reference'
        elif not roots:
            status='not_checked'
        elif normalized in present:
            status='found_unpacked'
        else:
            status='not_found_unpacked'
        matches=present.get(normalized, [])
        content=inspect_mdl(matches[0][1]) if matches else None
        attachment_checks=[]
        sequence_checks=[]
        if content and content['status']=='metadata_verified':
            attachment_checks=[{'name':name,'status':'verified' if name in content['attachments'] else 'missing'} for name in entry.get('named_attachments', [])]
            sequence_checks=[{'name':name,'status':'verified' if name in content['sequence_names'] else 'missing'} for name in entry.get('named_sequences', [])]
        reference_models=entry.get('model_references', [ref] if ref else [])
        dependencies=[]
        for model in reference_models:
            key=normalize_reference(model)
            candidates=present.get(key, [])
            dependencies.append({'reference':model,'status':'not_checked' if not roots else 'found_unpacked' if candidates else 'not_found_unpacked',
                                 'case_sensitive_match':any(actual==model.replace('\\','/') for actual,_ in candidates),
                                 'located_paths':[str(path) for _,path in candidates]})
        result.append({
            'classname':entry['name'],
            'factory_registration_in_source':bool(entry.get('registration_active')),
            'listed_in_debug_build':bool(entry.get('included_in_debug_hl2')),
            'expected_model':ref,
            'model_file_status':status,
            'model_case_status':'exact' if matches and any(actual==ref.replace('\\','/') for actual,_ in matches) else 'case_mismatch' if matches else 'not_checked',
            'model_content':content,
            'model_dependencies':dependencies,
            'named_attachment_checks':attachment_checks,
            'named_sequence_checks':sequence_checks,
            'dependency_verification_status':'metadata_only' if content and content['status']=='metadata_verified' else 'unverified',
            'required_named_attachments':entry.get('named_attachments', []),
            'required_sequences':entry.get('named_sequences', []),
            'required_activities':entry.get('activity_references', []),
            'required_effect_materials':entry.get('effect_material_references', []),
            'required_sound_files':entry.get('sound_file_references', []),
            'required_sound_events':entry.get('sound_event_references', []),
            'included_build_configurations':entry.get('included_build_configurations', []),
            'npc_runtime_verified':False,
        })
    return {
        'asset_roots':roots,
        'scanned_unpacked_mdl_count':len(present),
        'warning':'File existence and bounded studio35/36 metadata parsing do not verify geometry, UVs, animation correctness, materials, sounds, AI, precaching, archived contents, or runtime spawning. Asset roots are external installations and are not bundled in the source release.',
        'npcs':result
    }


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-root',type=Path,action='append',default=[],help='Directory containing unpacked models/ (repeatable); archived game files are not inspected')
    parser.add_argument('--inventory',type=Path,default=INVENTORY)
    parser.add_argument('--out',type=Path,help='Optional JSON output destination')
    args=parser.parse_args()
    entries=json.loads(args.inventory.read_text(encoding='utf-8'))
    result=audit(entries,args.assets_root)
    output=json.dumps(result,indent=2)+'\n'
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(output,encoding='utf-8')
        print(f'Wrote {args.out}')
    else:
        print(output,end='')

if __name__=='__main__': main()
