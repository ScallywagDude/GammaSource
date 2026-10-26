#!/usr/bin/env python3
"""Validate branch-specific weapon scripts, model dependencies and HUD atlases.

Checks assets in caller-provided, *unpacked* game directories only. No asset
path provided means UNVERIFIED, not missing. No resources are downloaded or
fabricated. Steam VPK/GCF archives must be extracted using authorized tools.
"""
from __future__ import annotations
import argparse
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRANCHES = ('hl2', 'hl1', 'cstrike', 'tf2')
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+', re.S)
MODEL_SUFFIXES = ('.mdl', '.vvd', '.dx90.vtx')

def tokens(text):
    for m in TOKEN.finditer(text):
        s=m.group()
        if s.startswith('//') or s.startswith('/*'):
            continue
        if len(s) > 1 and s[0] == '"' and s[-1] == '"':
            s=s[1:-1].replace('\\"', '"').replace('\\\\', '\\')
        yield s

def kv_parse(text):
    seq=list(tokens(text)); pos=0
    def group(end=False):
        nonlocal pos
        result={}
        while pos < len(seq):
            key=seq[pos]; pos+=1
            if key=='}':
                if not end: raise ValueError('unexpected }')
                return result
            if key=='{': raise ValueError('unexpected {')
            if pos>=len(seq): raise ValueError('missing value for '+key)
            value=seq[pos]; pos+=1
            if value=='{': value=group(True)
            elif value=='}': raise ValueError('missing value for '+key)
            result[key.casefold()]=value
        if end: raise ValueError('unclosed {')
        return result
    result=group()
    # Source weapon scripts have a single optional root key such as WeaponData.
    if len(result)==1:
        node=next(iter(result.values()))
        if isinstance(node,dict):return node
    return result

def clean_asset_path(raw, prefix, suffix):
    """Only relative slash paths under the expected directory are allowed."""
    if not isinstance(raw,str) or not raw.strip():return None
    raw=raw.strip().replace('\\','/')
    if raw.startswith('/') or ':' in raw or '?' in raw or '#' in raw:return None
    parts=raw.split('/')
    if any(p in ('', '.', '..') for p in parts):return None
    if raw.lower().startswith(prefix+'/'):raw=raw[len(prefix)+1:]
    if raw.lower().endswith(suffix):raw=raw[:-len(suffix)]
    return prefix+'/'+raw+suffix

def dependency(root, relative):
    if relative is None:return {'path':None,'status':'invalid_path'}
    return {'path':relative,'status':'present' if (root/relative).is_file() else 'missing'}

def model_assets(root, raw):
    if not raw:return {'status':'unconfigured','paths':[]}
    path=clean_asset_path(raw,'models','.mdl')
    if path is None:return {'status':'invalid_path','paths':[]}
    stem=path[:-4]
    deps=[dependency(root,path)]
    # A model may be a legacy one-file MDL or a modern split-file model.
    # Companion files are reported but are not universally mandatory.
    for ext in ('.vvd','.dx90.vtx','.dx80.vtx'):
        deps.append(dependency(root,stem+ext))
    return {'status':deps[0]['status'],'paths':deps}

def vtf_size(data):
    if len(data)<20 or data[:4]!=b'VTF\0':return None
    return struct.unpack_from('<HH',data,16)

def check_sprite(root, name, spec):
    record={'name':name}
    if not isinstance(spec,dict):return {'name':name,'status':'invalid_sprite_data'}
    material=clean_asset_path(spec.get('file',''),'materials','.vmt')
    if not material:return {'name':name,'status':'invalid_material_name'}
    try:
        x,y,w,h=(int(spec.get(k,0)) for k in ('x','y','width','height'))
    except (ValueError,TypeError):return {'name':name,'status':'invalid_rectangle','material':material}
    record.update({'material':material,'rectangle':[x,y,w,h]})
    if min(x,y)<0 or min(w,h)<1 or max(x+w,y+h)>8192:
        record['status']='invalid_rectangle';return record
    vmt=root/material
    if not vmt.is_file():record['status']='missing_material';return record
    try:
        doc=kv_parse(vmt.read_text(encoding='latin1'))
    except ValueError:
        record['status']='invalid_vmt';return record
    base=doc.get('$basetexture') or doc.get('$baseTexture'.lower())
    if not isinstance(base,str):record['status']='missing_basetexture';return record
    vtf=clean_asset_path(base,'materials','.vtf')
    if not vtf:record['status']='invalid_basetexture';return record
    record['texture']=vtf
    file=root/vtf
    if not file.is_file():record['status']='missing_texture';return record
    with file.open('rb') as stream:data=stream.read(20)
    dims=vtf_size(data)
    if dims is None:
        record['status']='invalid_vtf_header';return record
    record['texture_dimensions']=list(dims)
    record['status']='present' if x+w<=dims[0] and y+h<=dims[1] else 'outside_texture_bounds'
    return record

def audit(assets, inventory=None):
    if inventory is None:
        inventory=json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text())['branches']
    result={'note':'Asset presence checks, not proof of working animations, textures, or in-game render scale.', 'branches':{}}
    for branch in BRANCHES:
        root=assets.get(branch)
        records=[]
        for entry in inventory.get(branch,[]):
            cls=entry['classname']
            item={'classname':cls,'implementation_file':entry['implementation_file'],'runtime_verified':False}
            if root is None:
                item['status']='unverified_no_asset_directory'
                records.append(item);continue
            script_rel=f'scripts/{cls}.txt';script=root/script_rel
            item['weapon_script']=dependency(root,script_rel)
            if not script.is_file():
                item['status']='missing_weapon_script';records.append(item);continue
            try: data=kv_parse(script.read_text(encoding='latin1'))
            except ValueError as exc:
                item['status']='invalid_weapon_script';item['error']=str(exc)
                records.append(item);continue
            item['viewmodel']=model_assets(root,data.get('viewmodel',''))
            item['worldmodel']=model_assets(root,data.get('playermodel',''))
            sprites=data.get('texturedata',{})
            if not isinstance(sprites,dict):sprites={}
            item['hud_icons']=[check_sprite(root,name,config) for name,config in sorted(sprites.items())]
            # Legacy UI requires a normal selected and idle icon, or falls
            # back to shared "selection" sprites. Don't invent artwork.
            names=set(sprites)
            item['selection_icons']={'active': 'weapon' in names or 'weapon_s' in names,
                                      'inactive':'weapon_s' in names or 'weapon' in names}
            item['issues']=[]
            for model in ('viewmodel','worldmodel'):
                if item[model]['status']!='present':item['issues'].append(model+':'+item[model]['status'])
            for sprite in item['hud_icons']:
                if sprite['status']!='present':item['issues'].append('icon:'+sprite['name']+':'+sprite['status'])
            if not item['hud_icons']:item['issues'].append('no_weapon_hud_sprites')
            item['status']='assets_present_unverified_runtime' if not item['issues'] else 'asset_issues'
            records.append(item)
        result['branches'][branch]={'asset_root':str(root) if root else None,'weapons':records,
            'summary':{'total':len(records),'unverified':sum(x['status']=='unverified_no_asset_directory' for x in records),
                       'issues':sum(x['status'] in ('asset_issues','missing_weapon_script','invalid_weapon_script') for x in records)}}
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for branch in BRANCHES:
        parser.add_argument('--assets-'+branch,type=Path,help=f'unpacked {branch} game content directory')
    parser.add_argument('--output',type=Path,default=ROOT/'modernize/weapon_asset_report.json')
    args=parser.parse_args()
    roots={b:getattr(args,'assets_'+b) for b in BRANCHES}
    for name,path in roots.items():
        if path is not None and not path.is_dir():parser.error(f'{name} asset root does not exist: {path}')
    report=audit(roots)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('Weapon asset audit:',{k:v['summary'] for k,v in report['branches'].items()})
if __name__=='__main__': main()
