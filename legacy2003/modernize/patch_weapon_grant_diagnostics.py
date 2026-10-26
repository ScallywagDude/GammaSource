#!/usr/bin/env python3
"""Report actual successes/failures when giving registered prototype weapons.

No changes to the weapon grant list or default build configurations. This
instruments the three existing branch-specific impulse 101 loops and leaves
HL1's historical grant implementation unchanged.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES=(
 'dlls/hl2_dll/hl2_player.cpp',
 'dlls/cstrike/cs_player.cpp',
 'dlls/tf2_dll/tf_player.cpp'
)
needle='''\t\t\tfor (unsigned int k=0; k<sizeof(weapons)/sizeof(weapons[0]); ++k)
\t\t\t\tGiveNamedItem(weapons[k]);'''
replacement='''\t\t\t// HL2MODERN_IMPULSE101_RESULT: record what the player actually owns.
\t\t\t// A registered classname is not proof that its scripts/models work.
\t\t\tunsigned int given=0, already=0, failed=0;
\t\t\tfor (unsigned int k=0; k<sizeof(weapons)/sizeof(weapons[0]); ++k)
\t\t\t{
\t\t\t\tif (HasNamedPlayerItem(weapons[k])) { ++already; continue; }
\t\t\t\tGiveNamedItem(weapons[k]);
\t\t\t\tif (HasNamedPlayerItem(weapons[k])) ++given;
\t\t\t\telse
\t\t\t\t{
\t\t\t\t\t++failed;
\t\t\t\t\tMsg("impulse 101 could not equip %s\\n", weapons[k]);
\t\t\t\t}
\t\t\t}
\t\t\tMsg("impulse 101 extra weapons: equipped %u, owned %u, failed %u\\n",
\t\t\t\tgiven, already, failed);'''
for rel in SOURCES:
    p=ROOT/rel
    orig=p.read_bytes()
    crlf=b'\r\n' in orig
    src=orig.decode('latin-1').replace('\r\n','\n')
    if 'HL2MODERN_IMPULSE101_RESULT' not in src:
        assert src.count(needle)==1, (rel,src.count(needle))
        src=src.replace(needle,replacement,1)
        p.write_bytes((src.replace('\n','\r\n') if crlf else src).encode('latin-1'))
print('Instrumented experimental impulse 101 grants across HL2, CS and TF2 only.')
