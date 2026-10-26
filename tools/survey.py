"""Compare GammaSource (2003) with the 2018 engine: module presence, interface versions,
and 64-bit blockers in the 2003 tree."""
import re, os
from pathlib import Path
OLD = Path(r"C:\Users\doge6\Videos\GammaSource-main")
NEW = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")

def srcfiles(root, sub=''):
    base = root / sub
    for dp, dn, fn in os.walk(base):
        dn[:] = [d for d in dn if d not in ('build', '.git', '__pycache__')]
        for f in fn:
            if f.lower().endswith(('.cpp', '.h', '.c', '.inl')):
                yield Path(dp) / f

# 1. interface version strings
def versions(root):
    out = {}
    for f in srcfiles(root, 'public'):
        try: t = f.read_text(errors='replace')
        except Exception: continue
        for m in re.finditer(r'#define\s+(\w+_VERSION\w*)\s+"([^"]+)"', t):
            out.setdefault(m.group(1), m.group(2))
    return out
vo, vn = versions(OLD), versions(NEW)
print('== interfaces present in both, by version string')
same = diff = 0
rows = []
for k in sorted(set(vo) & set(vn)):
    if vo[k] == vn[k]: same += 1
    else:
        diff += 1; rows.append(f'  {k}: {vo[k]} -> {vn[k]}')
print(f'  same {same}, changed {diff}, only-2003 {len(set(vo)-set(vn))}, only-2018 {len(set(vn)-set(vo))}')
print('\n'.join(rows[:60]))

# 2. module dirs
print('== top-level modules')
od = {p.name.lower() for p in OLD.iterdir() if p.is_dir()}
nd = {p.name.lower() for p in NEW.iterdir() if p.is_dir()}
print('  only 2003:', sorted(od - nd))
print('  only 2018:', sorted(nd - od))

# 3. 64-bit blockers in 2003
asm = {}
ptrint = 0
for f in srcfiles(OLD):
    rel = f.relative_to(OLD).parts[0]
    try: t = f.read_text(errors='replace')
    except Exception: continue
    n = len(re.findall(r'\b__asm\b|\b_asm\b', t))
    if n: asm[rel] = asm.get(rel, 0) + n
    ptrint += len(re.findall(r'\(\s*(?:int|long|unsigned int|unsigned long|DWORD)\s*\)\s*(?:&|\w+\s*->|\(void\s*\*\))', t))
print('== inline asm blocks by module (2003):', dict(sorted(asm.items(), key=lambda kv: -kv[1])))
print('== pointer-to-int style casts (rough):', ptrint)
libs = sorted({p.name for p in (OLD / 'lib').rglob('*.lib')})
print('== prebuilt libs in 2003 lib/:', libs)
asmfiles = sorted({str(p.relative_to(OLD)) for p in OLD.rglob('*.asm') if 'build' not in p.parts})
print('== .asm files:', len(asmfiles), asmfiles[:20])
