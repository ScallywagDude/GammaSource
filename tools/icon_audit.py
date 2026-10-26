"""List each weapon script's selection icons (weapon / weapon_s) and crop them into a contact sheet."""
import re, sys, struct, zlib
from pathlib import Path
SCR = Path(r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad")
sys.path.insert(0, str(SCR))
import importlib.util
spec = importlib.util.spec_from_file_location("v2p", SCR / "vtf2png.py")
src = (SCR / "vtf2png.py").read_text().replace("main(sys.argv[1], sys.argv[2])", "")
ns = {}
exec(compile(src, "vtf2png", "exec"), ns)

content = Path(sys.argv[1])
layers = [Path(p) for p in sys.argv[2].split(';')]

def find(rel):
    for root in layers:
        p = root / rel
        if p.exists():
            return p
    return None

def block(text, key):
    m = re.search(r'"%s"\s*\{([^{}]*(\{[^{}]*\}[^{}]*)*)\}' % re.escape(key), text, re.I)
    return m.group(1) if m else None

def kv(b, k):
    m = re.search(r'"%s"\s*"([^"]*)"' % k, b, re.I)
    return m.group(1) if m else None

atlas_cache = {}
def load(name):
    if name in atlas_cache:
        return atlas_cache[name]
    p = find(Path('materials') / (name + '.vtf'))
    r = None
    if p:
        d = p.read_bytes()
        hdr = struct.unpack_from('<i', d, 12)[0]; w, h = struct.unpack_from('<HH', d, 16)
        fmt = struct.unpack_from('<i', d, 52)[0]; mips = d[56]
        lrf = struct.unpack_from('<i', d, 57)[0]; lrw, lrh = d[61], d[62]
        def size(fm, ww, hh):
            if fm in (13, 14, 15):
                return max(1, (ww + 3) // 4) * max(1, (hh + 3) // 4) * (8 if fm == 13 else 16)
            return ww * hh * {0: 4, 1: 4, 2: 3, 3: 3, 12: 4, 16: 4}[fm]
        off = hdr + (size(lrf, lrw, lrh) if lrw and lrh and lrf >= 0 else 0)
        for m in range(mips - 1, 0, -1):
            off += size(fmt, max(1, w >> m), max(1, h >> m))
        data = d[off:off + size(fmt, w, h)]
        if fmt in (13, 15):
            rows = ns['dxt'](data, w, h, fmt == 15)
        else:
            bpp = 3 if fmt in (2, 3) else 4
            rows = []
            for y in range(h):
                px = []
                for x in range(w):
                    q = data[(y * w + x) * bpp:(y * w + x + 1) * bpp]
                    px += ([q[2], q[1], q[0], 255] if fmt == 3 else [q[0], q[1], q[2], 255] if fmt == 2 else [q[2], q[1], q[0], q[3]])
                rows.append(bytes(px))
        r = (w, h, rows)
    atlas_cache[name] = r
    return r

entries = []
for f in sorted(content.glob('weapon_*.txt')):
    if f.name == 'weapon_manifest.txt':
        continue
    t = f.read_text(errors='replace')
    row = [f.stem]
    for role in ('weapon', 'weapon_s'):
        b = block(t, role)
        if not b:
            row.append(None); continue
        if re.search(r'"font"', b):
            row.append(('font', kv(b, 'character'))); continue
        row.append((kv(b, 'file'), int(kv(b, 'x') or 0), int(kv(b, 'y') or 0), int(kv(b, 'width') or 0), int(kv(b, 'height') or 0)))
    entries.append(row)

for e in entries:
    print(e)

# sheet: each row name | weapon crop | weapon_s crop, crops scaled to 128x64 cell
CW, CH = 140, 70
W, H = 160 + 2 * CW, CH * len(entries)
canvas = [bytearray([30, 30, 30, 255] * W) for _ in range(H)]
for i, e in enumerate(entries):
    for j, spec_ in enumerate(e[1:]):
        if not spec_ or spec_[0] == 'font':
            continue
        a = load(spec_[0])
        if not a:
            for y in range(4, CH - 4):
                for x in range(4, CW - 4):
                    canvas[i * CH + y][(160 + j * CW + x) * 4:(160 + j * CW + x) * 4 + 4] = bytes([120, 0, 0, 255])
            continue
        aw, ah, rows = a
        _, sx, sy, sw, sh = spec_
        for y in range(min(sh, CH - 2)):
            for x in range(min(sw, CW - 2)):
                ax, ay = sx + x, sy + y
                if 0 <= ax < aw and 0 <= ay < ah:
                    px = rows[ay][ax * 4:ax * 4 + 4]
                else:
                    px = bytes([255, 0, 255, 255])
                cx = 160 + j * CW + x + 1
                canvas[i * CH + y + 1][cx * 4:cx * 4 + 4] = px
raw = b''.join(b'\x00' + bytes(r) for r in canvas)
def chunk(t, data):
    c = struct.pack('>I', len(data)) + t + data
    return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
open(SCR / 'icons' / 'audit.png', 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
