#!/usr/bin/env python3
"""Plan three conservative VMT syntax repairs; never edit installed originals.

Only a single missing final brace, extra terminal brace or bare terminal token
exactly matching the filename stem can be repaired. The resulting file must
parse as one shader block. Shader names, parameters, proxy values and artwork
are never inferred. Apply writes a new, separate hash-checked local overlay.
"""
from __future__ import annotations
import argparse
import collections
import difflib
import hashlib
import json
from pathlib import Path

from weapon_asset_audit import TOKEN
from asset_validation_v18 import AssetIndex, BRANCHES, ROOT, kv_pairs
from asset_case_patch_v18 import apply_plan


def sha(data):
    return hashlib.sha256(data).hexdigest()


def lexical_tokens(text):
    """Source TOKEN scanner with spans, rejecting unrecognized quote gaps."""
    result=[]
    previous=0
    for match in TOKEN.finditer(text):
        if text[previous:match.start()].strip():
            raise ValueError('unrecognized/unclosed quoted token')
        previous=match.end()
        raw=match[0]
        if raw.startswith('//') or (raw.startswith('/*') and raw.endswith('*/')):
            continue
        if raw.startswith('/*'):
            raise ValueError('unclosed block comment')
        result.append({'raw':raw,'offset':match.start(),'end':match.end()})
    if text[previous:].strip():
        raise ValueError('unrecognized/unclosed trailing token')
    return result


def valid_material(text):
    try:
        lexical_tokens(text)
        parsed=kv_pairs(text)
        return len(parsed)==1 and bool(parsed[0][0]) and isinstance(parsed[0][1],list)
    except ValueError:
        return False


def propose(text, filename):
    """Return one minimal edit or an explicit refusal; no multi-edit guesses."""
    try:
        scanned=lexical_tokens(text)
    except ValueError as exc:
        return None,str(exc)
    if valid_material(text):
        return None,'already_valid'
    if not scanned:
        return None,'empty_material'
    proposals=[]
    depth=0
    minimum=0
    for token in scanned:
        if token['raw']=='{':depth+=1
        elif token['raw']=='}':depth-=1
        minimum=min(minimum,depth)
    newline='\r\n' if '\r\n' in text else '\n'
    if depth==1 and minimum>=0:
        ending=('' if text.endswith(('\r','\n')) else newline)+'}'+newline
        if valid_material(text+ending):
            proposals.append({'offset':len(text),'before':'','after':ending,
                              'reason':'append_single_missing_final_brace'})
    last=scanned[-1]
    if last['raw']=='}':
        modified=text[:last['offset']]+text[last['end']:]
        if valid_material(modified):
            proposals.append({'offset':last['offset'],'before':'}','after':'',
                              'reason':'remove_single_extra_terminal_brace'})
    if not last['raw'].startswith('"') and last['raw']==Path(filename).stem:
        modified=text[:last['offset']]+text[last['end']:]
        if valid_material(modified):
            proposals.append({'offset':last['offset'],'before':last['raw'],'after':'',
                              'reason':'remove_bare_terminal_filename_stem'})
    if len(proposals)!=1:
        return None,'ambiguous_or_unsupported_syntax'
    return proposals[0],None


def prepare(roots):
    plan={'schema':1,'scope':'Three single-edit syntactic repairs only. Parsed KeyValues syntax does not verify shaders, parameter meaning, textures, alpha, UVs, or runtime behavior.',
          'roots':{b:[str(Path(p).resolve()) for p in paths] for b,paths in roots.items()},
          'files':[],'unrepaired':[]}
    diffs=[]
    counts=collections.Counter()
    for branch,paths in roots.items():
        index=AssetIndex(paths)
        for number,files in enumerate(index.files):
            for names in files.values():
                for rel in names:
                    if not rel.casefold().endswith('.vmt'):continue
                    counts['scanned']+=1
                    raw=(index.roots[number]/rel).read_bytes()
                    text=raw.decode('latin1')
                    edit,refusal=propose(text,rel)
                    if refusal=='already_valid':
                        counts['already_valid']+=1
                        continue
                    counts['malformed']+=1
                    if edit is None:
                        counts['unrepaired']+=1
                        plan['unrepaired'].append({'branch':branch,'root_index':number,'path':rel,
                                                  'reason':refusal,'runtime_verified':False})
                        continue
                    modified=text[:edit['offset']]+edit['after']+text[edit['offset']+len(edit['before']):]
                    counts['repaired']+=1
                    counts[edit['reason']]+=1
                    plan['files'].append({'branch':branch,'root_index':number,'path':rel,
                        'original_sha256':sha(raw),'modified_sha256':sha(modified.encode('latin1')),
                        'edits':[edit],'status':'repaired_syntax_runtime_unverified'})
                    name=f'{branch}/root{number}/{rel}'
                    diffs.extend(difflib.unified_diff(text.splitlines(keepends=True),modified.splitlines(keepends=True),
                        fromfile='a/'+name,tofile='b/'+name,n=2))
    plan['summary']=dict(counts)
    plan['runtime_verified']=False
    plan['original_assets_modified']=False
    plan['proprietary_asset_payloads_bundled']=False
    return plan,''.join(diffs)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for branch in BRANCHES:
        parser.add_argument('--assets-'+branch,type=Path,action='append',default=[])
    parser.add_argument('--plan',type=Path,default=ROOT/'modernize/reports/MATERIAL_SYNTAX_REPAIRS_V18.json')
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args(argv)
    roots={branch:getattr(args,'assets_'+branch) for branch in BRANCHES}
    for paths in roots.values():
        for path in paths:
            if not path.is_dir():parser.error(f'Asset root unavailable: {path}')
    if args.apply and args.output_dir is None:
        parser.error('--apply requires a new --output-dir outside the source release')
    if args.output_dir:
        output=args.output_dir.resolve()
        if output==ROOT or ROOT in output.parents:
            parser.error('Local edited assets must remain outside the source release')
    plan,diff=prepare(roots)
    args.plan.parent.mkdir(parents=True,exist_ok=True)
    args.plan.write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    args.plan.with_suffix('.patch').write_bytes(diff.encode('latin1'))
    print(json.dumps(plan['summary'],sort_keys=True))
    if args.apply:
        print('Created local syntax-repaired files:',apply_plan(plan,roots,args.output_dir))
    return 0


if __name__=='__main__':raise SystemExit(main())
