#!/usr/bin/env python3
"""Expand the HL2-only, server-vetted humanoid model picker.

All entries are potential assets; the existing UI and server code only expose
models whose MDL file exists in the game installation. No other game branches
are modified. Missing portraits are reported honestly rather than fabricated.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT/'modernize/hl2_playermodel_policy.h'
data = POLICY.read_text()
start = data.index('static const Entry kModels[] = {')
end = data.index('\n};',start)+3
models = [
  ('Classic Player','models/player.mdl','player'),
  ('Combine Soldier','models/combine_soldier.mdl','combine_soldier'),
  ('Combine Elite','models/combine_super_soldier.mdl','combine_super_soldier'),
  ('Metropolice','models/police.mdl','police'),
]
for group, label in [('group01','Citizen'),('group03','Rebel'),('group03m','Medic Rebel')]:
    for sex, count in [('male',9 if group!='group03m' else 5),('female',7 if group!='group03m' else 5)]:
        for number in range(1,count+1):
            n = '%02d'%number
            models.append((f'{label} {sex.capitalize()} {n}',f'models/humans/{group}/{sex}_{n}.mdl',f'humans_{group}_{sex}_{n}'))
paths = [path for _,path,_ in models]
assert len(paths)==len(set(paths))
assert len(models)<=64 # avoid extreme model precache pressure
assert all(len(p)<96 and re.fullmatch('[a-z0-9_/.]+',p) for p in paths)
lines=['static const Entry kModels[] = {']
for label,path,portrait in models:
    lines.append(f'    {{ "{label}", "{path}", "{portrait}" }},')
lines[-1] = lines[-1][:-1]
lines.append('};')
modified=data[:start]+'\n'.join(lines)+data[end:]
if modified!=data:POLICY.write_text(modified)
print(f'HL2 model picker: {len(models)} allowlisted model paths (installed models only).')
