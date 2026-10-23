#!/usr/bin/env python3
"""Make experimental impulse 101 grants use only branch-local server-compiled entities.

No .dsp/.vcxproj/build files are edited. If a future project explicitly includes
additional prototype weapons, rerunning this script will include them in grants.
"""
from __future__ import annotations
import json
import re
from pathlib import Path
from json_output_policy import write_json
from weapon_build_audit import audit
from weapon_branch_audit import EXCLUDED

ROOT = Path(__file__).resolve().parents[1]
SOURCE = {'hl2':'dlls/hl2_dll/hl2_player.cpp', 'cstrike':'dlls/cstrike/cs_player.cpp', 'tf2':'dlls/tf2_dll/tf_player.cpp'}
MARKERS = {'hl2':'HL2', 'cstrike':'CS', 'tf2':'TF2'}
REPORT = audit()
MANIFEST = json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())

for branch, rel in SOURCE.items():
    row = next(x for x in MANIFEST if x['branch'] == ('cs' if branch == 'cstrike' else branch))
    mapped = {w['classname']: w for w in REPORT['branches'][branch]['weapons']}
    # A weapon_ classname can identify an internal projectile or base class.
    # Use the same exclusions as the authoritative branch inventory, even when
    # refreshing a stale historical manifest whose source was server-compiled.
    internal = {name: EXCLUDED[name] for name in row['impulse101_extra'] if name in EXCLUDED}
    requested = [name for name in row['impulse101_extra'] if name not in EXCLUDED]
    if internal:
        row.setdefault('excluded_source_or_internal', {}).update(internal)
    row['impulse101_extra'] = requested
    # Only emit grants for classes registered in the current branch's server DLL
    # under BOTH the Debug and Release configurations. The original cheat's
    # normal grant list remains untouched.
    eligible = [n for n in requested if n in mapped and mapped[n]['server_compiled_all_configs']]
    excluded = [n for n in requested if n not in eligible]
    path = ROOT/rel
    raw = path.read_bytes()
    windows_line_ending = b'\r\n' in raw
    data = raw.decode('latin1').replace('\r\n','\n')
    marker = 'HL2MODERN_BRANCH_IMPULSE101_' + MARKERS[branch]
    pattern = r'('+re.escape(marker)+r':[^\n]*\n.*?const char \*const weapons\[\] = \{\n)(.*?)(\n\s*\};)'
    matches = list(re.finditer(pattern, data, re.S))
    if len(matches)!=1:
        raise RuntimeError('cannot identify exactly one weapon array in '+rel)
    names = ',\n'.join('\t\t\t\t"%s"'%name for name in eligible)+','
    if not eligible:
        raise RuntimeError('refusing empty grant array in '+rel)
    updated = data[:matches[0].start(2)] + names + data[matches[0].end(2):]
    if data!=updated:
        path.write_bytes((updated.replace('\n','\r\n') if windows_line_ending else updated).encode('latin1'))
    row['compiled_impulse101_extra'] = eligible
    row['excluded_not_compiled'] = excluded
    if row.get('schema_version',0)<18:
        row['note'] = 'Registered prototype weapons not compiled in this branch are excluded from impulse 101 until added to that branch project. Art and runtime behavior unverified.'

write_json(ROOT/'modernize/weapon_impulse101_manifest.json', MANIFEST)
print('Build-aware impulse101:',{b:len(next(x for x in MANIFEST if x['branch']==('cs' if b=='cstrike' else b))['compiled_impulse101_extra']) for b in SOURCE})
