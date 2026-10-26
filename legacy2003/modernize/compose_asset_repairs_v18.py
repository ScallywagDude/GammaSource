#!/usr/bin/env python3
"""Compose hash-checked token plans so syntax/case repairs cannot overwrite each other.

The result is another minimal plan usable by asset_case_patch_v18.py --apply.
Original local materials/scripts stay read only. Conflicting edits are refused.
"""
import argparse
import difflib
import json
from pathlib import Path

from asset_case_patch_v18 import apply_plan, sha
from asset_validation_v18 import BRANCHES, ROOT, relative_asset


def compose(plans, roots):
    combined={}
    for plan in plans:
        for entry in plan['files']:
            branch,number,rel=entry['branch'],entry['root_index'],entry['path']
            if branch not in BRANCHES or relative_asset(rel)!=rel or not isinstance(number,int) or number<0:
                raise ValueError('Unsafe plan identity')
            if number>=len(roots.get(branch,[])):
                raise ValueError('Missing explicitly supplied branch root')
            key=(branch,number,rel)
            if key not in combined:
                combined[key]={'branch':branch,'root_index':number,'path':rel,
                    'original_sha256':entry['original_sha256'],'edits':[]}
            output=combined[key]
            if output['original_sha256']!=entry['original_sha256']:
                raise ValueError('Plans describe different originals: '+rel)
            for edit in entry['edits']:
                if edit not in output['edits']:
                    output['edits'].append(edit)
    diffs=[]
    for (branch,number,rel),entry in sorted(combined.items()):
        base=Path(roots[branch][number]).resolve()
        source=(base/rel).resolve()
        source.relative_to(base)
        raw=source.read_bytes()
        if sha(raw)!=entry['original_sha256']:
            raise ValueError('Original hash mismatch: '+rel)
        text=raw.decode('latin1')
        end=-1
        edits=sorted(entry['edits'],key=lambda e:e['offset'])
        for edit in edits:
            start=edit['offset']
            if not isinstance(start,int) or start<0 or start<end or text[start:start+len(edit['before'])]!=edit['before']:
                raise ValueError('Overlapping or invalid edit: '+rel)
            end=start+max(1,len(edit['before']))
        fixed=text
        for edit in reversed(edits):
            start=edit['offset']
            fixed=fixed[:start]+edit['after']+fixed[start+len(edit['before']):]
        entry['edits']=edits
        entry['modified_sha256']=sha(fixed.encode('latin1'))
        entry['status']='repaired_references_and_configuration_runtime_unverified'
        name=f'{branch}/root{number}/{rel}'
        diffs.extend(difflib.unified_diff(text.splitlines(keepends=True),fixed.splitlines(keepends=True),
            fromfile='a/'+name,tofile='b/'+name,n=2))
    return {'schema':1,'note':'Combined minimal token repairs; no original asset payloads bundled.',
        'roots':{b:[str(Path(p).resolve()) for p in v] for b,v in roots.items()},
        'files':[entry for key,entry in sorted(combined.items())],
        'runtime_verified':False,'original_assets_modified':False},''.join(diffs)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-plan',type=Path,action='append',required=True)
    for b in BRANCHES:
        parser.add_argument('--assets-'+b,type=Path,action='append',default=[])
    parser.add_argument('--plan',type=Path,default=ROOT/'modernize/reports/ASSET_RESTORATION_PLAN_V18.json')
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args(argv)
    if args.output_dir:
        destination=args.output_dir.resolve()
        if destination==ROOT or ROOT in destination.parents:
            parser.error('Edited local asset output must be outside the source package')
    roots={b:getattr(args,'assets_'+b) for b in BRANCHES}
    plans=[json.loads(p.read_text(encoding='utf-8')) for p in args.input_plan]
    combined,diff=compose(plans,roots)
    args.plan.parent.mkdir(parents=True,exist_ok=True)
    args.plan.write_text(json.dumps(combined,indent=2)+'\n',encoding='utf-8')
    args.plan.with_suffix('.patch').write_bytes(diff.encode('latin1'))
    if args.output_dir:
        print('Created composed local asset files:',apply_plan(combined,roots,args.output_dir))
    print(f"Combined {len(combined['files'])} files and {sum(len(f['edits']) for f in combined['files'])} edits")
    return 0


if __name__=='__main__':
    raise SystemExit(main())
