"""Give retail weapons unique model paths and isolate their hidden hand materials.

Beta IRifle and retail AR2 use the same v_irifle name but different animation sets.
Material overrides must also stay local to these copies: globally hiding v_hand_sheet
removes hands from unrelated retail weapons. Run after mounting local HL2 and Portal.
"""
from pathlib import Path
import argparse
import struct
import os
RETAIL_ROOT = os.environ.get('GAMMA_RETAIL_ROOT', r'C:\GammaSource\retail')  # copies of the retail game folders

from vpkshaders import entries, read_entry


def extract(vpk, names):
    found = {}
    for name, *entry in entries(str(vpk)):
        if name.lower() in names:
            found[name.lower()] = read_entry(str(vpk), *entry)
    missing = names - found.keys()
    if missing:
        raise ValueError(f"Missing in {vpk}: {sorted(missing)}")
    return found


def alias_model(data, alias, hide_hands=False):
    mdl = bytearray(data)
    integer = lambda offset: struct.unpack_from('<i', mdl, offset)[0]
    string = lambda offset: bytes(mdl[offset:mdl.index(0, offset)]).decode('latin1')
    if mdl[:4] != b'IDST' or integer(4) not in range(44, 50):
        raise ValueError('Expected retail MDL v44-v49')
    name = f'weapons/{alias}.mdl'.encode('ascii')
    mdl[12:76] = name.ljust(64, b'\0')
    hidden_dirs = []
    if hide_hands:
        count, index = integer(204), integer(208)
        for i in range(count):
            texture = index + i * 64
            if string(texture + integer(texture)).lower() in ('v_hand_sheet', 'v_hands'):
                struct.pack_into('<i', mdl, texture, len(mdl) - texture)
                mdl += b'gamma_hidden_hands\0'
        for i in range(integer(212)):
            path = string(integer(integer(216) + i * 4)).replace('\\', '/')
            hidden_dirs.append(path)
    struct.pack_into('<i', mdl, 76, len(mdl))
    return bytes(mdl), hidden_dirs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hl2', type=Path, default=Path(os.path.join(RETAIL_ROOT, r'Half-Life 2\hl2\hl2_misc_dir.vpk')))
    parser.add_argument('--portal', type=Path, default=Path(os.path.join(RETAIL_ROOT, r'Portal\portal\portal_pak_dir.vpk')))
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / 'gammasource_overlay')
    parser.add_argument('--beta-content', type=Path, default=Path(__file__).resolve().parents[2] / 'gammasource_content')
    args = parser.parse_args()
    sources = [(args.hl2, 'v_357', 'v_gamma_357', True),
               (args.hl2, 'v_irifle', 'v_gamma_ar2future', True),
               (args.hl2, 'w_irifle', 'w_gamma_ar2future', False),
               (args.hl2, 'v_crossbow', 'v_gamma_crossbow', True),
               (args.portal, 'v_portalgun', 'v_gamma_portalgun', True)]
    nodraw = '// Private hand material for GammaSource retail viewmodel aliases.\n"UnlitGeneric"\n{\n\t"$no_draw" "1"\n\t"$basetexture" "tools/toolsnodraw"\n}\n'
    for vpk, original, alias, hide in sources:
        names = {f'models/weapons/{original}.{extension}' for extension in ('mdl', 'vvd', 'dx90.vtx')}
        found = extract(vpk, names)
        for name, data in found.items():
            extension = name.split(original + '.', 1)[1]
            if extension == 'mdl':
                data, paths = alias_model(data, alias, hide)
                for path in paths:
                    material = args.output / 'materials' / path / 'gamma_hidden_hands.vmt'
                    material.parent.mkdir(parents=True, exist_ok=True)
                    material.write_text(nodraw, encoding='ascii')
            destination = args.output / f'models/weapons/{alias}.{extension}'
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        print(f'{original} -> {alias}')
    # The 2003 hand materials/textures are authoritative, including the hand-sheet
    # used by native beta Alyx/cguard/immolator models. Never restore retail Phong.
    hand_folder = Path('materials/models/weapons/v_hand')
    for source in (args.beta_content / hand_folder).iterdir():
        if source.is_file() and source.suffix.lower() in ('.vmt', '.vtf'):
            destination = args.output / hand_folder / source.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(source.read_bytes())


if __name__ == '__main__':
    main()
