"""Install private 2003 HEV pickup and physgun beam assets for the 2018 port.

Original model/mesh/texture data comes from gammasource_content. Only the MDL
name/material directory and the beam's shader wrapper change for compatibility.
"""
from pathlib import Path
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'gammasource_content'
OUT = ROOT / 'gammasource_content_2018'


def install():
    rows = []

    def write(rel, data, original=None):
        target = OUT / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        rows.append({'path': rel, 'source': str(original) if original else 'compatibility material',
                     'sha256': hashlib.sha256(data).hexdigest()})

    original = SOURCE / 'models/Items/hevsuit.mdl'
    data = bytearray(original.read_bytes())
    assert struct.unpack_from('<i', data, 4)[0] == 36
    name = b'items/gamma_hevsuit_2003.mdl'
    data[12:76] = name.ljust(64, b'\0')
    ncd, cdtable = struct.unpack_from('<ii', data, 216)
    assert ncd == 1
    cdoffset = struct.unpack_from('<i', data, cdtable)[0]
    end = data.index(0, cdoffset)
    assert bytes(data[cdoffset:end]).lower() == b'models/hevsuit//'
    private = b'models/gs_suit/'
    data[cdoffset:end + 1] = private.ljust(end + 1 - cdoffset, b'\0')
    write('models/items/gamma_hevsuit_2003.mdl', data, original)
    for suffix in ('.dx80.vtx', '.dx7_2bone.vtx'):
        original = SOURCE / ('models/Items/hevsuit' + suffix)
        write('models/items/gamma_hevsuit_2003' + suffix, original.read_bytes(), original)
    original = SOURCE / 'materials/Models/HEVSuit/hev_sheet.vtf'
    write('materials/models/gs_suit/hev_sheet.vtf', original.read_bytes(), original)
    write('materials/models/gs_suit/hev_sheet.vmt',
          b'"VertexLitGeneric"\n{\n "$basetexture" "models/gs_suit/hev_sheet"\n}\n')

    original = SOURCE / 'materials/sprites/physbeam.vtf'
    write('materials/sprites/gamma_physbeam.vtf', original.read_bytes(), original)
    # The old Sprite shader expects sprite-specific geometry. The quadratic
    # beam uses regular position/color/UV vertices in the 2018 engine.
    write('materials/sprites/gamma_physbeam.vmt', b'''"UnlitGeneric"
{
 "$basetexture" "sprites/gamma_physbeam"
 "$additive" "1"
 "$translucent" "1"
 "$vertexcolor" "1"
 "$vertexalpha" "1"
 "$nocull" "1"
 "$nofog" "1"
}
''')
    return rows


if __name__ == '__main__':
    print(json.dumps(install(), indent=2))
