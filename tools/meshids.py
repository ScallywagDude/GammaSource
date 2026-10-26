"""Report v48 models whose mesh ids are not 0..n-1 in model order."""
import struct, sys
from pathlib import Path
I = lambda d, o: struct.unpack_from('<i', d, o)[0]
bad = 0; total = 0
for p in Path(sys.argv[1]).rglob('*.mdl'):
    d = p.read_bytes()
    if I(d, 4) != 48: continue
    total += 1
    ids = []
    bpi = I(d, 236)
    for b in range(I(d, 232)):
        bo = bpi + 16 * b; mi = bo + I(d, bo + 12)
        for m in range(I(d, bo + 4)):
            mo = mi + 148 * m; mei = mo + I(d, mo + 76)
            for q in range(I(d, mo + 72)):
                ids.append(I(d, mei + 116 * q + 32))
    if ids and (max(ids) >= len(ids) or len(set(ids)) != len(ids)):
        bad += 1
        if bad < 15: print(p.name, len(ids), ids[:12])
print('models', total, 'odd meshids', bad)
