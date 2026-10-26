"""Build hud1.vtf from the old-HUD atlas plus an ENERGY label in the same 5x7 label font.
E is copied from the atlas's own HEALTH label; N, R, G, Y follow the same grid, scanline
rows (dim rows 3 and 6) and faint top row."""
import sys
sys.path.insert(0, r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad")
from atlas import read_vtf_bgr888, write_vtf_bgr888

src, dst, png = sys.argv[1], sys.argv[2], sys.argv[3]
w, h, img = read_vtf_bgr888(src)
img = [list(r) for r in img]

GLYPHS = {
    'N': ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
}
BRIGHT, DIM, FAINT = (176, 188, 176), (116, 125, 116), (78, 82, 78)
DIM_ROWS = {2, 5}   # rows 34 and 37 of the original labels are dimmer scanlines

# Label cell: 14 tall like health_label; glyph rows start 4 px down (atlas rows 32..38 of a 28..41 cell).
LX, LY, LW, LH = 0, 80, 50, 14
for y in range(LY, LY + LH):
    for x in range(LX, LX + LW):
        img[y][x] = (0, 0, 0)

def copy_atlas_glyph(sx, dx):
    for oy in range(LH):
        for ox in range(8):
            img[LY + oy][dx + ox] = img[28 + oy][sx + ox]

def draw_glyph(rows, dx):
    for r, line in enumerate(rows):
        for c, ch in enumerate(line):
            if ch == '#':
                img[LY + 4 + r][dx + c] = DIM if r in DIM_ROWS else BRIGHT
                if r == 0:
                    img[LY + 3][dx + c] = FAINT

x = LX
for ch in "ENERGY":
    if ch == 'E':
        copy_atlas_glyph(170, x)   # the E from HEALTH (its strokes start at x=171)
    else:
        draw_glyph(GLYPHS[ch], x + 1)
    x += 8

write_vtf_bgr888(dst, w, h, img, 0x30c)

# Preview of the label strip, scaled 8x, for review.
import struct, zlib
crop = [row[0:60] for row in img[76:96]]
s = 8
rows = []
for r in crop:
    line = b''.join(bytes((p[0], p[1], p[2], 255)) * s for p in r)
    rows += [line] * s
def chunk(t, data):
    c = struct.pack('>I', len(data)) + t + data
    return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
raw = b''.join(b'\x00' + r for r in rows)
open(png, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 60 * s, 20 * s, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
print("ok", w, h)
