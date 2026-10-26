"""Raw 8-bit RGB -> VTF 7.1 BGR888 with a full mip chain (the format of the selector icon atlases).
usage: rgb2vtf.py in.rgb width height out.vtf"""
import struct, sys

src, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
d = open(src, 'rb').read()
img = [[tuple(d[(y * w + x) * 3:(y * w + x) * 3 + 3]) for x in range(w)] for y in range(h)]

mips, cur, cw, ch = [img], img, w, h
while cw > 1 or ch > 1:
    nw, nh = max(1, cw // 2), max(1, ch // 2)
    nxt = []
    for y in range(nh):
        row = []
        for x in range(nw):
            px = [cur[min(ch - 1, 2 * y + dy)][min(cw - 1, 2 * x + dx)] for dy in (0, 1) for dx in (0, 1)]
            row.append(tuple(sum(p[i] for p in px) // 4 for i in range(3)))
        nxt.append(row)
    mips.append(nxt); cur, cw, ch = nxt, nw, nh

hdr = bytearray(64)
hdr[0:4] = b'VTF\0'
struct.pack_into('<iii', hdr, 4, 7, 1, 64)
struct.pack_into('<HHIHH', hdr, 16, w, h, 0x2000 | 0x4 | 0x8, 1, 0)	# no LOD, clamp S/T
struct.pack_into('<fff', hdr, 32, 1.0, 1.0, 1.0)
struct.pack_into('<f', hdr, 48, 1.0)
struct.pack_into('<i', hdr, 52, 3)	# BGR888
hdr[56] = len(mips)
struct.pack_into('<i', hdr, 57, -1)
data = bytearray(hdr)
for m in reversed(mips):
    for row in m:
        for (r, g, b) in row:
            data += bytes((b, g, r))
open(out, 'wb').write(data)
print('wrote', out)
