#!/usr/bin/env python3
"""Prepare exact-case reference repairs, or reproduce them into a new overlay.

Only unambiguous existing files qualify. Geometry, names, sprites and missing
content are never guessed. The checked-in plan contains hashes and minimal token
edits rather than complete copyrighted game scripts/materials. Inputs stay read
only. Apply mode verifies the original hash before creating each edited file.
"""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import sys

from asset_validation_v18 import AssetIndex, BRANCHES, ROOT, TEXTURE_KEYS, relative_asset

PAIR=re.compile(r'(?P<key>"[^"\r\n]+"|\$?[A-Za-z_][A-Za-z_0-9]*)[ \t]+"(?P<value>[^"\r\n]*)"')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def prepare(branch, roots, only_paths=()):
    index=AssetIndex(roots)
    edits=[]
    diffs=[]
    for root_number, files in enumerate(index.files):
        for names in files.values():
            for rel in names:
                if only_paths and rel not in only_paths:
                    continue
                if not ((rel.casefold().startswith('scripts/weapon_') and rel.endswith('.txt')) or rel.endswith('.vmt')):
                    continue
                path=index.roots[root_number]/rel
                original=path.read_bytes()
                text=original.decode('latin1')
                changes=[]
                # Preserve byte offsets; mask comments instead of removing them.
                masked=re.sub(r'/\*.*?\*/|//[^\r\n]*',lambda m:' '*(m.end()-m.start()),text,flags=re.S)
                for match in PAIR.finditer(masked):
                    key=match['key'].strip('"').casefold()
                    raw=match['value']
                    if key in ('viewmodel','playermodel'):
                        prefix,suffix='models','.mdl'
                    elif key=='include' and rel.endswith('.vmt'):
                        prefix,suffix='materials','.vmt'
                    elif key in TEXTURE_KEYS and rel.endswith('.vmt'):
                        prefix,suffix='materials','.vtf'
                    elif key=='file' and rel.casefold().startswith('scripts/weapon_'):
                        prefix,suffix='materials','.vmt'
                    else:
                        continue
                    record=index.resolve(raw,prefix,suffix)
                    repair_kind='exact_case'
                    if record['status']=='missing' and key=='file' and prefix=='materials' and '/' not in raw.replace('\\','/') and ':' not in raw:
                        # Old weapon sprite blocks use implicit sprites/ leaf names.
                        candidate=index.resolve('sprites/'+raw,prefix,suffix)
                        if candidate['status'] in ('verified_file','case_mismatch'):
                            record=candidate
                            repair_kind='legacy_sprite_prefix_and_case'
                    if record['status']=='invalid_path':
                        normalized=raw.replace('\\','/')
                        parts=normalized.split('/')
                        # Repair redundant internal separators only. Never resolve
                        # traversal, absolute paths or ambiguous directory aliases.
                        if not normalized.startswith('/') and all(p not in ('.','..') for p in parts):
                            collapsed='/'.join(p for p in parts if p)
                            candidate=index.resolve(collapsed,prefix,suffix)
                            if candidate['status'] in ('verified_file','case_mismatch'):
                                record=candidate
                                repair_kind='redundant_separator_and_case'
                    if (record['status']!='case_mismatch' and repair_kind=='exact_case') or record.get('case_collisions'):
                        continue
                    actual=record['actual_path']
                    # Retain the original convention (prefix/extension omitted or present).
                    if not raw.replace('\\','/').casefold().startswith(prefix+'/'):
                        actual=actual[len(prefix)+1:]
                    if not raw.casefold().endswith(suffix):
                        actual=actual[:-len(suffix)]
                    if raw==actual:
                        continue
                    changes.append({'offset':match.start('value'),'before':raw,'after':actual,
                                    'dependency':record['actual_path'],'dependency_root_index':record['root_index'],
                                    'repair_kind':repair_kind})
                if changes:
                    modified=text
                    for edit in reversed(changes):
                        start=edit['offset']
                        modified=modified[:start]+edit['after']+modified[start+len(edit['before']):]
                    edits.append({'branch':branch,'root_index':root_number,'path':rel,
                        'original_sha256':sha(original),'modified_sha256':sha(modified.encode('latin1')),
                        'edits':changes,'status':'repaired_reference_plan_runtime_unverified'})
                    name=f'{branch}/root{root_number}/{rel}'
                    diffs.extend(difflib.unified_diff(text.splitlines(keepends=True),modified.splitlines(keepends=True),
                        fromfile='a/'+name,tofile='b/'+name))
    return edits,diffs


def apply_plan(plan, roots, output):
    output=Path(output).resolve()
    if output.exists():
        raise ValueError('Output must be a new directory; existing content is never overwritten')
    ready=[]
    for entry in plan['files']:
        branch=entry['branch']
        if branch not in BRANCHES:
            raise ValueError('Unknown branch in plan')
        candidates=roots.get(branch,[])
        number=entry['root_index']
        if not isinstance(number,int) or number<0 or number>=len(candidates):
            raise ValueError(f'Missing {branch} root index {number}')
        rel=relative_asset(entry['path'])
        if rel is None:
            raise ValueError('Unsafe plan path')
        source=(Path(candidates[number]).resolve()/rel).resolve()
        source.relative_to(Path(candidates[number]).resolve())
        data=source.read_bytes()
        if sha(data)!=entry['original_sha256']:
            raise ValueError(f'Original content differs: {source}')
        text=data.decode('latin1')
        for edit in sorted(entry['edits'],key=lambda e:e['offset'],reverse=True):
            start=edit['offset']
            if not isinstance(start,int) or start<0 or text[start:start+len(edit['before'])]!=edit['before']:
                raise ValueError('Invalid token offset/content')
            text=text[:start]+edit['after']+text[start+len(edit['before']):]
        fixed=text.encode('latin1')
        if sha(fixed)!=entry['modified_sha256']:
            raise ValueError('Edited hash mismatch')
        target=output/branch/f'root{number}'/rel
        target.resolve().relative_to(output)
        ready.append((target,fixed))
    # Validate the entire plan before writing any output.
    for target,data in ready:
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
    return len(ready)


def verify_output(plan, roots, output):
    output=Path(output).resolve()
    failures=[]
    originals_verified=0
    for entry in plan['files']:
        branch,number=entry['branch'],entry['root_index']
        rel=relative_asset(entry['path'])
        if branch not in BRANCHES or not isinstance(number,int) or number<0 or rel is None:
            raise ValueError('Unsafe plan identity')
        target=(output/branch/f'root{number}'/rel).resolve()
        target.relative_to(output)
        if not target.is_file() or sha(target.read_bytes())!=entry['modified_sha256']:
            failures.append({'path':rel,'check':'edited_output_hash'})
        candidates=roots.get(branch,[])
        if number<len(candidates):
            original=(Path(candidates[number]).resolve()/rel).resolve()
            original.relative_to(Path(candidates[number]).resolve())
            if not original.is_file() or sha(original.read_bytes())!=entry['original_sha256']:
                failures.append({'path':rel,'check':'original_hash'})
            else:
                originals_verified+=1
    return {'files_verified':len(plan['files']),'reference_and_syntax_edits':sum(len(f['edits']) for f in plan['files']),
        'originals_verified_unchanged':originals_verified,'failures':failures,
        'original_assets_modified':False if originals_verified==len(plan['files']) and not failures else 'unverified',
        'original_assets_bundled':False,'local_overlay_path':str(output),'runtime_verified':False}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for b in BRANCHES:
        parser.add_argument('--assets-'+b,type=Path,action='append',default=[])
    parser.add_argument('--plan',type=Path,default=ROOT/'modernize/reports/ASSET_CASE_REPAIR_PLAN_V18.json')
    parser.add_argument('--only-path',action='append',default=[],help='Limit plan generation to these exact relative script/material paths')
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--verify-output',type=Path)
    parser.add_argument('--verification-report',type=Path,default=ROOT/'modernize/reports/ASSET_REPAIRS_VALIDATION_V18.json')
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args(argv)
    roots={b:getattr(args,'assets_'+b) for b in BRANCHES}
    for branch,values in roots.items():
        for root in values:
            if not root.is_dir():
                parser.error(f'Root does not exist: {root}')
    if args.verify_output:
        if args.apply:
            parser.error('--apply and --verify-output are separate operations')
        result=verify_output(json.loads(args.plan.read_text(encoding='utf-8')),roots,args.verify_output)
        args.verification_report.parent.mkdir(parents=True,exist_ok=True)
        args.verification_report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result,indent=2))
        return 1 if result['failures'] else 0
    if args.apply:
        if args.output_dir is None:
            parser.error('--apply requires a new --output-dir')
        plan=json.loads(args.plan.read_text(encoding='utf-8'))
        print('Created edited overlay files:',apply_plan(plan,roots,args.output_dir))
        return 0
    plan={'schema':1,'note':'Minimal exact-case token repairs. No proprietary original asset payloads. Input hashes required. Apply only against the same root ordering.',
          'roots':{b:[str(p.resolve()) for p in v] for b,v in roots.items()},'files':[]}
    patches=[]
    for branch,values in roots.items():
        changes,diff=prepare(branch,values,set(args.only_path))
        plan['files']+=changes
        patches+=diff
    args.plan.parent.mkdir(parents=True,exist_ok=True)
    args.plan.write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    args.plan.with_suffix('.patch').write_bytes(''.join(patches).encode('latin1'))
    print(f"Prepared {len(plan['files'])} file repairs, {sum(len(f['edits']) for f in plan['files'])} reference edits; originals remain read only.")
    return 0


if __name__=='__main__':
    sys.exit(main())
