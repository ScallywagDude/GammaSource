"""Simulate studiorender's bone weight check on a converted v48 set; also show the v36 source."""
import struct, sys
from pathlib import Path
base = Path(sys.argv[1])          # cache path without extension
src = Path(sys.argv[2])           # source v36 .mdl
mdl = base.with_suffix('.mdl').read_bytes()
vvd = Path(str(base) + '.vvd').read_bytes()
vtx = Path(str(base) + '.dx90.vtx').read_bytes()
s36 = src.read_bytes()
I = lambda b, o: struct.unpack_from('<i', b, o)[0]
vstart = I(vvd, 56)
bad = 0; shown = 0
nbp, bpi = I(mdl, 232), I(mdl, 236)
nbpv, bpo = I(vtx, 28), I(vtx, 32)
for b in range(nbp):
    bo = bpi + 16 * b; nm = I(mdl, bo + 4); mi = bo + I(mdl, bo + 12)
    vb = bpo + 8 * b; vm = vb + I(vtx, vb + 4)
    for m in range(nm):
        mo = mi + 148 * m
        modelvert = I(mdl, mo + 84) // 48
        nme, mei = I(mdl, mo + 72), mo + I(mdl, mo + 76)
        vmo = vm + 8 * m
        for L in range(I(vtx, vmo)):
         lo = vmo + I(vtx, vmo + 4) + 12 * L
         meo = lo + I(vtx, lo + 4)
         for me in range(nme):
            mm = mei + 116 * me
            voff = I(mdl, mm + 12)
            vme = meo + 9 * me; ng = I(vtx, vme); go = vme + I(vtx, vme + 4)
            for g in range(ng):
                og = go + 25 * g
                nv, vo = I(vtx, og), I(vtx, og + 4)
                for v in range(nv):
                    p = og + vo + 9 * v
                    bw = vtx[p:p + 3]; nb = vtx[p + 3]; orig = struct.unpack_from('<H', vtx, p + 4)[0]
                    gidx = modelvert + voff + orig
                    vv = vstart + 48 * gidx
                    w = struct.unpack_from('<3f', vvd, vv); vnb = vvd[vv + 15]
                    tot = sum(w[bw[i]] for i in range(nb))
                    if nb > 0 and vnb <= 3 and abs(tot - 1) > 1e-3:
                        bad += 1
                        if shown < 5:
                            shown += 1
                            print('mesh', me, 'orig', orig, 'g', gidx, 'vtx bw', list(bw[:nb]), 'vvd w', w, 'vnb', vnb)
print('bad', bad)
