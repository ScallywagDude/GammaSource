"""Audit NPC identity across 2003 branches and compiled GammaSource source.

This deliberately distinguishes classname coverage from implementation provenance.
Run from any directory: py -3 verification/audit_beta_npc_variants.py
No assets or source files are rewritten; reports are reproducible.
"""
from __future__ import print_function
from pathlib import Path
from collections import defaultdict
from difflib import SequenceMatcher
import json
import re

ROOT = Path(__file__).resolve().parent.parent
NPC_PREFIXES = ('npc_', 'monster_')
ENTITY = re.compile(r'LINK_ENTITY_TO_CLASS\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)', re.I)
FILES = re.compile(r'\$File\s+"([^"\r\n]+\.cpp)"', re.I)
MODELS = re.compile(r'"(models/[^"\r\n]*?\.mdl)"', re.I)
BRANCHES = ('hl2_dll', 'tf2_dll', 'hl1_dll')


def active_lines(source):
    """Remove literal #if 0 blocks; other preprocessor conditionals remain unknown."""
    out = []
    frames = []
    for line in source.splitlines():
        s = line.strip()
        if re.match(r'^#\s*if\s+0\s*(?://.*)?$', s):
            frames.append({'disabled': True, 'else': False})
            continue
        if re.match(r'^#\s*(if|ifdef|ifndef)\b', s):
            frames.append({'disabled': False, 'else': False})
            continue
        if re.match(r'^#\s*(else|elif)\b', s):
            if frames:
                frames[-1]['else'] = True
            continue
        if re.match(r'^#\s*endif\b', s):
            if frames:
                frames.pop()
            continue
        if any(f['disabled'] and not f['else'] for f in frames):
            continue
        out.append(line)
    return '\n'.join(out)


def read(path):
    return path.read_text(encoding='latin1', errors='replace')


def registrations(folder):
    found = defaultdict(list)
    for p in sorted(folder.rglob('*.cpp')):
        text = read(p)
        live = active_lines(text)
        enabled = {(name.lower(), c.lower()) for name, c in ENTITY.findall(live)}
        for name, cppclass in ENTITY.findall(text):
            key = (name.lower(), cppclass.lower())
            found[name.lower()].append({
                'file': p.relative_to(ROOT).as_posix(),
                'cpp_type': cppclass,
                'literal_if0_disabled': key not in enabled,
            })
    return found


def vpc_sources(paths, base):
    found = set()
    for name in paths:
        for rel in FILES.findall(read(ROOT / name)):
            path = (base / rel.replace('\\', '/')).resolve()
            if path.is_file():
                found.add(path)
    return found


def main():
    legacy = {br: registrations(ROOT / 'legacy2003/dlls' / br) for br in BRANCHES}
    server = registrations(ROOT / 'game/server')
    active_server_paths = vpc_sources(('game/server/server_hl2.vpc', 'game/server/server_base.vpc'), ROOT / 'game/server')
    active_client_paths = vpc_sources(('game/client/client_hl2.vpc', 'game/client/client_base.vpc'), ROOT / 'game/client')
    compiled_server = defaultdict(list)
    for path in sorted(active_server_paths):
        for name, cls in ENTITY.findall(active_lines(read(path))):
            compiled_server[name.lower()].append({'file':path.relative_to(ROOT).as_posix(), 'cpp_type':cls})
    collisions = {name:files for name,files in compiled_server.items() if len(files)>1}

    rows=[]
    for br in BRANCHES:
        for name, definitions in sorted(legacy[br].items()):
            if not name.startswith(NPC_PREFIXES):
                continue
            matched = compiled_server.get(name, [])
            modern_definitions = server.get(name, [])
            comparisons=[]
            for old in definitions:
                old_path = ROOT / old['file']
                old_src = read(old_path)
                for new in modern_definitions:
                    newer_path = ROOT / new['file']
                    newer_src = read(newer_path)
                    ratio = SequenceMatcher(None, old_src.splitlines(), newer_src.splitlines()).quick_ratio()
                    comparisons.append({'old':old['file'], 'new':new['file'],
                                        'approx_line_similarity':round(ratio, 3),
                                        'same_bytes':old_src == newer_src,
                                        'newer_file_compiled':newer_path.resolve() in active_server_paths})
            models = sorted({m.replace('\\','/') for d in definitions for m in MODELS.findall(read(ROOT/d['file']))})
            status=('compiled' if matched else ('source_only' if modern_definitions else 'legacy_only'))
            rows.append({'branch':br, 'classname':name, 'legacy':definitions,
                         'compiled_implementations':matched,
                         'current_sources':modern_definitions,
                         'implementation_comparison':sorted(comparisons, key=lambda c:c['approx_line_similarity'], reverse=True)[:5],
                         'referenced_models':models[:25], 'status':status})
    rows.sort(key=lambda x:(BRANCHES.index(x['branch']), x['classname']))
    out={
        'notes':[
            'Classname coverage does not prove that beta NPC behavior has been retained.',
            'Same classname can refer to distinct historic and newer implementations.',
            'Preprocessor #if 0 is detected, but conditional configurations and function-level behavior need manual review.',
            'Only HL2 Waf server/client VPC source lists determine compiled registration here.',
            'Referenced models may not be present in the separate playable game installation.',
            'Original 2003 implementation is preferred for HL2 restoration; conflicting or unbuildable source requires a safe port.'
        ],
        'branch_totals':{b:{'npc_and_monster_names':sum(r['branch']==b for r in rows),
                            'compiled_in_hl2':sum(r['branch']==b and r['status']=='compiled' for r in rows)} for b in BRANCHES},
        'compiled_duplicate_registrations':collisions,
        'hydra_client_renderer_compiled':(ROOT / 'game/client/hl2/c_npc_hydra.cpp').resolve() in active_client_paths,
        'entities':rows,
    }
    (ROOT/'verification/beta_npc_variant_audit.json').write_text(json.dumps(out, indent=2)+'\n', encoding='utf8')
    counts=out['branch_totals']
    md=['# GammaSource: 2003 NPC variants and recycled classname audit', '',
        'Generated by `verification/audit_beta_npc_variants.py`.', '',
        '**Source/static analysis only. No claim of working NPCs or loaded assets.**', '',
        '| 2003 branch | NPC/monster names | Compiled into HL2 server (by classname) |',
        '| --- | ---: | ---: |']
    for b in BRANCHES:
        md.append('| '+b+' | '+str(counts[b]['npc_and_monster_names'])+' | '+str(counts[b]['compiled_in_hl2'])+' |')
    md += ['', '## Historical HL2 NPCs with no compiled registration', '']
    for r in rows:
        if r['branch']=='hl2_dll' and r['status']!='compiled':
            md.append('- `'+r['classname']+'`: '+r['status']+', sources: '+', '.join(d['file'] for d in r['current_sources']) )
    md += ['', '## Reused classnames that need behavior and model comparison',
           '', 'These have 2003 implementations but match the same classname in the active code. The name alone is not proof of beta behavior.', '']
    for r in rows:
        if r['branch']=='hl2_dll' and r['status']=='compiled':
            sims=r['implementation_comparison']
            similarity= str(sims[0]['approx_line_similarity']) if sims else 'none'
            md.append('- `'+r['classname']+'`: highest approximate source-line similarity **'+similarity+'**; '+
                      (sims[0]['new'] if sims else 'no matching source path'))
    md += ['', '## Conflicts and specific blockers', '']
    if collisions:
        for key, defs in sorted(collisions.items()):
            md.append('- Duplicate `'+key+'`: '+', '.join(d['file'] for d in defs))
    else:
        md.append('- No duplicate `LINK_ENTITY_TO_CLASS` registrations in the directly listed HL2 server sources (static check).')
    md += ['- `npc_turret_ceiling`: the historic `hl2/npc_turret.cpp` would register a duplicate alongside the currently compiled `hl2/npc_turret_ceiling.cpp`. Do not add that old file wholesale.',
           '- `npc_miniturret`: its registration inside the old turret file is under `#if 0`. It is not restored by merely listing the file.',
           '- `npc_sentry`: registration exists in the old file but its old turret base is not yet safely ported.',
           '- Legacy turret source has syntax and compatibility defects; isolate the turret variants for safe integration.',
           '- Hydra requires `game/client/hl2/c_npc_hydra.cpp` for the networked chain renderer; compiled entry: **'+str(out['hydra_client_renderer_compiled'])+'**.',
           '- The HL1 monsters and the three TF2 prototype bug NPCs are separately inventoried. Do not silently substitute their unrelated modern entities into the HL2 build.',
           '- Models, sounds, scripts and actual behavior must be checked in the target playable installation before marking any of these complete.',
           '', '## Preservation rule', '',
           'When modern and beta implementations use the same classname, prefer porting **beta behavior and assets**, with a single registration per classname. Keep a documented compatibility shim when needed rather than registering both. Preserve original 2003 gameplay, art, loading UI, beta hands, and the existing Annabelle/`impulse 101` work.', '']
    (ROOT/'verification/BETA_NPC_VARIANT_AUDIT.md').write_text('\n'.join(md),encoding='utf8')
    print('Branch counts:',counts)
    print('Compiled duplicate registrations:',len(collisions))
    print('Hydra renderer in HL2 client:',out['hydra_client_renderer_compiled'])
    print('Wrote variant reports')

if __name__ == '__main__':
    main()
