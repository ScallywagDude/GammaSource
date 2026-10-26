"""Use the hl2_2001 weapon sound definitions: every soundscript entry from
hl2_2001/scripts/game_sounds_weapons.txt replaces (or is added to) the one in
gammasource_content when all of its waves exist in some content layer. 2001 wav
files are copied over the later ones. --apply writes; otherwise reports."""
import re, shutil, sys
from pathlib import Path
MEGA = Path.home() / 'Documents' / 'HL2BetaMegaCuck'
SRC = MEGA / 'hl2_2001'
DEST = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content")
LAYERS = [SRC, MEGA / 'hl2_2000', MEGA / 'hl2_2001-2002', MEGA / 'hl2_2002', DEST, MEGA / 'hl2_2003', MEGA / 'hl2']

def find_ci(base, rel):
    cur = base
    for part in rel.replace('\\', '/').split('/'):
        if not part: continue
        if not cur.is_dir(): return None
        hit = next((c for c in cur.iterdir() if c.name.lower() == part.lower()), None)
        if hit is None: return None
        cur = hit
    return cur

def blocks(text):
    """Top-level  "name" { ... }  entries with their exact text spans (comments kept outside)."""
    out, i, n = [], 0, len(text)
    pat = re.compile(r'"([^"]+)"\s*\{')
    while True:
        m = pat.search(text, i)
        if not m: break
        # skip matches inside a line comment
        line_start = text.rfind('\n', 0, m.start()) + 1
        if '//' in text[line_start:m.start()]:
            i = m.end(); continue
        depth, j = 0, m.end() - 1
        while j < n:
            if text[j] == '{': depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0: break
            elif text.startswith('//', j):
                j = text.find('\n', j)
                if j < 0: j = n
                continue
            j += 1
        out.append((m.group(1), m.start(), j + 1))
        i = j + 1
    return out

def waves(body):
    res = []
    for line in body.splitlines():
        s = line.split('//', 1)[0]
        m = re.search(r'"wave"\s*"([^"]+)"', s)
        if m: res.append(m.group(1).lstrip('*#@<>^)}!?').replace('\\', '/'))
    return res

def main(apply):
    src_text = (SRC / 'scripts' / 'game_sounds_weapons.txt').read_bytes().decode('latin-1')
    dst_path = DEST / 'scripts' / 'game_sounds_weapons.txt'
    dst_text = dst_path.read_bytes().decode('latin-1')
    dst_blocks = {name.lower(): (s, e) for name, s, e in blocks(dst_text)}
    replaced, added, skipped, copies = [], [], [], {}
    edits = []
    appended = []
    for name, s, e in blocks(src_text):
        body = src_text[s:e]
        missing = []
        for w in waves(body):
            rel = 'sound/' + w
            src2001 = find_ci(SRC, rel)
            if src2001 and src2001.is_file():
                copies[rel] = src2001; continue
            if not any((find_ci(L, rel) or Path('/x')).is_file() for L in LAYERS):
                missing.append(w)
        if missing:
            skipped.append((name, missing)); continue
        key = name.lower()
        if key in dst_blocks:
            ds, de = dst_blocks[key]
            if dst_text[ds:de].split() != body.split():
                edits.append((ds, de, body)); replaced.append(name)
        else:
            appended.append(body); added.append(name)
    print('replaced', len(replaced), replaced)
    print('added', len(added), added)
    print('skipped (missing waves)', skipped)
    print('2001 wavs to copy', sorted(copies))
    if not apply: return
    for ds, de, body in sorted(edits, reverse=True):
        dst_text = dst_text[:ds] + body + dst_text[de:]
    if appended:
        dst_text += '\r\n\r\n// --- hl2_2001 weapon sounds ---\r\n' + '\r\n\r\n'.join(appended) + '\r\n'
    bak = Path(str(dst_path) + '.before-2001.bak')
    if not bak.exists(): shutil.copy2(dst_path, bak)
    dst_path.write_bytes(dst_text.encode('latin-1'))
    for rel, src in copies.items():
        existing = find_ci(DEST, rel)
        dst = existing if existing and existing.is_file() else DEST / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists() and not Path(str(dst) + '.before-2001.bak').exists():
            shutil.copy2(dst, str(dst) + '.before-2001.bak')
        shutil.copy2(src, dst)
    # Any other 2001 weapon wav is copied too (they win over later versions).
    for f in (SRC / 'sound' / 'weapons').rglob('*.wav'):
        rel = f.relative_to(SRC).as_posix()
        existing = find_ci(DEST, rel)
        dst = existing if existing and existing.is_file() else DEST / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists() and not Path(str(dst) + '.before-2001.bak').exists():
            shutil.copy2(dst, str(dst) + '.before-2001.bak')
        shutil.copy2(f, dst)
    print('applied')

main('--apply' in sys.argv)
