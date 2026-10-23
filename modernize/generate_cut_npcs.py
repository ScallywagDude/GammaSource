#!/usr/bin/env python3
"""Generate source evidence and experimental Hammer definitions.

Registration and project inclusion never prove model/animation/AI completeness.
"""
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'dlls' / 'hl2_dll'
CUT_CANDIDATES = {
    'npc_assassin': 'Assassin (early prototype)',
    'npc_bullsquid': 'Bullsquid (cut classic NPC)',
    'npc_houndeye': 'Houndeye (cut classic NPC)',
    'npc_hydra': 'Hydra (unfinished)',
    'npc_crabsynth': 'Crab Synth (cut Synth)',
    'npc_mortarsynth': 'Mortar Synth (cut Synth)',
    'npc_combineguard': 'Combine Guard (early prototype)',
    'npc_conscript': 'Conscript (disabled implementation)',
    'npc_ichthyosaur': 'Ichthyosaur (prototype)',
    'npc_lightstalk': 'Light Stalk (prototype)',
    'npc_rollerbull': 'Roller Bull (prototype)',
    'npc_missiledefense': 'Missile Defense (prototype)',
    'npc_roller': 'Roller (prototype base entity)',
    'npc_rollerbuddy': 'Roller Buddy (prototype)',
    'npc_rollerdozer': 'Roller Dozer (prototype)',
    'npc_rollerturret': 'Roller Turret (prototype)',
    'npc_sscanner': 'Shield Scanner (prototype)',
    'npc_wscanner': 'Wasteland Scanner (prototype)',
    'npc_combineelite': 'Combine Elite (early character)',
    'npc_odell': 'Odell (cut character)',
    'npc_launcher': 'Launcher (experimental entity)',
}
INHERITED_MODELS = {'npc_rollerbuddy': 'npc_roller', 'npc_rollerdozer': 'npc_roller',
                    'npc_rollerturret': 'npc_roller'}
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}


def strip_comments(text):
    # Preserve quoted paths, schedules and line numbers.
    token = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(token, lambda m: ('\n' * m[0].count('\n') if m[0].startswith(('//', '/*')) else m[0]), text)


def source_regions(text):
    """Yield (line, True/False/None) for known/disabled/conditional code."""
    stack = []
    for line in strip_comments(text).splitlines():
        match = re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', line)
        if match:
            directive, value = match.groups()
            value = value.strip().strip('()').strip()
            if directive in ('if', 'ifdef', 'ifndef'):
                state = (value != '0') if directive == 'if' and value in ('0', '1') else None
                stack.append([state, state])
            elif directive == 'else' and stack:
                prior = stack[-1][1]
                stack[-1][0] = None if prior is None else not prior
                stack[-1][1] = True
            elif directive == 'elif' and stack:
                prior = stack[-1][1]
                state = (value != '0') if value in ('0', '1') else None
                stack[-1][0] = False if prior is True else (None if prior is None else state)
                stack[-1][1] = True if prior is True or state is True else (None if prior is None or state is None else False)
            elif directive == 'endif' and stack:
                stack.pop()
            continue
        states = [item[0] for item in stack]
        yield line, (False if False in states else None if None in states else True)


def active_registrations(text):
    active = '\n'.join(line for line, state in source_regions(text) if state is True)
    return set(re.findall(r'\bLINK_ENTITY_TO_CLASS\s*\(\s*(npc_\w+)\s*,', active))


def call_references(text, calls):
    defines = dict(re.findall(r'^\s*#\s*define\s+(\w+)\s+"([^"\n]+)"', text, re.M))
    pattern = r'\b(?:' + '|'.join(calls) + r')\s*\(\s*(?:"([^"\n]+)"|(\w+))'
    return list(dict.fromkeys(value for literal, macro in re.findall(pattern, text) if (value := literal or defines.get(macro, ''))))


def project_inclusion():
    project = ET.parse(ROOT / 'dlls/hl.vcxproj')
    configs = [el.get('Include') for el in project.findall('.//m:ProjectConfiguration', NS)]
    result = {}
    for el in project.findall('.//m:ClCompile', NS):
        source = el.get('Include', '').replace('\\', '/').removeprefix('./').casefold()
        included = []
        for config in configs:
            blocked = False
            for exclude in el.findall('m:ExcludedFromBuild', NS):
                condition = exclude.get('Condition', '')
                if not condition or condition.endswith("=='" + config + "'"):
                    blocked = (exclude.text or '').strip().casefold() == 'true'
            if not blocked:
                included.append(config)
        result[source] = included
    return result


def inventory():
    compiled = project_inclusion()
    sources = [(p, strip_comments(p.read_bytes().decode('latin1'))) for p in sorted(FOLDER.glob('npc_*.cpp'))]
    entries = []
    for name, description in CUT_CANDIDATES.items():
        selected = next(((p, text) for p, text in sources if re.search(r'\bLINK_ENTITY_TO_CLASS\s*\(\s*' + re.escape(name) + r'\s*,', text)), None)
        if not selected:
            entries.append(dict(name=name, description=description, source=None,
                                registration_active=False, included_in_debug_hl2=False,
                                prototype_candidate=False, model_reference=None,
                                assets_verified=False, runtime_verified=False))
            continue
        path, text = selected
        active = '\n'.join(line for line, state in source_regions(text) if state is True)
        uncertain = '\n'.join(line for line, state in source_regions(text) if state is None)
        registered = name in active_registrations(text)
        # Keep disabled dependencies visible without enabling the old code.
        evidence = active if registered else text
        model_refs = call_references(evidence, ('SetModel', 'PrecacheModel', 'SpriteCreate', 'SpriteTrailCreate'))
        models = [ref for ref in model_refs if ref.casefold().endswith('.mdl')]
        spawn_body = re.search(r'\b\w+::Spawn\s*\([^)]*\)\s*\{(.*?)^\}', evidence, re.S | re.M)
        spawn_models = [ref for ref in call_references(spawn_body[1] if spawn_body else '', ('SetModel',)) if ref.casefold().endswith('.mdl')]
        # Macro definitions live outside Spawn, so resolve those against the file.
        if spawn_body and not spawn_models:
            definitions = '\n'.join(re.findall(r'^\s*#\s*define\s+\w+\s+"[^"\n]+"', evidence, re.M))
            spawn_models = [ref for ref in call_references(definitions+'\n'+spawn_body[1], ('SetModel',)) if ref.casefold().endswith('.mdl')]
        primary = next(iter(spawn_models), next(iter(models), None))
        rel = path.relative_to(ROOT).as_posix()
        configs = compiled.get(rel.removeprefix('dlls/').casefold(), [])
        entries.append(dict(
            name=name, description=description, source=rel, registration_active=registered,
            registration_conditional=name in set(re.findall(r'\bLINK_ENTITY_TO_CLASS\s*\(\s*(npc_\w+)\s*,', uncertain)),
            included_in_debug_hl2='Debug HL2|Win32' in configs,
            included_build_configurations=configs,
            prototype_candidate=registered and 'Debug HL2|Win32' in configs,
            model_reference=primary, model_references=models,
            effect_material_references=[ref for ref in model_refs if ref.casefold().endswith('.vmt')],
            sound_file_references=sorted(set(re.findall(r'"([^"\n]+\.wav)"', evidence, re.I))),
            sound_event_references=sorted(set(re.findall(r'\bEmitSound\s*\(\s*"([^"\n]+)"', evidence)) - set(re.findall(r'"([^"\n]+\.wav)"', evidence, re.I))),
            named_attachments=call_references(evidence, ('LookupAttachment', 'GetAttachment')),
            named_sequences=call_references(evidence, ('LookupSequence',)),
            activity_references=sorted(set(re.findall(r'\bACT_[A-Z0-9_]+\b', evidence)) - {'ACT_INVALID'}),
            charge_hit_source_patch='HL2MODERN_CRABSYNTH_CHARGE_DAMAGE' in path.read_bytes().decode('latin1'),
            sonic_attack_source_patch='HL2MODERN_HOUNDEYE_SONIC_RESTORATION' in path.read_bytes().decode('latin1'),
            melee_attack_source_patch='HL2MODERN_BULLSQUID_COMBAT_RESTORATION' in path.read_bytes().decode('latin1'),
            assets_verified=False, runtime_verified=False))
    lookup = {entry['name']: entry for entry in entries}
    for entry in entries:
        inherited = INHERITED_MODELS.get(entry['name'])
        if inherited and not entry['model_reference']:
            entry['model_reference'] = lookup[inherited]['model_reference']
            entry['model_references'] = lookup[inherited]['model_references'][:]
            entry['model_inherited_from'] = inherited
    lookup['npc_assassin']['named_attachments'] = sorted(set(lookup['npc_assassin']['named_attachments'] + ['LeftMuzzle', 'RightMuzzle']))
    lookup['npc_launcher']['model_role'] = 'invisible_engine_dummy_not_restored_appearance'
    lookup['npc_launcher']['dynamic_asset_keyfields'] = ['MissileModel', 'LaunchSound', 'FlySound']
    return entries


def main():
    entries = inventory()
    (ROOT / 'modernize/cut_npc_inventory.json').write_text(json.dumps(entries, indent=2) + '\n', encoding='utf-8')
    fgd = ['// HL2 2003 prototype NPCs - experimental Hammer definitions.',
           '// Model geometry is shown when legally installed. Definitions do not supply assets.',
           '// AI, animation, hitboxes, audio and runtime behavior require verification.',
           '// Load as an additional FGD; preserve the game FGD.', '']
    for entry in entries:
        if not entry['prototype_candidate']:
            continue
        model = entry['model_reference'] if entry.get('model_role') != 'invisible_engine_dummy_not_restored_appearance' else None
        display = f'studio("{model}")' if model else 'size(-16 -16 0, 16 16 72)'
        fgd.extend([f'@PointClass color(255 145 40) {display} = {entry["name"]} : "{entry["description"]}"', '[',
                    '  targetname(string) : "Name" : ""', '  angles(string) : "Orientation" : "0 0 0"'])
        if entry['name']=='npc_launcher':
            fgd.extend(['  MissileModel(studio) : "Missile model (required for firing)" : ""',
                        '  LaunchSound(sound) : "Optional launch sound" : ""',
                        '  FlySound(sound) : "Optional flight sound" : ""',
                        '  StartOn(choices) : "Start active" : 0 = [0 : "No" 1 : "Yes"]'])
        fgd.extend([']', ''])
    (ROOT / 'modernize/hl2_2003_cut_npcs.fgd').write_text('\n'.join(fgd) + '\n', encoding='utf-8')
    print(f'Cut NPCs inventoried: {len(entries)}, experimental definitions: {sum(e["prototype_candidate"] for e in entries)}')


if __name__ == '__main__':
    main()
