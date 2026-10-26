import struct, sys, zlib
d = open(sys.argv[1], 'rb').read()
idlen, cmap, itype = d[0], d[1], d[2]
w, h, bpp, desc = struct.unpack_from('<HHBB', d, 12)
assert itype == 2 and bpp in (24, 32), (itype, bpp)
n = bpp // 8
px = d[18 + idlen:]
rows = []
for y in range(h):
    sy = y if desc & 0x20 else h - 1 - y
    r = px[sy * w * n:(sy + 1) * w * n]
    out = bytearray()
    for x in range(w):
        b, g, rr = r[x * n], r[x * n + 1], r[x * n + 2]
        out += bytes((rr, g, b))
    rows.append(bytes(out))
def chunk(t, data):
    c = struct.pack('>I', len(data)) + t + data
    return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
raw = b''.join(b'\x00' + r for r in rows)
open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
