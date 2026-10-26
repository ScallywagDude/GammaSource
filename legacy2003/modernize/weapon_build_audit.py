#!/usr/bin/env python3
"""Compare per-branch registered weapons with BOTH client/server MSBuild inputs.

This is a compile-inclusion audit, not proof that a class links, spawns, or has
working animations. Windows project paths are intentionally compared without
case sensitivity; the 2003 source contains mixed-case filename references.
"""
from __future__ import annotations
import argparse
import json
import os
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
NS={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
BRANCH_CONFIGS={'hl2':'HL2','hl1':'HL1','cstrike':'CounterStrike','tf2':'TF2'}
PROJECTS={'server':'dlls/hl.vcxproj','client':'cl_dll/client.vcxproj'}

def windows_path(path):
    return os.path.normpath(str(path)).replace('\\','/').casefold()

def compiled_sources(project, configuration):
    """Return source paths that MSBuild includes for Configuration|Win32."""
    doc=ET.parse(project).getroot()
    names=set()
    for item in doc.findall('.//m:ClCompile',NS):
        name=item.get('Include')
        if not name:continue  # ItemDefinitionGroup is not a source item.
        excluded=False
        for prop in item.findall('m:ExcludedFromBuild',NS):
            condition=prop.get('Condition','')
            match=re.search(r"==\s*'([^']+)'",condition)
            if match and match.group(1).casefold()==(configuration+'|Win32').casefold():
                excluded=(prop.text or '').strip().casefold()=='true'
        if not excluded:
            names.add(windows_path(project.parent/name.replace('\\','/')))
    return names

def audit(root=ROOT):
    inventory=json.loads((root/'modernize/weapon_branch_inventory.json').read_text())['branches']
    report={'note':'Only server-source exclusion blocks server-side entity registration. Client inclusion of the same file is OPTIONAL and often intentionally absent. Project membership never proves compilation or runtime functionality.', 'branches':{}}
    for branch,cfg in BRANCH_CONFIGS.items():
        included={mode:{kind:compiled_sources(root/rel,mode+' '+cfg) for kind,rel in PROJECTS.items()} for mode in ('Debug','Release')}
        weapon_rows=[]
        for weapon in inventory[branch]:
            entry={'classname':weapon['classname'],'source':weapon['implementation_file']}
            key=windows_path(root/weapon['implementation_file'])
            for mode in ('Debug','Release'):
                entry[mode.lower()]={kind:key in included[mode][kind] for kind in PROJECTS}
            entry['server_compiled_all_configs']=all(entry[mode]['server'] for mode in ('debug','release'))
            entry['client_compiled_all_configs']=all(entry[mode]['client'] for mode in ('debug','release'))
            weapon_rows.append(entry)
        not_server=[w['classname'] for w in weapon_rows if not w['server_compiled_all_configs']]
        not_client=[w['classname'] for w in weapon_rows if not w['client_compiled_all_configs']]
        report['branches'][branch]={
            'registered':len(weapon_rows),'server_excluded':not_server,'client_source_not_shared':not_client,
            'weapons':weapon_rows,
        }
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'modernize/weapon_build_report.json')
    parser.add_argument('--strict',action='store_true',help='Return nonzero if any registered weapon is excluded from its branch build')
    args=parser.parse_args()
    result=audit()
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    for b,v in result['branches'].items():
        print('%s: registered=%d, server excluded=%d, client excluded=%d'%(b,v['registered'],len(v['server_excluded']),len(v['client_source_not_shared'])))
        if v['server_excluded']:print('  not in server:',', '.join(v['server_excluded']))
        if v['client_source_not_shared']:print('  source not shared with client (often intentional):',', '.join(v['client_source_not_shared']))
    if args.strict and any(v['server_excluded'] for v in result['branches'].values()):raise SystemExit(1)
