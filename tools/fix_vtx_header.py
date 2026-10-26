"""Repair VTX7 files written by the first converter run: the material replacement list
offset was stored in the numBodyParts field (28) instead of offset 24."""
import struct, sys
from pathlib import Path
fixed = 0
for root in sys.argv[1:]:
    for vtx in Path(root).rglob('*.vtx'):
        d = bytearray(vtx.read_bytes())
        if struct.unpack_from('<i', d, 0)[0] != 7 or struct.unpack_from('<i', d, 24)[0] != 0:
            continue
        mdl = Path(str(vtx).split('.')[0] + '.mdl') if False else vtx.with_name(vtx.name.split('.')[0] + '.mdl')
        if not mdl.exists():
            continue
        nbp = struct.unpack_from('<i', mdl.read_bytes(), 232)[0]
        mr = struct.unpack_from('<i', d, 28)[0]
        struct.pack_into('<ii', d, 24, mr, nbp)
        vtx.write_bytes(bytes(d)); fixed += 1
print('fixed', fixed)
