#!/usr/bin/env python3
"""Inventory real weapon registrations, shared tools and disabled prototypes.

Directory membership is supplemented by per-branch project membership for
shared server sources. A commented LINK_ENTITY_TO_CLASS is never registration.
Neither registration nor project membership proves working gameplay or art.
"""
from __future__ import annotations
import argparse, json, re
from pathlib import Path
from weapon_build_audit import compiled_sources, windows_path
ROOT=Path(__file__).resolve().parents[1]
BRANCHES={
    'hl2':[ROOT/'dlls/hl2_dll'],
    'hl1':[ROOT/'dlls/hl1_dll'],
    'cstrike':[ROOT/'game_shared/cstrike'],
    'tf2':[ROOT/'game_shared/tf2']
}
CONFIGS={'hl2':'HL2','hl1':'HL1','cstrike':'CounterStrike','tf2':'TF2'}
EXCLUDED={
 'weapon_cs_base':'weapon base class',
 'weapon_csbase_gun':'weapon base class; Spawn and PrimaryAttack assert',
 'weapon_basecsgrenade':'grenade base class without a concrete projectile',
 'weapon_basecombatobject':'TF2 object weapon base class',
 'weapon_combat_basegrenade':'TF2 grenade base class',
 'weapon_combat_grenade':'TF2 grenade base without a concrete projectile',
 'weapon_combat_usedwithshieldbase':'TF2 shield combination base class',
 'weapon_combat_plasmarifle_base':'TF2 plasma rifle base class',
 'weapon_objectselection':'internal builder selection entry; requires an initialized object subtype',
 'weapon_twohandedcontainer':'internal class-loadout support container; requires initialized compatible child weapons',
 'weapon_grenade_rocket':'internal rocket projectile derived from CBaseAnimating, not an equipable combat weapon',
 'weapon_cubemap':'shared cubemap debugging tool, not a branch gameplay weapon',
}
LINK=re.compile(r'\bLINK_ENTITY_TO_CLASS\s*\(\s*(weapon_[a-zA-Z0-9_]+)\s*,')
PATH=re.compile(r'"(models/[^"\n\\]+\.mdl)"',re.I)

def strip_comments(source):
    # Keep quoted paths intact, including // inside quoted strings.
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m:m.group() if m.group().startswith('"') else '',source,flags=re.S)

def audit(asset_root=None):
    branches={};prototypes={}
    for branch, directories in BRANCHES.items():
        registered={};disabled={}
        included=set()
        for mode in ('Debug','Release'):
            included |= compiled_sources(ROOT/'dlls/hl.vcxproj',mode+' '+CONFIGS[branch])
        directories=directories+[[p for p in sorted((ROOT/'dlls').glob('*.cpp')) if windows_path(p) in included]]
        for directory in directories:
            for file in (directory if isinstance(directory,list) else sorted(directory.glob('*.cpp'))):
                source=file.read_bytes().decode('latin1')
                active=strip_comments(source)
                names=set(LINK.findall(active))
                paths=sorted(set(PATH.findall(active)))
                for name in sorted(set(LINK.findall(source))-names):
                    disabled[name]={'classname':name,'implementation_file':file.relative_to(ROOT).as_posix(),
                                    'status':'unregistered_commented_prototype','runtime_verified':False}
                for name in names:
                    scripts=[]; resources=[]
                    if asset_root:
                        script=asset_root/'scripts'/f'{name}.txt'
                        scripts.append({'path':str(script.relative_to(asset_root)).replace('\\','/'),'present':script.is_file()})
                        for path in paths:
                            resources.append({'path':path,'present':(asset_root/path).is_file()})
                    registered[name]={
                        'implementation_file':file.relative_to(ROOT).as_posix(),
                        'source_scope':'shared_project_member' if file.parent==ROOT/'dlls' else 'branch',
                        'grant_exclusion_reason':EXCLUDED.get(name),
                        'implementation_status':'source_present_runtime_unverified',
                        'model_literals_in_same_file':paths,
                        'asset_checks':{'weapon_script':scripts,'explicit_models':resources},
                        'runtime_verified':False,'viewmodel_verified':False,
                        'hud_image_verified':False,'texture_scale_verified':False}
        branches[branch]=[dict(classname=k, **registered[k]) for k in sorted(registered)]
        prototypes[branch]=[disabled[k] for k in sorted(disabled) if k not in registered]
    return {'note':'Active source registrations plus branch-compiled shared tools; commented prototypes and unusable bases are explicit. Compilation, gameplay and asset quality remain unverified.',
            'branches':branches,'unregistered_prototypes':prototypes}

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--game-assets',type=Path,help='An authorized unpacked install containing models and scripts')
    ap.add_argument('--output',type=Path,default=ROOT/'modernize/weapon_branch_inventory.json')
    a=ap.parse_args()
    data=audit(a.game_assets)
    a.output.write_text(json.dumps(data,indent=2)+'\n')
    print('Branch-specific weapon inventory:', {k:len(v) for k,v in data['branches'].items()})
