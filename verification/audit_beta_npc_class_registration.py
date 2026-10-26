"""Audit original 2003 HL2 NPC registrations against effective HL2 build sources.

Unlike the earlier filename-only audit, registrations inside literal #if 0 blocks
are NOT counted as compiled. This is still a static source audit, not a Windows
build or runtime verification.
"""
from pathlib import Path
import json
import re
from audit_beta_npc_variants import active_lines

ROOT = Path(__file__).resolve().parent.parent
CLASS_RE = re.compile(r'LINK_ENTITY_TO_CLASS\s*\(\s*(npc_\w+)', re.I)
FILE_RE = re.compile(r'\$File\s+"([^"\r\n]+\.cpp)"', re.I)


def registrations(path, honor_disabled=True):
    found = {}
    for fp in sorted(path.rglob('*.cpp')):
        src = fp.read_text(encoding='latin1', errors='replace')
        if honor_disabled:
            src = active_lines(src)
        for cls in CLASS_RE.findall(src):
            found.setdefault(cls.lower(), set()).add(fp)
    return found


def main():
    old = registrations(ROOT/'legacy2003/dlls/hl2_dll', honor_disabled=False)
    current = registrations(ROOT/'game/server', honor_disabled=True)
    referenced = set()
    for path in (ROOT/'game/server/server_hl2.vpc', ROOT/'game/server/server_base.vpc'):
        for filename in FILE_RE.findall(path.read_text('latin1')):
            referenced.add(filename.lower().replace('\\','/'))
    records = []
    for name, files in sorted(old.items()):
        sources = sorted(p.relative_to(ROOT/'game/server').as_posix() for p in current.get(name, set()))
        listed = any(s.lower() in referenced for s in sources)
        original = sorted(p.relative_to(ROOT).as_posix() for p in files)
        # There can be inactive implementations in files listed by the build.
        inactive = []
        for fp in sorted((ROOT/'game/server').rglob('*.cpp')):
            # Quickly skip files without this classname.
            text = fp.read_text('latin1', errors='replace')
            if name not in text.lower():
                continue
            if name in (x.lower() for x in CLASS_RE.findall(text)) and name not in (x.lower() for x in CLASS_RE.findall(active_lines(text))):
                inactive.append(fp.relative_to(ROOT/'game/server').as_posix())
        records.append({'classname':name, '2003_source':original, 'current_source':sources,
                        'disabled_current_source':inactive,
                        'direct_hl2_vpc_entry':listed,
                        'status': 'registered_in_build_source' if listed else ('disabled_preprocessor' if inactive else ('source_only' if sources else 'no_active_registration'))})
    out = ROOT/'verification/beta_npc_class_audit.json'
    out.write_text(json.dumps({'limitations':[
        'Static matching only: Windows compilation, spawn behavior, assets, AI and animations unverified.',
        'Literal #if 0 regions are excluded from registered counts.',
        'Classnames and differing class implementations must be audited separately for beta authenticity.',
        'TF2/HL1 historical NPC/monster classes are inventoried in beta_npc_variant_audit.json.'
    ], 'classes':records},indent=2)+'\n',encoding='utf8')
    missing=[r for r in records if not r['direct_hl2_vpc_entry']]
    print('Original 2003 HL2 NPC registrations:',len(records))
    print('Names with active registration in listed HL2 source:',len(records)-len(missing))
    print('Missing/disabled:')
    for rec in missing:
        print(' ',rec['classname'], '=>',rec['status'])

if __name__=='__main__':
    main()
