#!/usr/bin/env python3
"""Phase 7: read-only layered game-asset diagnostics (not runtime proof).

Supports ordered overlay directories per branch. Uses the existing weapon
KeyValues reader and conservative 2003 VTX header classifier; never merges
HL2/HL1/Counter-Strike/TF2 or modifies commercial assets.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
import weapon_asset_audit as legacy
from scan_studio_v21 import inspect_mdl

BRANCHES=legacy.BRANCHES

def roots_of(value):
    values=[] if value is None else ([value] if isinstance(value,(str,Path)) else value)
    roots=[Path(p).resolve() for p in values]
    for p in roots:
        if not p.is_dir():raise ValueError('asset directory not found: '+str(p))
    return roots

def locate(roots,relative):
    if not isinstance(relative,str) or not relative or relative.startswith('/') or '\\' in relative or ':' in relative or any(p in ('','.','..') for p in relative.split('/')):
        return None
    for i,root in enumerate(roots):
        path=(root/relative).resolve()
        if path.is_relative_to(root) and path.is_file():return (i,path)
    return None

def select_sprite(spec):
    if not isinstance(spec,dict):return None,'invalid'
    if isinstance(spec.get('file'),str) and spec['file'].strip():return spec,'direct'
    for size in ('640','320'):
        x=spec.get(size)
        if isinstance(x,dict) and isinstance(x.get('file'),str) and x['file'].strip():return x,size
    return None,'font_or_missing'

def check_model(roots,model):
    path=legacy.clean_asset_path(model,'models','.mdl')
    if path is None or path=='models/.mdl':return {'status':'invalid_model_path'}
    match=locate(roots,path)
    if not match:return {'status':'missing_model','path':path}
    layer,filename=match
    # Each split model companion resolves independently through the search
    # path. An override layer may provide only an MDL or only a VTX. The
    # old same-directory inspection could incorrectly mark those models as
    # missing geometry. Keep the variant and exact winning layer visible.
    stem=path[:-4]
    def vertex_lookup(ext):
        return locate(roots,stem+ext)
    result=inspect_mdl(filename,vertex_lookup=vertex_lookup)
    result.update({'path':path,'layer':layer,'runtime_verified':False})
    return result

class MaterialError(ValueError):
    def __init__(self, code):
        super().__init__(code)
        self.code=code


def resolve_material(roots, material, *, max_depth=12):
    """Resolve base texture through Source VMT Patch includes (read-only).

    Every include is searched independently through the given branch's
    ordered asset layers, so an override can depend on a lower layer.
    Patch replace changes existing values; insert only adds absent keys.
    This is not a general material renderer and does not validate shaders.
    """
    seen=set()
    chain=[]

    def resolve(relative, depth):
        if depth>=max_depth or relative in seen:
            raise MaterialError('cyclic_or_deep_material')
        seen.add(relative)
        match=locate(roots,relative)
        if not match:
            raise MaterialError('missing_material')
        layer,filename=match
        chain.append({'path':relative,'layer':layer})
        try:
            contents=filename.read_text(encoding='latin1')
            # Legacy KeyValues helper intentionally unwraps a lone root node.
            # Retain the root token here to distinguish Patch from a shader.
            tok=legacy.tokens(contents)
            root=next(tok, '').casefold()
            data=legacy.kv_parse(contents)
        except (OSError,ValueError):
            raise MaterialError('malformed_material')
        if not isinstance(data,dict):
            raise MaterialError('malformed_material')
        if root!='patch':
            return dict(data)
        include=data.get('include')
        incpath=legacy.clean_asset_path(include,'materials','.vmt')
        if not incpath or incpath=='materials/.vmt':
            raise MaterialError('invalid_material_include')
        base=resolve(incpath,depth+1)
        # Source Patch materials inherit the referenced shader's parameters.
        # 'replace' applies only to existing names; 'insert' only to absent ones.
        # Value sections are required to be dictionaries, not arbitrary tokens.
        for section in ('replace','insert'):
            override=data.get(section,{})
            if not isinstance(override,dict):
                raise MaterialError('malformed_material')
            for key,value in override.items():
                if section=='replace' and key in base:
                    base[key]=value
                elif section=='insert' and key not in base:
                    base[key]=value
        return base

    result=resolve(material,0)
    return result,chain


def check_icon(roots,name,spec):
    source,layout=select_sprite(spec)
    item={'role':name,'layout':layout,'runtime_verified':False}
    if source is None:item['status']='missing_raster';return item
    material=source.get('file','')
    # Match HL2ModernHud::SpriteMaterialName: only bare names inside legacy
    # resolution blocks gain sprites/. Explicit paths and direct names keep
    # their spelling; the shared path validator still rejects traversal.
    if layout!='direct' and isinstance(material,str) and not any(c in material for c in '/\\:'):
        material='sprites/'+material
    path=legacy.clean_asset_path(material,'materials','.vmt')
    if path is None:item['status']='invalid_material_path';return item
    item['material']=path
    try:x,y,w,h=[int(source.get(k,0)) for k in ('x','y','width','height')]
    except (ValueError,TypeError):item['status']='invalid_rectangle';return item
    item['rectangle']=[x,y,w,h]
    if x<0 or y<0 or w<=0 or h<=0 or x+w>8192 or y+h>8192:
        item['status']='invalid_rectangle';return item
    try:
        doc,chain=resolve_material(roots,path)
    except MaterialError as error:
        item['status']=error.code;return item
    item['material_layer']=chain[0]['layer']
    item['material_chain']=chain
    texture=legacy.clean_asset_path(doc.get('$basetexture',''),'materials','.vtf')
    item['texture']=texture
    if not texture:item['status']='invalid_texture_path';return item
    vtf=locate(roots,texture)
    if not vtf:item['status']='missing_texture';return item
    item['texture_layer']=vtf[0]
    try:
        with vtf[1].open('rb') as reader:size=legacy.vtf_size(reader.read(20))
    except OSError:size=None
    item['dimensions']=list(size) if size else None
    if size is None or min(size)<1:item['status']='invalid_vtf'
    elif x+w>size[0] or y+h>size[1]:item['status']='out_of_bounds'
    else:item['status']='rectangle_verified'
    return item

def audit(root_map,registry):
    result={'schema_version':23,'note':'File presence and format are not texture identity, correct scale, or working gameplay.','branches':{}}
    for branch in BRANCHES:
        roots=roots_of(root_map.get(branch))
        output=[]
        for weapon in registry.get(branch,[]):
            classname=weapon['classname']
            row={'classname':classname,'runtime_verified':False,'status':'unverified_no_assets'}
            if not roots:output.append(row);continue
            name='scripts/'+classname+'.txt'
            script=locate(roots,name)
            if not script:
                row['status']='missing_script';output.append(row);continue
            row['script_layer']=script[0]
            try:kv=legacy.kv_parse(script[1].read_text(encoding='latin1'))
            except (ValueError,OSError):
                row['status']='invalid_script';output.append(row);continue
            row['viewmodel']=check_model(roots,kv.get('viewmodel'))
            row['worldmodel']=check_model(roots,kv.get('playermodel'))
            textures=kv.get('texturedata',{})
            row['hud']={n:check_icon(roots,n,s) for n,s in textures.items()} if isinstance(textures,dict) else {}
            blocked=[n+':'+row[n]['status'] for n in ('viewmodel','worldmodel') if row[n]['status']!='2003_loader_candidate']
            blocked.extend('hud:'+name+':'+icon['status'] for name,icon in row['hud'].items() if icon['status']!='rectangle_verified')
            if not row['hud']:blocked.append('missing_hud_icons')
            row['blocking_dependencies']=blocked
            row['status']='metadata_candidate' if not blocked else 'dependencies_blocked'
            output.append(row)
        result['branches'][branch]={'roots':[str(r) for r in roots],'weapons':output,
            'total':len(output),'metadata_candidates':sum(x['status']=='metadata_candidate' for x in output)}
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for branch in BRANCHES:parser.add_argument('--assets-'+branch,action='append',default=[])
    parser.add_argument('--output',type=Path,default=Path(__file__).parent/'reports/WEAPON_PREFLIGHT_V23.json')
    args=parser.parse_args()
    inventory=json.loads((Path(__file__).parent/'weapon_branch_inventory.json').read_text(encoding='utf8'))['branches']
    try:result=audit({b:getattr(args,'assets_'+b) for b in BRANCHES},inventory)
    except ValueError as exc:parser.error(str(exc))
    text=json.dumps(result,indent=2,sort_keys=True)+'\n'
    if args.output:args.output.write_text(text,encoding='utf8')
    else:print(text,end='')
    print({b:(v['metadata_candidates'],v['total']) for b,v in result['branches'].items()},file=sys.stderr)
if __name__=='__main__':main()
