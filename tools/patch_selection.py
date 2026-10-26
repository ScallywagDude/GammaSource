from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\cl_dll\hl2_hud\hud_weaponselection.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
start = d.index(b"void CHudWeaponSelection::Paint()")
end = d.index(b"//-----------------------------------------------------------------------------" + nl + b"// Purpose: draws a selection box")
new = Path(r"C:\Users\doge6\AppData\Local\Temp\claude\C--Users-doge6-Videos-GammaSource-main\a21c1dde-df0d-438a-98db-ab8502d5afe4\scratchpad\new_paint.cpp").read_bytes()
new = new.replace(b"\r\n", b"\n").replace(b"\n", nl)
d = d[:start] + new + d[end:]
inc = b'#include "../../modernize/hl1_weapon_buckets.h"'
assert d.count(inc) == 1
d = d.replace(inc, inc + nl + b'#include "ammodef.h"')
p.write_bytes(d)
print("ok")
