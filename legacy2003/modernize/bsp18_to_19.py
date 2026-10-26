#!/usr/bin/env python3
"""Convert a 2002-2003 BSP v18 map into BSP v19 for the 2018 engine.

Only the structures that changed between the two are rewritten:
  * LUMP_FACES / LUMP_ORIGINALFACES: v18 dface_t starts with
    m_AvgLightColor[4] (16 bytes) that v19 dropped (72 -> 56 bytes per face).
    The shorter lump is written back at the same offset, so every other lump
    (and the absolute offsets inside the game lump) stays where it was.
  * LUMP_LEAFS: v18 leaves have no ambient light cube, which is the layout the
    2018 engine reads as leaf lump version 1. Reconstruct the absent sky flags
    so the 2018 client submits the sky pass in legacy maps that have a sky.
  * LUMP_WORLDLIGHTS: tagged as version 0 (no shadow_cast_offset).
The header version becomes 19 and the old fourCC field (v19+: uncompressed
size) is cleared. The input file is never modified. The engine applies the
same conversion in memory when it loads a v18 map (engine/modelloader.cpp).
"""
import argparse, struct, sys
from pathlib import Path

HEADER_LUMPS = 64
LUMP_LEAFS, LUMP_FACES, LUMP_WORLDLIGHTS, LUMP_ORIGINALFACES = 10, 7, 15, 27
FACE18, LEAF = 72, 32


def convert(data):
    if data[:4] != b'VBSP':
        raise ValueError('not a VBSP file')
    if struct.unpack_from('<i', data, 4)[0] != 18:
        raise ValueError('expected BSP v18, found v%d' % struct.unpack_from('<i', data, 4)[0])
    out = bytearray(data)
    for i in range(HEADER_LUMPS):
        h = 8 + 16 * i
        ofs, length, ver = struct.unpack_from('<iii', out, h)
        if length < 0 or (length and (ofs < 0 or ofs + length > len(data))):
            raise ValueError('lump %d out of range' % i)
        if i in (LUMP_FACES, LUMP_ORIGINALFACES) and length:
            if length % FACE18:
                raise ValueError('lump %d is not a whole number of v18 faces' % i)
            faces = b''.join(data[k + 16:k + FACE18] for k in range(ofs, ofs + length, FACE18))
            out[ofs:ofs + len(faces)] = faces
            out[ofs + len(faces):ofs + length] = bytes(length - len(faces))
            length = len(faces)
        if i == LUMP_LEAFS:
            if length % LEAF:
                raise ValueError('unexpected v18 leaf size')
            ver = 1
        if i == LUMP_WORLDLIGHTS:
            ver = 0
        struct.pack_into('<iii4s', out, h, ofs, length, ver, b'\0\0\0\0')
    texofs, texlen = struct.unpack_from('<ii', out, 8 + 16 * 6)
    leafofs, leaflen = struct.unpack_from('<ii', out, 8 + 16 * LUMP_LEAFS)
    if texlen % 72:
        raise ValueError('unexpected v18 texinfo size')
    if any(struct.unpack_from('<i', out, k + 64)[0] & 4
           for k in range(texofs, texofs + texlen, 72)):
        for k in range(leafofs, leafofs + leaflen, LEAF):
            if not struct.unpack_from('<i', out, k)[0] & 1:  # CONTENTS_SOLID
                area = struct.unpack_from('<H', out, k + 6)[0]
                struct.pack_into('<H', out, k + 6, area | (1 << 9))  # LEAF_FLAGS_SKY
    struct.pack_into('<i', out, 4, 19)
    return bytes(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('inputs', nargs='+', type=Path, help='v18 .bsp files or folders of them')
    ap.add_argument('--out', type=Path, required=True, help='destination maps folder')
    a = ap.parse_args(argv)
    a.out.mkdir(parents=True, exist_ok=True)
    files = []
    for p in a.inputs:
        files += sorted(p.glob('*.bsp')) if p.is_dir() else [p]
    ok = bad = 0
    for f in files:
        try:
            (a.out / f.name.lower()).write_bytes(convert(f.read_bytes()))
            ok += 1
        except (ValueError, struct.error) as e:
            print('skip', f.name, '-', e)
            bad += 1
    print('converted', ok, 'skipped', bad)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
