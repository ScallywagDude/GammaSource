#!/usr/bin/env python3
"""Targeted clang-cl syntax probes with the actual converted branch project flags.

This creates diagnostic JSON only: no object files, linking, or full game build.
Legacy header/compiler errors are reported as blockers, never as passing builds.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
NS={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
UNITS=[('HL2','dlls/player.cpp'),('HL2','dlls/hl2_dll/hl2_player.cpp'),
       ('HL2','dlls/hl2_dll/weapon_irifle.cpp'),('HL2','dlls/hl2_dll/weapon_tripwire.cpp'),
       ('HL2','dlls/hl2_dll/weapon_thumper.cpp'),('HL2','dlls/hl2_dll/weapon_immolator.cpp'),
       ('HL1','dlls/hl1_dll/hl1_player.cpp'),('CounterStrike','dlls/cstrike/cs_player.cpp'),
       ('CounterStrike','game_shared/basecombatweapon_shared.cpp'),
       ('TF2','dlls/tf2_dll/tf_player.cpp'),('TF2','game_shared/tf2/weapon_twohandedcontainer.cpp')]

def find_compiler():
    found=shutil.which('clang-cl')
    if found:return found
    root=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Microsoft Visual Studio'
    return next((str(p) for p in sorted(root.glob('*/*/VC/Tools/Llvm/x64/bin/clang-cl.exe')) if p.is_file()),None)

def flags(branch):
    project=ROOT/'dlls/hl.vcxproj';doc=ET.parse(project).getroot()
    config='Release '+branch+'|Win32'
    for group in doc.findall('m:ItemDefinitionGroup',NS):
        if config not in group.get('Condition',''):continue
        cc=group.find('m:ClCompile',NS)
        includes=(cc.findtext('m:AdditionalIncludeDirectories','',NS)).split(';')
        definitions=(cc.findtext('m:PreprocessorDefinitions','',NS)).split(';')
        return [('/I'+str((project.parent/p.replace('\\','/')).resolve())) for p in includes if p and not p.startswith('%(')] + [('/D'+p) for p in definitions if p and not p.startswith('%(')]
    raise ValueError('configuration not found: '+config)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default=find_compiler())
    parser.add_argument('--output',type=Path,default=ROOT/'modernize/reports/weapon_syntax_v18.json')
    args=parser.parse_args()
    if not args.compiler:parser.error('clang-cl unavailable; use --compiler PATH')
    report={'compiler':args.compiler,'kind':'targeted_translation_unit_syntax_probe','full_build_verified':False,'units':[]}
    for branch,rel in UNITS:
        command=[args.compiler,'--target=i686-pc-windows-msvc','/Zs','/Y-','/W0','/EHsc','/GR','/clang:-ferror-limit=12']+flags(branch)+[str(ROOT/rel)]
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,errors='replace')
        report['units'].append({'branch':branch,'source':rel,'exit_code':result.returncode,
                                'status':'syntax_passed_unlinked' if result.returncode==0 else 'compiler_blocked',
                                'command':command,'diagnostics':result.stdout+result.stderr})
        print(branch+': '+rel+': '+report['units'][-1]['status'],flush=True)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('Targeted probes:',sum(x['exit_code']==0 for x in report['units']),'passed;',sum(x['exit_code']!=0 for x in report['units']),'blocked')

if __name__=='__main__':main()
