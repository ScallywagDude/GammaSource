#!/usr/bin/env python3
"""Platform-neutral structural validation for the VS 2026 conversion.

Pass means project files are syntactically valid and cross-references resolve.
It emphatically does NOT mean the engine compiles, starts, or runs correctly.
"""
from __future__ import annotations
import json, re, sys, xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NS={'x':'http://schemas.microsoft.com/developer/msbuild/2003'}
allprojs=list(ROOT.rglob('*.vcxproj'))
errors=[]
for f in allprojs:
    try:
        root=ET.parse(f).getroot()
        if root.tag!='{'+NS['x']+'}Project':
            errors.append(f'{f}: not an MSBuild Project root')
            continue
        guid=root.find('.//x:ProjectGuid',NS)
        if guid is None or not guid.text:
            errors.append(f'{f}: no ProjectGuid')
        configs=root.findall('.//x:ProjectConfiguration',NS)
        if not configs:
            errors.append(f'{f}: no configuration')
        for cfg in configs:
            parts=cfg.get('Include','').split('|')
            if len(parts)!=2 or parts[1]!='Win32':
                errors.append(f'{f}: invalid x86 configuration {cfg.get("Include")}')
        for t in root.findall('.//x:AdditionalDependencies',NS):
            if '/nodefaultlib' in (t.text or '').lower():
                errors.append(f'{f}: linker switch incorrectly parsed as library')
    except ET.ParseError as e:
        errors.append(f'{f}: invalid XML: {e}')
project_guids={}
for f in allprojs:
    if f.exists():
        try:
            el=ET.parse(f).getroot().find('.//x:ProjectGuid',NS)
            if el is not None:project_guids[f.resolve()]=el.text.upper()
        except ET.ParseError: pass
for sol in ['HL2_2003_VS2026.sln','HL2_2003_AllProjects_VS2026.sln']:
    p=ROOT/sol
    txt=p.read_text(encoding='utf-8-sig')
    matches=re.findall(r'^Project\("[^"\n]+"\)\s*=\s*"[^"]+",\s*"([^"]+\.vcxproj)",\s*"(\{[A-F0-9-]+\})"',txt,re.M|re.I)
    if not matches:errors.append(f'{sol}: no project entries')
    for rel,guid in matches:
        proj=(ROOT/rel.replace('\\','/')).resolve()
        if not proj.exists():errors.append(f'{sol}: missing project {rel}')
        elif project_guids.get(proj)!=guid.upper():errors.append(f'{sol}: GUID mismatch {rel}')
    print(f'{sol}: {len(matches)} project references')

for target in ['dlls/hl.vcxproj','cl_dll/client.vcxproj','engine/engine.vcxproj']:
    proj=ET.parse(ROOT/target).getroot()
    configs={x.get('Include') for x in proj.findall('.//x:ProjectConfiguration',NS)}
    if target!='engine/engine.vcxproj':
        for c in ['Debug HL2|Win32','Release HL2|Win32']:
            if c not in configs:errors.append(f'{target}: missing {c}')

report=json.loads((ROOT/'modernize/conversion_report.json').read_text())
if report.get('xml_errors'):errors.append('conversion report reports XML errors')
# VC6 per-file custom builds include generated pixel/vertex shader headers.
shader=ET.parse(ROOT/'materialsystem/stdshaders/stdshader_dx9.vcxproj').getroot()
shader_custom=shader.findall('.//x:CustomBuild',NS)
if len(shader_custom)<15:errors.append(f'stdshader_dx9 shader commands missing ({len(shader_custom)} converted)')
for entry in shader_custom:
    if not entry.findall('x:Command',NS) or not entry.findall('x:Outputs',NS):
        errors.append('shader custom build recipe missing command/output')
for path in allprojs:
    r=ET.parse(path)
    for dep in r.findall('.//x:AdditionalDependencies',NS):
        v=(dep.text or '').lower()
        if any(k in v for k in ('/implib:','\\implib:','/out:','\\out:','/pdb:','\\pdb:')):
            errors.append(f'{path}: linker metadata parsed as a library')

if report['converted']!=len(allprojs):errors.append('conversion report count mismatch')

# A missing compiled CPP is a tracked blocker, not structural failure.
miss=0;examples=[]
for obj in report['missing']:
    cpp=[m for m in obj['examples'] if m.lower().endswith(('.cpp','.c','.cxx','.rc'))]
    if cpp:examples.append({'project':obj['project'],'examples':cpp[:4]})
    miss+=obj['count']
# The inventory file has no runtime verification.
cut=json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
if len([x for x in cut if x['prototype_candidate']])<1: errors.append('cut NPC inventory empty')
if any(x['runtime_verified'] for x in cut):errors.append('prototype NPCs must not claim runtime verification')
assert 'npc_allow_unprecached_create' in (ROOT/'dlls/ai_concommands.cpp').read_bytes().decode('latin1')
assert 'HL2MODERN_XINPUT_BRIDGE' in (ROOT/'cl_dll/in_joystick.cpp').read_bytes().decode('latin1')
assert 'HL2MODERN_NPC_SPAWN_SAFETY' in (ROOT/'dlls/ai_concommands.cpp').read_bytes().decode('latin1')
assert 'HL2MODERN_RENDER_BACKEND_SELECTION' in (ROOT/'materialsystem/cmaterialsystem.cpp').read_bytes().decode('latin1')
if not (ROOT/'modernize/build_audit.json').exists():
    errors.append('build audit not generated')

summary={'projects':len(allprojs),'conversion_configs':report.get('configurations'),
         'missing_source_projects':len(report['missing']),'missing_source_references':miss,
         'npc_prototype_candidates':sum(x['prototype_candidate'] for x in cut),
         'windows_compiled':False,'renderer_upgrades_compiled':False,
         'shader_custom_build_items':len(shader_custom),
         'multiplayer_tested':False,'errors':errors,'missing_examples':examples[:10]}
(ROOT/'modernize/validation_report.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k not in ['missing_examples','errors']},indent=2))
if errors:
    print('ERRORS:')
    for x in errors[:35]:print(' *',x)
    sys.exit(1)
print('PASS: structural project checks (NOT Windows build validation)')
