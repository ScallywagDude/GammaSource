"""Convert the hl2_2001 weapon models (MDL37) to MDL36 with the repo's model37 converter,
check their VTX files, and resolve every texture they reference across content layers.
--apply stages models, sidecars and the resolved materials into gammasource_content."""
import argparse, json, shutil, struct, sys
from pathlib import Path
ROOT = Path(r"C:\Users\doge6\Videos\GammaSource-main")
sys.path.insert(0, str(ROOT / 'modernize' / 'model_conversion'))
import model37

MEGA = Path.home() / 'Documents' / 'HL2BetaMegaCuck'
SRC = MEGA / 'hl2_2001'
DEST = ROOT / 'gammasource_content'
# Texture search: 2001 first, then nearby eras, then what the game already mounts.
LAYERS = [SRC, MEGA / 'hl2_2000', MEGA / 'hl2_2001-2002', MEGA / 'hl2_2002', DEST, MEGA / 'hl2_2003', MEGA / 'hl2']

def i32(b, o): return struct.unpack_from('<i', b, o)[0]
def cstr(b, o):
    e = b.find(b'\0', o); return b[o:e].decode('latin-1')

def textures(mdl):
    nt, ti, nc, ci = i32(mdl, 208), i32(mdl, 212), i32(mdl, 216), i32(mdl, 220)
    names = [cstr(mdl, ti + 32 * k + i32(mdl, ti + 32 * k)) for k in range(nt)]
    cds = [cstr(mdl, i32(mdl, ci + 4 * k)) for k in range(nc)]
    return names, cds

def find_ci(base, rel):
    """Case-insensitive lookup of rel under base."""
    cur = base
    for part in rel.replace('\\', '/').split('/'):
        if not part: continue
        if not cur.is_dir(): return None
        hit = next((c for c in cur.iterdir() if c.name.lower() == part.lower()), None)
        if hit is None: return None
        cur = hit
    return cur

def resolve(name, cds):
    for cd in cds or ['']:
        rel = ('materials/' + cd + name).replace('\\', '/')
        for layer in LAYERS:
            vmt = find_ci(layer, rel + '.vmt')
            if vmt and vmt.is_file():
                return layer, vmt, rel
    return None, None, None

def vmt_textures(vmt):
    out = []
    for line in vmt.read_text(errors='replace').splitlines():
        parts = [p for p in line.replace('\t', ' ').split('"') if p.strip()]
        if len(parts) >= 2 and parts[0].strip().lower() in ('$basetexture', '$bumpmap', '$envmapmask', '$detail', '$basetexture2', '$normalmap'):
            out.append(parts[1].strip())
    return out

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--apply', action='store_true'); a = ap.parse_args()
    report = []
    for mdl_path in sorted((SRC / 'models' / 'weapons').glob('*.mdl')):
        stem = mdl_path.with_suffix('')
        entry = {'model': mdl_path.name}
        data = mdl_path.read_bytes()
        try:
            conv, meta = model37.convert(data)
            sidecars = {}
            for suf in ('.dx80.vtx', '.dx7_2bone.vtx', '.dx90.vtx', '.sw.vtx'):
                p = Path(str(stem) + suf)
                if p.exists():
                    try:
                        model37.check_vtx(p.read_bytes(), conv); sidecars[suf] = p
                    except Exception as e:
                        entry.setdefault('bad_vtx', {})[suf] = str(e)
            if '.dx80.vtx' not in sidecars:
                raise model37.FormatError('no valid dx80 vtx')
            phy = Path(str(stem) + '.phy')
            entry['sequences'] = [s['label'] for s in meta['sequences']]
            names, cds = textures(conv)
            entry['cdmaterials'] = cds
            mats = []
            for n in names:
                layer, vmt, rel = resolve(n, cds)
                if not vmt:
                    mats.append({'name': n, 'found': False}); continue
                texs = []
                for t in vmt_textures(vmt):
                    tl = next((L for L in LAYERS if (find_ci(L, 'materials/' + t + '.vtf') or Path('/nonexistent')).is_file()), None)
                    texs.append({'texture': t, 'layer': tl.name if tl else None})
                mats.append({'name': n, 'found': True, 'layer': layer.name, 'vmt': rel, 'textures': texs})
            entry['materials'] = mats
            entry['status'] = 'ok'
            if a.apply:
                out = DEST / 'models' / 'weapons' / mdl_path.name.lower()
                out.parent.mkdir(parents=True, exist_ok=True)
                for existing in [out] + [Path(str(out)[:-4] + s) for s in sidecars] + [Path(str(out)[:-4] + '.phy')]:
                    if existing.exists() and not Path(str(existing) + '.before-2001.bak').exists():
                        shutil.copy2(existing, str(existing) + '.before-2001.bak')
                out.write_bytes(conv)
                for suf, p in sidecars.items():
                    shutil.copy2(p, str(out)[:-4] + suf)
                if phy.exists():
                    shutil.copy2(phy, str(out)[:-4] + '.phy')
                # Materials only when they come from an older-era layer (not already mounted).
                for m in mats:
                    if not m.get('found') or m['layer'] in ('gammasource_content', 'hl2_2003', 'hl2'): continue
                    layer = next(L for L in LAYERS if L.name == m['layer'])
                    files = [m['vmt'] + '.vmt'] + ['materials/' + t['texture'] + '.vtf' for t in m['textures'] if t['layer']]
                    for f in files:
                        srcf = None
                        for L in ([layer] + LAYERS):
                            srcf = find_ci(L, f)
                            if srcf and srcf.is_file(): break
                        if srcf and srcf.is_file():
                            dst = DEST / f
                            dst.parent.mkdir(parents=True, exist_ok=True)
                            if dst.exists() and not Path(str(dst) + '.before-2001.bak').exists():
                                shutil.copy2(dst, str(dst) + '.before-2001.bak')
                            shutil.copy2(srcf, dst)
        except Exception as e:
            entry['status'] = 'failed: ' + str(e)
        report.append(entry)
    if a.apply:
        # Every 2001 weapon texture wins over the later-era copies ("textures from here").
        src_mats = SRC / 'materials' / 'models' / 'weapons'
        for f in src_mats.rglob('*'):
            if not f.is_file(): continue
            rel = f.relative_to(SRC)
            existing = find_ci(DEST, rel.as_posix())
            dst = existing if existing and existing.is_file() else DEST / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            if dst.exists() and not Path(str(dst) + '.before-2001.bak').exists():
                shutil.copy2(dst, str(dst) + '.before-2001.bak')
            shutil.copy2(f, dst)
    print(json.dumps(report, indent=1))

main()
