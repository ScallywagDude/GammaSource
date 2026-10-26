"""Read/write the hud1 atlas (VTF BGR888) for label work."""
import struct

def read_vtf_bgr888(path):
    d = open(path, 'rb').read()
    hdr = struct.unpack_from('<i', d, 12)[0]
    w, h = struct.unpack_from('<HH', d, 16)
    fmt = struct.unpack_from('<i', d, 52)[0]
    mips = d[56]
    lrf = struct.unpack_from('<i', d, 57)[0]; lrw, lrh = d[61], d[62]
    assert fmt == 3, fmt
    off = hdr + ((max(1, (lrw + 3) // 4) * max(1, (lrh + 3) // 4) * 8) if lrw and lrh else 0)
    for m in range(mips - 1, 0, -1):
        off += max(1, w >> m) * max(1, h >> m) * 3
    data = d[off:off + w * h * 3]
    # rows of [r,g,b]
    return w, h, [[(data[(y * w + x) * 3 + 2], data[(y * w + x) * 3 + 1], data[(y * w + x) * 3]) for x in range(w)] for y in range(h)]

def write_vtf_bgr888(path, w, h, img, flags):
    """VTF 7.1 (64-byte header the 2003 loader expects), full mip chain, no low-res image."""
    mips = []
    cur = img; cw, ch = w, h
    mips.append(cur)
    while cw > 1 or ch > 1:
        nw, nh = max(1, cw // 2), max(1, ch // 2)
        nxt = []
        for y in range(nh):
            row = []
            for x in range(nw):
                px = [cur[min(ch - 1, 2 * y + dy)][min(cw - 1, 2 * x + dx)] for dy in (0, 1) for dx in (0, 1)]
                row.append(tuple(sum(p[i] for p in px) // 4 for i in range(3)))
            nxt.append(row)
        mips.append(nxt); cur = nxt; cw, ch = nw, nh
    hdr = bytearray(64)
    hdr[0:4] = b'VTF\0'
    struct.pack_into('<iii', hdr, 4, 7, 1, 64)
    struct.pack_into('<HHIHH', hdr, 16, w, h, flags, 1, 0)
    struct.pack_into('<fff', hdr, 32, 1.0, 1.0, 1.0)   # reflectivity (VectorAligned at 32)
    struct.pack_into('<f', hdr, 48, 1.0)               # bump scale
    struct.pack_into('<i', hdr, 52, 3)                 # BGR888
    hdr[56] = len(mips)
    struct.pack_into('<i', hdr, 57, -1)                # no low-res image
    hdr[61] = 0; hdr[62] = 0
    out = bytearray(hdr)
    for m in reversed(mips):                          # smallest first
        for row in m:
            for (r, g, b) in row:
                out += bytes((b, g, r))
    open(path, 'wb').write(out)
