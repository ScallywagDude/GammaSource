"""Swap the selection icon blocks (weapon / weapon_s) between weapon_smg1 and weapon_smg2."""
import re
from pathlib import Path
root = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content\scripts")
a, b = root / 'weapon_smg1.txt', root / 'weapon_smg2.txt'
ta, tb = a.read_bytes().decode('latin-1'), b.read_bytes().decode('latin-1')
for role in ('weapon', 'weapon_s'):
    pat = re.compile(r'"%s"\s*\{[^{}]*\}' % re.escape(role))
    ma, mb = pat.search(ta), pat.search(tb)
    ta = ta[:ma.start()] + mb.group(0) + ta[ma.end():]
    tb = tb[:mb.start()] + ma.group(0) + tb[mb.end():]
a.write_bytes(ta.encode('latin-1')); b.write_bytes(tb.encode('latin-1'))
print('swapped')
