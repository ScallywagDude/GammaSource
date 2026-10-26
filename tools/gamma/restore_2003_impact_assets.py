#!/usr/bin/env python3
"""Restore MegaBuild's original blood and bullet-mark assets into a private namespace.

Copies original VTF bytes and preserves VMT parameters, rewriting only material
and texture dependencies to gamma_2003/. Existing shared assets are never edited.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRIVATE = 'gamma_2003/'
GROUPS = (
    'Impact.Concrete', 'Impact.Metal', 'Impact.Wood', 'Impact.Glass',
    'Impact.Flesh', 'Impact.BloodyFlesh', 'Impact.AlienFlesh',
    'Blood', 'YellowBlood', 'BigShot', 'BulletProof', 'GlassBreak',
    'ExplosiveGunshot',
)
BLOOD_MATERIALS = (
    'effects/blood', 'effects/blood2', 'effects/blood_gore',
    'effects/blood_drop', 'effects/blood_puff',
    'sprites/blood', 'sprites/bloodspray',
)
TEXTURE_KEYS = {
    '$basetexture', '$basetexture2', '$bumpmap', '$bumpmap2', '$normalmap',
    '$normalmap2', '$dudvmap', '$detail', '$envmapmask', '$selfillummask',
    '$phongexponenttexture', '$blendmodulatetexture', '$flowmap',
}
MATERIAL_KEYS = {'$modelmaterial', 'include'}
TOKEN = re.compile(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tokens(text):
    result = []
    for match in TOKEN.finditer(text):
        value = match.group()
        if value.startswith('//'):
            continue
        if value.startswith('"'):
            value = value[1:-1].replace('\\"', '"')
        result.append((value, match.start(), match.end()))
    return result


def parse(text):
    tok = tokens(text)

    def object_at(pos, nested=False):
        pairs = []
        while pos < len(tok):
            key = tok[pos][0]
            pos += 1
            if key == '}':
                if not nested:
                    raise ValueError('unexpected closing brace')
                return pairs, pos
            if pos >= len(tok):
                raise ValueError('missing value for ' + key)
            if tok[pos][0] == '{':
                value, pos = object_at(pos + 1, True)
            else:
                value = tok[pos][0]
                pos += 1
            pairs.append((key, value))
        if nested:
            raise ValueError('unclosed KeyValues object')
        return pairs, pos

    return object_at(0)[0]


def format_pairs(pairs, indent=0):
    result = []
    for key, value in pairs:
        if isinstance(value, list):
            result += ['\t' * indent + '"' + key + '"', '\t' * indent + '{',
                       format_pairs(value, indent + 1), '\t' * indent + '}']
        else:
            result.append('\t' * indent + '"' + key + '" "' + value + '"')
    return '\n'.join(result)


def normalize(name, suffix):
    name = name.replace('\\', '/').strip('/').lower()
    if name.startswith('materials/'):
        name = name[len('materials/'):]
    if name.endswith(suffix):
        name = name[:-len(suffix)]
    if '..' in name.split('/') or name.startswith(PRIVATE):
        raise ValueError('invalid original material path: ' + name)
    return name


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return
    # Replace atomically so an existing staging hard link is never edited.
    temp = path.with_name(path.name + '.gamma_tmp')
    temp.write_bytes(data)
    temp.replace(path)


def restore(reference, output, baseline, report):
    matroot = reference / 'materials'
    index = {p.relative_to(matroot).as_posix().lower(): p
             for p in matroot.rglob('*') if p.suffix.lower() in ('.vmt', '.vtf')}
    rows, visited = [], set()

    def copy_asset(name, suffix):
        name = normalize(name, suffix)
        key = name + suffix
        if key in visited:
            return PRIVATE + name
        if key not in index:
            raise FileNotFoundError('Original dependency is missing: materials/' + key)
        visited.add(key)
        source = index[key]
        data = source.read_bytes()
        deps = []
        row = {'source': str(source), 'source_relative': 'materials/' + key,
               'source_sha256': sha(data), 'source_size': len(data),
               'output_relative': 'materials/' + PRIVATE + key,
               'dependencies': deps}
        if suffix == '.vmt':
            text = data.decode('utf-8-sig', errors='strict')
            tok = tokens(text)
            edits = []
            for i, (value, _, _) in enumerate(tok[:-1]):
                kind = value.lower()
                if kind not in TEXTURE_KEYS | MATERIAL_KEYS:
                    continue
                dep, start, end = tok[i + 1]
                if dep in ('{', '}') or dep.startswith('$'):
                    continue
                dep_suffix = '.vmt' if kind in MATERIAL_KEYS else '.vtf'
                alias = copy_asset(dep, dep_suffix)
                deps.append({'parameter': kind, 'original': dep,
                             'private': alias, 'type': dep_suffix[1:]})
                edits.append((start, end, '"' + alias + '"'))
            for start, end, value in reversed(edits):
                text = text[:start] + value + text[end:]
            data = text.encode('utf-8')
            row['shader'] = tok[0][0]
            row['changes'] = 'dependency paths only; original shader and numeric parameters preserved'
        else:
            if data[:4] != b'VTF\0':
                raise ValueError('Invalid VTF header: ' + str(source))
            row['dimensions'] = list(struct.unpack_from('<HH', data, 16))
            row['frames'] = struct.unpack_from('<H', data, 24)[0]
            row['format'] = struct.unpack_from('<I', data, 52)[0]
            row['changes'] = 'none; texture bytes identical to source'
        row['output_sha256'] = sha(data)
        row['output_size'] = len(data)
        write(output / row['output_relative'], data)
        rows.append(row)
        return PRIVATE + name

    original_script = reference / 'scripts/decals.txt'
    old = dict(parse(original_script.read_text(encoding='utf-8-sig')))
    current = parse(baseline.read_text(encoding='utf-8-sig'))
    replacement = {}
    for group in GROUPS:
        replacement[group.lower()] = (group, [(copy_asset(material, '.vmt'), weight)
                                              for material, weight in old[group]])
    old_translation = dict(old['TranslationData'])
    merged = []
    for group, entries in current:
        if group.lower() in replacement:
            merged.append(replacement.pop(group.lower()))
        elif group.lower() == 'translationdata':
            translation = dict(entries)
            translation.update(old_translation)
            merged.append((group, list(translation.items())))
        else:
            merged.append((group, entries))
    merged.extend(replacement.values())
    for material in BLOOD_MATERIALS:
        copy_asset(material, '.vmt')
    script = ('// Generated by tools/gamma/restore_2003_impact_assets.py.\n'
              '// Original MegaBuild bullet/blood groups; all other 2018 groups retained.\n'
              + format_pairs(merged) + '\n').encode('utf-8')
    write(output / 'scripts/gamma_2003_decals.txt', script)
    manifest = {
        'schema': 1, 'private_namespace': PRIVATE, 'reference': str(reference),
        'original_decal_script': {'path': str(original_script), 'sha256': sha(original_script.read_bytes())},
        'baseline_2018_script': {'path': str(baseline), 'sha256': sha(baseline.read_bytes())},
        'restored_groups': list(GROUPS), 'blood_material_roots': list(BLOOD_MATERIALS),
        'script': {'path': 'scripts/gamma_2003_decals.txt', 'sha256': sha(script),
                   'preserved_2018_groups': [n for n, _ in current if n not in GROUPS]},
        'files': sorted(rows, key=lambda row: row['output_relative']),
        'runtime_validation': 'pending; generation and provenance checks only',
    }
    write(report, (json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
    return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--reference', type=Path, default=ROOT.parent / 'hl2')
    ap.add_argument('--out', type=Path, default=ROOT / 'gammasource_content_2018')
    ap.add_argument('--baseline', type=Path,
                    default=Path(__file__).parent / 'reference/decals_subrect_2018.txt')
    ap.add_argument('--report', type=Path,
                    default=ROOT / 'verification/impact_restoration_2026-10-26/provenance_manifest.json')
    args = ap.parse_args()
    result = restore(args.reference, args.out, args.baseline, args.report)
    print('Restored %d private assets and %d decal groups; runtime QA pending.' %
          (len(result['files']), len(result['restored_groups'])))


if __name__ == '__main__':
    main()
