"""List entity classes (LINK_ENTITY_TO_CLASS) in the 2003 HL2 game code that the 2018 game code lacks."""
import re, os, json
from pathlib import Path
OLD = Path(r"C:\Users\doge6\Videos\GammaSource-main")
NEW = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")
pat = re.compile(r'LINK_ENTITY_TO_CLASS\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)')

def scan(roots, skip=()):
    out = {}
    for root in roots:
        for dp, dn, fn in os.walk(root):
            dn[:] = [d for d in dn if d.lower() not in ('build',) + skip]
            for f in fn:
                if not f.endswith('.cpp'): continue
                p = Path(dp) / f
                try: t = p.read_text(errors='replace')
                except Exception: continue
                for m in pat.finditer(t):
                    out.setdefault(m.group(1).lower(), str(p))
    return out

old = scan([OLD / 'dlls', OLD / 'game_shared'], skip=('tf2_dll', 'hl1_dll', 'cstrike', 'tf2', 'hl1'))
new = scan([NEW / 'game' / 'server', NEW / 'game' / 'shared'], skip=('tf', 'hl1', 'cstrike', 'dod', 'portal', 'episodic_only', 'tfc'))
missing = {k: v for k, v in old.items() if k not in new}
by_file = {}
for k, v in missing.items():
    by_file.setdefault(os.path.relpath(v, OLD), []).append(k)
print('2003 HL2 entity classes:', len(old), '| in 2018:', len(set(old) & set(new)), '| missing from 2018:', len(missing))
for f in sorted(by_file):
    print(' ', f, ':', ', '.join(sorted(by_file[f])))
json.dump({'missing': missing}, open(r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad\missing_entities.json", 'w'), indent=1)
