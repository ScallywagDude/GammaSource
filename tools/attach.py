"""List attachments and sequence events of MDL36/37 models."""
import struct, sys
from pathlib import Path
sys.path.insert(0, r"C:\Users\doge6\Videos\GammaSource-main\modernize\model_conversion")
import model37
def i32(b, o): return struct.unpack_from('<i', b, o)[0]
def f32(b, o): return struct.unpack_from('<f', b, o)[0]
def cstr(b, o): e = b.find(b'\0', o); return b[o:e].decode('latin-1')
for p in sys.argv[1:]:
    d = Path(p).read_bytes()
    if i32(d, 4) == 37: d, _ = model37.convert(d)
    na, ai = i32(d, 244), i32(d, 248)
    names = [cstr(d, ai + 60 * k + i32(d, ai + 60 * k)) for k in range(na)] if 0 <= na < 64 else []
    print(Path(p).name, 'attachments', names)
    ns, so = i32(d, 188), i32(d, 192)
    for k in range(ns):
        r = so + 4284 * k
        lab = cstr(d, r + i32(d, r))
        nev, evi = i32(d, r + 20), i32(d, r + 24)
        evs = [(round(f32(d, r + evi + 76 * e), 3), i32(d, r + evi + 76 * e + 4), cstr(d, r + evi + 76 * e + 12)) for e in range(nev)] if 0 <= nev < 64 else []
        print('  seq', lab, 'events', evs)
