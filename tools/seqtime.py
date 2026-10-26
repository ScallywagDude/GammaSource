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
    ns, so, ai = i32(d, 188), i32(d, 192), i32(d, 184)
    print(Path(p).name)
    for k in range(ns):
        r = so + 4284 * k
        anim = i32(d, r + 56)
        a = ai + 92 * anim
        fps, nf = f32(d, a + 4), i32(d, a + 12)
        print('  %-12s fps %.1f frames %d  %.2fs' % (cstr(d, r + i32(d, r)), fps, nf, (nf - 1) / fps if fps else 0))
