"""Offline mock of CHudWeaponSelection::Paint using the real atlases and weapon scripts,
so the layout can be reviewed without driving the game on the user's desktop."""
import re, sys, struct, zlib
from pathlib import Path
SCR = Path(r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad")
ns = {}
exec(compile((SCR / "icon_audit.py").read_text().split("entries = []")[0].replace('content = Path(sys.argv[1])', 'content = None'), "a", "exec"), ns)
ns['layers'][:] = [Path(p) for p in sys.argv[1].split(';')]
scripts = Path(sys.argv[2]); open_slot = int(sys.argv[3]); sel_pos = int(sys.argv[4]); out = sys.argv[5]
load, block, kv = ns['load'], ns['block'], ns['kv']

W, H = 900, 720
img = [[(70, 66, 60)] * W for _ in range(H)]
INK = (232, 204, 107); RED = (255, 48, 0)
def blend(x, y, c, a):
    if 0 <= x < W and 0 <= y < H:
        b = img[y][x]; img[y][x] = tuple(int(b[i] + (c[i] - b[i]) * a) for i in range(3))
def rect(x0, y0, x1, y1, c, a=1.0):
    for y in range(y0, y1):
        for x in range(x0, x1): blend(x, y, c, a)
def outline(x0, y0, x1, y1, c, a=1.0):
    rect(x0, y0, x1, y0 + 1, c, a); rect(x0, y1 - 1, x1, y1, c, a); rect(x0, y0, x0 + 1, y1, c, a); rect(x1 - 1, y0, x1, y1, c, a)
def sprite(spec, x, y, w, h, c, a):  # additive-ish: atlas luminance as coverage
    at = load(spec[0]);
    if not at: return False
    aw, ah, rows = at; _, sx, sy, sw, sh = spec
    for dy in range(h):
        for dx in range(w):
            ax, ay = sx + dx * sw // w, sy + dy * sh // h
            p = rows[ay][ax * 4:ax * 4 + 4]
            cov = (p[0] + p[1] + p[2]) / 765 * (p[3] / 255)
            if cov > 0.02: blend(x + dx, y + dy, c, a * cov)
    return True

weapons = {}
for f in scripts.glob('weapon_*.txt'):
    t = f.read_text(errors='replace')
    b = kv(t, 'bucket'); p = kv(t, 'bucket_position')
    if b is None: continue
    def spec(role):
        bl = block(t, role)
        if not bl or re.search('"font"', bl): return None
        return (kv(bl, 'file'), int(kv(bl, 'x')), int(kv(bl, 'y')), int(kv(bl, 'width')), int(kv(bl, 'height')))
    weapons.setdefault(int(b), {})[int(p)] = (f.stem, spec('weapon'), spec('weapon_s'))
digits = {i: ('sprites/hud1', i * 20, 0, 20, 28) for i in range(10)}
frame = ('sprites/640hud3', 0, 180, 170, 45)
ox, oy, box, gap, iw, ih = 16, 8, 22, 4, 128, 64
x = ox
for slot in range(6):
    op = slot == open_slot
    outline(x, oy, x + box, oy + box, INK, 1 if op else .45)
    sprite(digits[(slot + 1) % 10], x + 6, oy + 4, 10, 14, INK, 1 if op else .45)
    y = oy + box + gap
    for i, pos in enumerate(sorted(weapons.get(slot, {}))):
        name, inact, act = weapons[slot][pos]
        if not op:
            rect(x, y, x + box, y + 6, INK, .45); y += 8; continue
        top = oy + box + gap + i * (ih + gap)
        cur = pos == sel_pos
        sp = (act if cur else inact) or act or inact
        if sp:
            if sp[3] == iw and sp[4] == ih: sprite(sp, x, top, iw, ih, INK, 1 if cur else .55)
            else:
                s = min(iw / sp[3], ih / sp[4], 1.0); w2, h2 = int(sp[3] * s), int(sp[4] * s)
                sprite(sp, x + (iw - w2) // 2, top + (ih - h2) // 2, w2, h2, INK, 1 if cur else .55)
        rect(x + iw - 7, top + 8, x + iw - 3, top + ih - 8, INK, .2)
        rect(x + iw - 7, top + 20, x + iw - 3, top + ih - 8, INK, 1 if cur else .55)
        if cur: sprite(frame, x - 3, top - 3, iw + 6, ih + 6, INK, 1)
    x += (iw if op else box) + gap
raw = b''.join(b'\x00' + bytes(v for p in r for v in p) for r in img)
def chunk(t, data):
    c = struct.pack('>I', len(data)) + t + data
    return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
print({k: [weapons[k][p][0] for p in sorted(weapons[k])] for k in sorted(weapons)})
