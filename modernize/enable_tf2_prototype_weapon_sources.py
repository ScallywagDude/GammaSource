#!/usr/bin/env python3
"""Opt-in TF2 prototype weapon compilation in the converted server project.

This enables only the six registered weapon translation units omitted from the
2003 TF2 server configurations. It does NOT restore client prediction, network
recv tables, viewmodels, sounds, or missing assets. These files may fail to
compile; keep this opt-in until Windows testing proves otherwise.

Usage after convert_dsp.py:
    py -3 modernize/enable_tf2_prototype_weapon_sources.py --enable
"""
import argparse
import os
import xml.etree.ElementTree as ET
from pathlib import Path
from weapon_build_audit import ROOT, audit, windows_path

MSBUILD='http://schemas.microsoft.com/developer/msbuild/2003'
NS=lambda tag:'{%s}%s'%(MSBUILD,tag)
ET.register_namespace('',MSBUILD)

def enable(project, root=ROOT):
    tree=ET.parse(project);doc=tree.getroot()
    old={os.path.normpath(x.get('Include').replace('\\','/')).replace('\\','/').casefold() for x in doc.iter(NS('ClCompile')) if x.get('Include')}
    items=None
    for group in doc.findall(NS('ItemGroup')):
        if group.find(NS('ClCompile')) is not None:items=group;break
    if items is None:raise ValueError('No ClCompile ItemGroup found')
    inventory=audit(root)['branches']['tf2']['weapons']
    added=[]
    for weapon in inventory:
        if weapon['server_compiled_all_configs']:continue
        path=(root/weapon['source']).resolve()
        if not path.is_file():raise FileNotFoundError(str(path))
        key=os.path.normpath('../'+weapon['source']).replace('\\','/').casefold()
        if key in old:continue  # Do not override intentional config exclusion.
        relative='..\\'+weapon['source'].replace('/','\\')
        item=ET.SubElement(items,NS('ClCompile'),{'Include':relative})
        for config in ('Debug HL2','Release HL2','Debug HL1','Release HL1','Debug CounterStrike','Release CounterStrike'):
            ET.SubElement(item,NS('ExcludedFromBuild'),{'Condition':"'$(Configuration)|$(Platform)'=='%s|Win32'"%config}).text='true'
        added.append(weapon['classname'])
        old.add(key)
    if added:
        tree.write(project,encoding='utf-8',xml_declaration=True)
    return added

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--enable',action='store_true',help='Actually edit the converted TF2 server .vcxproj')
    parser.add_argument('--project',type=Path,default=ROOT/'dlls/hl.vcxproj')
    args=parser.parse_args()
    if not args.enable:
        print('Opt-in only. Add --enable to include omitted TF2 source files. Compilation remains unverified.')
    else:
        added=enable(args.project)
        print('TF2-only experimental files added:',', '.join(added) if added else 'none')
        print('Re-running convert_dsp.py will reset these generated project edits.')
