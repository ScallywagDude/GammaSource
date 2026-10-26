"""Compare two v48 MDLs structurally (header fields, bodyparts/models/meshes)."""
import struct, sys
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
I = lambda d, o: struct.unpack_from('<i', d, o)[0]
names = {156: 'numbones', 164: 'numctrl', 172: 'numhbset', 180: 'numanim', 188: 'numseq', 204: 'numtex', 212: 'numcd',
         220: 'nskinref', 224: 'nskinfam', 232: 'numbp', 240: 'numatt', 248: 'numnodes', 284: 'numik', 300: 'numpose', 316: 'kvsize'}
for o, n in names.items():
    if I(a, o) != I(b, o): print('hdr', n, I(a, o), I(b, o))
for o in range(0, 408, 4):
    if o in names or o - 4 in names: continue
    if a[o:o + 4] != b[o:o + 4] and o not in (76,): print('hdr@%d' % o, I(a, o), I(b, o))
def walk(d):
    out = []
    bpi = I(d, 236)
    for p in range(I(d, 232)):
        bo = bpi + 16 * p
        mi = bo + I(d, bo + 12)
        for m in range(I(d, bo + 4)):
            mo = mi + 148 * m
            rec = ['model', p, m] + [I(d, mo + x) for x in range(64, 148, 4)]
            out.append(rec)
            mei = mo + I(d, mo + 76)
            for q in range(I(d, mo + 72)):
                mm = mei + 116 * q
                out.append(['mesh', p, m, q] + [I(d, mm + x) for x in range(0, 116, 4) if x not in (4, 20)])
    return out
wa, wb = walk(a), walk(b)
for x, y in zip(wa, wb):
    if x != y: print('A', x); print('B', y)
print(len(wa), len(wb))
