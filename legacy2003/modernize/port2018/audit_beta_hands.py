"""Audit the live weapon roster against the actual 2003 model/material content.

This inspects source MDLs, not converted cache versions, and records complete paths
and hashes. Retail weapon bodies are allowed only with hidden arms and a beta donor.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import os
RETAIL_ROOT = os.environ.get('GAMMA_RETAIL_ROOT', r'C:\GammaSource\retail')  # copies of the retail game folders

from vpkshaders import entries, read_entry

REPO = Path(__file__).resolve().parents[2]
OVERLAY = REPO / 'gammasource_overlay'
BETA = REPO / 'gammasource_content'
BUILD_BETA = Path(r'C:\Users\doge6\Videos\GammaSource_Build\gammasource_content')
ASSETS = Path(r'C:\Users\doge6\Downloads\source2018_assets')
ROOTS = [OVERLAY, ASSETS / 'gamma', BETA, Path(r'C:\Users\doge6\Documents\HL2BetaMegaCuck\hl2_2003'), ASSETS]
HAND_NAMES = {'hand', 'hand2', 'finger', 'handplate', 'thumb', 'arm', 'inner', 'joint', 'bicep', 'v_hand_sheet', 'v_hands', 'forearm', 'rightarm'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def mdl(data):
    integer = lambda o: struct.unpack_from('<i', data, o)[0]
    string = lambda o: data[o:data.index(b'\0', o)].decode('latin1')
    version = integer(4)
    if version in (36, 37):
        count, table, stride, directories, directory_table = integer(208), integer(212), 32, integer(216), integer(220)
    else:
        count, table, stride, directories, directory_table = integer(204), integer(208), 64, integer(212), integer(216)
    return {'version': version, 'bones': integer(156),
            'textures': [string(table + stride * i + integer(table + stride * i)) for i in range(count)],
            'directories': [string(integer(directory_table + 4 * i)).replace('\\', '/').strip('/') for i in range(directories)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roster-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    roster = dict(re.findall(r'GAMMA_WEAPON name=(\w+).*? view=(\S+)', args.roster_log.read_text(errors='replace')))
    rules = (OVERLAY / 'scripts/gamma_betahands.txt').read_text()
    donors = dict(re.findall(r'"(models/[^\"]+\.mdl)"\s*\{\s*"donor"\s*"([^\"]+)"', rules))
    # The rules containing comments before donor are parsed by each complete block.
    for alias, block in re.findall(r'"(models/[^\"]+\.mdl)"\s*\{(.*?)(?=\n\t\}|\Z)', rules, re.S):
        match = re.search(r'"donor"\s*"([^\"]+)"', block)
        if match:
            donors[alias.lower()] = match.group(1)
    vpk_paths = [Path(os.path.join(RETAIL_ROOT, r'Half-Life 2\hl2\hl2_misc_dir.vpk')),
                 Path(os.path.join(RETAIL_ROOT, r'Portal\portal\portal_pak_dir.vpk'))]
    wanted = {model.lower() for model in roster.values()}
    packed = {}
    for vpk in vpk_paths:
        for name, *entry in entries(str(vpk)):
            if name.lower() in wanted and name.lower() not in packed:
                packed[name.lower()] = (read_entry(str(vpk), *entry), f'{vpk}::{name}')

    def resolve(path):
        path = re.sub('/+', '/', path.replace('\\', '/'))
        for root in ROOTS:
            candidate = root / path
            if candidate.is_file():
                return candidate.read_bytes(), str(candidate)
        return packed.get(path.lower(), (None, None))

    failures, records = [], []
    for weapon, previous_model in sorted(roster.items()):
        script = OVERLAY / 'scripts' / f'{weapon}.txt'
        match = re.search(r'"viewmodel"\s*"([^\"]+)"', script.read_text())
        model = match.group(1) if match else previous_model
        data, source = resolve(model)
        row = {'weapon': weapon, 'viewmodel': model, 'source': source}
        if data is None:
            failures.append(f'{weapon}: missing {model}')
            records.append(row)
            continue
        row.update(mdl(data))
        row['sha256'] = digest(data)
        beta_model = BUILD_BETA / model
        row['reference_2003'] = str(beta_model) if beta_model.is_file() else None
        row['matches_2003_model'] = beta_model.is_file() and beta_model.read_bytes() == data
        row['donor'] = donors.get(model.lower())
        row['hand_materials'] = []
        for texture in row['textures']:
            if texture.lower() not in HAND_NAMES and texture.lower() != 'gamma_hidden_hands':
                continue
            material, material_source, relative = None, None, None
            for directory in row['directories']:
                relative = re.sub('/+', '/', f'materials/{directory}/{texture}.vmt')
                material, material_source = resolve(relative)
                if material is not None:
                    break
            record = {'name': texture, 'path': relative, 'source': material_source}
            if material is not None:
                reference = BUILD_BETA / relative
                record['sha256'] = digest(material)
                record['matches_2003'] = reference.is_file() and reference.read_bytes() == material
                record['hidden'] = bool(re.search(r'"\$no_draw"\s*"1"', material.decode('latin1')))
                base = re.search(r'"\$basetexture"\s*"([^\"]+)"', material.decode('latin1'), re.I)
                if base:
                    texture_path = 'materials/' + base.group(1).replace('\\', '/') + '.vtf'
                    texture_data, texture_source = resolve(texture_path)
                    reference_texture = BUILD_BETA / texture_path
                    record['texture_source'] = texture_source
                    record['texture_matches_2003'] = texture_data is not None and reference_texture.is_file() and reference_texture.read_bytes() == texture_data
            else:
                failures.append(f'{weapon}: missing hand material {texture}')
            row['hand_materials'].append(record)
        if row['donor']:
            hidden = [material for material in row['hand_materials'] if material['name'] == 'gamma_hidden_hands']
            if not hidden or not all(material.get('hidden') for material in hidden):
                failures.append(f'{weapon}: retail arms are not hidden')
            row['classification'] = 'retail weapon body with mandatory 2003 donor arms'
        elif row['hand_materials']:
            row['classification'] = 'native 2003 arms'
            if not row['matches_2003_model']:
                failures.append(f'{weapon}: native hand model differs from actual 2003 build')
            for material in row['hand_materials']:
                if not material.get('matches_2003') or not material.get('texture_matches_2003'):
                    failures.append(f"{weapon}: hand material/texture differs from 2003: {material['name']}")
        else:
            row['classification'] = 'original utility or weapon with no arm mesh'
            if row['version'] >= 44 and any('hand' in name.lower() for name in row['textures']):
                failures.append(f'{weapon}: uncovered retail hand mesh')
        records.append(row)

    donor_records = []
    for donor, original in [('v_betahands_pistol', 'v_pistol'), ('v_betahands_ar2', 'v_ar2')]:
        path = OVERLAY / f'models/weapons/{donor}.mdl'
        reference = BUILD_BETA / f'models/weapons/{original}.mdl'
        data, beta = path.read_bytes(), reference.read_bytes()
        differences = [i for i, (left, right) in enumerate(zip(data, beta)) if left != right]
        row = {'donor': str(path), 'reference_2003': str(reference), 'version': mdl(data)['version'],
               'sha256': digest(data), 'reference_sha256': digest(beta), 'equal_length': len(data) == len(beta),
               'changed_byte_offsets': differences, 'note': 'One material-directory character changes; geometry, rig, animation and VTX data remain original.'}
        if len(data) != len(beta) or len(differences) != 1:
            failures.append(f'{donor}: unexpected changes beyond material-directory alias')
        row['vtx_checks'] = []
        for suffix in ('dx80.vtx', 'dx7_2bone.vtx'):
            a = path.with_suffix('.' + suffix)
            b = reference.with_suffix('.' + suffix)
            matches = a.read_bytes() == b.read_bytes()
            row['vtx_checks'].append({'suffix': suffix, 'matches_2003': matches})
            if not matches:
                failures.append(f'{donor}: VTX geometry differs from 2003: {suffix}')
        donor_records.append(row)
    library = []
    relative = Path('materials/models/weapons/v_hand')
    for source in sorted((BUILD_BETA / relative).iterdir()):
        if source.suffix.lower() not in ('.vmt', '.vtf'):
            continue
        destination = OVERLAY / relative / source.name
        matches = destination.is_file() and source.read_bytes() == destination.read_bytes()
        library.append({'file': source.name, 'reference_2003': str(source), 'active_overlay': str(destination),
                        'sha256': digest(source.read_bytes()), 'matches_2003': matches})
        if not matches:
            failures.append(f'2003 hand library differs or is missing: {source.name}')
    result = {'roster_log': str(args.roster_log), 'weapons': len(records), 'reference_content': str(BUILD_BETA),
              'mandatory': 'cl_betahands is clamped to 1; legacy archived zero is rejected by its min/max.',
              'failures': failures, 'hand_material_texture_library': library, 'donors': donor_records, 'models': records}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2))
    lines = [f'WEAPONS {len(records)}', f'FAILURES {len(failures)}', f'REFERENCE {BUILD_BETA}']
    for row in records:
        lines.append(f"{row['weapon']} | {row['viewmodel']} | v{row.get('version')} | {row.get('classification')} | {row['source']}")
    lines += failures
    args.output.with_suffix('.log').write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines[:3]))
    for failure in failures:
        print(failure)
    raise SystemExit(1 if failures else 0)


if __name__ == '__main__':
    main()
