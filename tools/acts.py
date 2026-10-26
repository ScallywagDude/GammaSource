"""Print sequence label -> activity for MDL36/37 viewmodels (converted in memory)."""
import struct, sys
from pathlib import Path
sys.path.insert(0, r"C:\Users\doge6\Videos\GammaSource-main\modernize\model_conversion")
import model37
def i32(b, o): return struct.unpack_from('<i', b, o)[0]
def cstr(b, o): e = b.find(b'\0', o); return b[o:e].decode('latin-1')
for p in sys.argv[1:]:
    d = Path(p).read_bytes()
    if i32(d, 4) == 37: d, _ = model37.convert(d)
    ns, so = i32(d, 188), i32(d, 192)
    acts = []
    for k in range(ns):
        r = so + 4284 * k
        lab = cstr(d, r + i32(d, r)); act = cstr(d, r + i32(d, r + 4)) if i32(d, r + 4) else ''
        acts.append(f"{lab}={act}")
    print(Path(p).name, ' | '.join(acts))
