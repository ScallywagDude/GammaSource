"""Dump mip 0 of an uncompressed BGR888/BGRA8888 VTF (7.x) to PNG, plus DXT1/DXT5 via simple decoders."""
import struct, sys, zlib

def png(path, w, h, rows):
    raw = b''.join(b'\x00' + r for r in rows)
    def chunk(t, data):
        c = struct.pack('>I', len(data)) + t + data
        return c + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

def c565(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31

def dxt(data, w, h, alpha):
    out = [[(0, 0, 0, 0)] * w for _ in range(h)]
    bs = 16 if alpha else 8
    i = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            blk = data[i:i + bs]; i += bs
            a = [255] * 16
            if alpha:
                a0, a1 = blk[0], blk[1]
                bits = int.from_bytes(blk[2:8], 'little')
                pal = [a0, a1] + ([((6 - k) * a0 + (k + 1) * a1) // 7 for k in range(6)] if a0 > a1 else
                                  [((4 - k) * a0 + (k + 1) * a1) // 5 for k in range(4)] + [0, 255])
                a = [pal[(bits >> (3 * k)) & 7] for k in range(16)]
                blk = blk[8:]
            c0, c1, bits = struct.unpack('<HHI', blk)
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1 or alpha:
                cols = [p0, p1, tuple((2 * x + y) // 3 for x, y in zip(p0, p1)), tuple((x + 2 * y) // 3 for x, y in zip(p0, p1))]
            else:
                cols = [p0, p1, tuple((x + y) // 2 for x, y in zip(p0, p1)), (0, 0, 0)]
            for k in range(16):
                x, y = bx + k % 4, by + k // 4
                if x < w and y < h:
                    r, g, b = cols[(bits >> (2 * k)) & 3]
                    out[y][x] = (r, g, b, a[k])
    return [bytes(v for px in row for v in px) for row in out]

def main(src, dst):
    d = open(src, 'rb').read()
    hdr = struct.unpack_from('<i', d, 12)[0]
    w, h = struct.unpack_from('<HH', d, 16)
    fmt = struct.unpack_from('<i', d, 52)[0]
    mips = d[56]
    lrf = struct.unpack_from('<i', d, 57)[0]; lrw, lrh = d[61], d[62]
    def size(fm, ww, hh):
        if fm in (13, 14, 15):
            return max(1, (ww + 3) // 4) * max(1, (hh + 3) // 4) * (8 if fm == 13 else 16)
        return ww * hh * {0: 4, 1: 4, 2: 3, 3: 3, 12: 4, 16: 4}[fm]
    off = hdr + (size(lrf, lrw, lrh) if lrw and lrh else 0)
    for m in range(mips - 1, 0, -1):  # smallest mips first
        off += size(fmt, max(1, w >> m), max(1, h >> m))
    data = d[off:off + size(fmt, w, h)]
    if fmt in (13, 15):
        rows = dxt(data, w, h, fmt == 15)
    else:
        bpp = 3 if fmt in (2, 3) else 4
        rows = []
        for y in range(h):
            r = data[y * w * bpp:(y + 1) * w * bpp]
            px = []
            for x in range(w):
                p = r[x * bpp:(x + 1) * bpp]
                if fmt == 3: px += [p[2], p[1], p[0], 255]
                elif fmt == 2: px += [p[0], p[1], p[2], 255]
                elif fmt == 12: px += [p[2], p[1], p[0], p[3]]
                else: px += [p[0], p[1], p[2], p[3]]
            rows.append(bytes(px))
    png(dst, w, h, rows)
    print(w, h, fmt, mips, hdr)

main(sys.argv[1], sys.argv[2])
