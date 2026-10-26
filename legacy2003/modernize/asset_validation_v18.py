#!/usr/bin/env python3
"""Content-aware, offline dependency audit. No downloads, extraction or asset edits.

Repeated --assets-BRANCH directories are searched in caller-specified GAME order.
Never mix branches to satisfy missing dependencies. Local content is not packaged.
Without a supplied branch directory, references are unverified, not absent.
"""
from __future__ import annotations
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys

from weapon_asset_audit import kv_parse, tokens

ROOT = Path(__file__).resolve().parents[1]
BRANCHES = ('hl2', 'hl1', 'cstrike', 'tf2')
TEXTURE_KEYS = {'$basetexture', '$basetexture2', '$bumpmap', '$bumpmap2',
    '$normalmap', '$normalmap2', '$envmap', '$envmapmask', '$envmapmask2',
    '$detail', '$dudvmap', '$reflecttexture', '$refracttexture', '$flowmap',
    '$phongexponenttexture', '$selfillummask', '$blendmodulatetexture'}
RUNTIME_TEXTURES = {'env_cubemap', '_rt_fullframefb', '_rt_waterreflection',
    '_rt_waterrefraction', '_rt_camerarendertexture', '_rt_fullframedepth'}


def relative_asset(value, prefix='', suffix=''):
    if not isinstance(value, str):
        return None
    value = value.strip().replace('\\', '/')
    if not value or value.startswith('/') or any(c in value for c in ':?*#\x00'):
        return None
    if any(p in ('', '.', '..') for p in value.split('/')):
        return None
    if prefix and not value.casefold().startswith(prefix.casefold() + '/'):
        value = prefix + '/' + value
    if suffix and not value.casefold().endswith(suffix):
        value += suffix
    return value


class AssetIndex:
    """Index exact casing once, preserving ordered search roots and overrides."""
    def __init__(self, roots=()):
        self.roots = [Path(p).resolve() for p in roots]
        self.files = []
        for root in self.roots:
            index = collections.defaultdict(list)
            for folder in ('models', 'materials', 'scripts', 'sound', 'resource'):
                start = root / folder
                if not start.is_dir() or start.is_symlink():
                    continue
                for directory, dirs, names in os.walk(start, followlinks=False):
                    dirs[:] = sorted(d for d in dirs if not (Path(directory)/d).is_symlink())
                    for name in sorted(names):
                        file = Path(directory)/name
                        if not file.is_symlink():
                            rel = file.relative_to(root).as_posix()
                            index[rel.casefold()].append(rel)
            self.files.append(dict(index))
        self.used = set()

    def resolve(self, value, prefix='', suffix=''):
        rel = relative_asset(value, prefix, suffix)
        result = {'path': rel if rel else value}
        if rel is None:
            return dict(result, status='invalid_path')
        if not self.roots:
            return dict(result, status='unverified_no_asset_directory')
        for i, index in enumerate(self.files):
            matches = index.get(rel.casefold(), [])
            if not matches:
                continue
            actual = rel if rel in matches else matches[0]
            self.used.add((i, actual))
            result.update(status='verified_file' if actual == rel else 'case_mismatch',
                          root_index=i, actual_path=actual)
            if len(matches) > 1:
                result['case_collisions'] = matches
            return result
        return dict(result, status='missing')

    def disk_path(self, record):
        if 'root_index' not in record:
            return None
        return self.roots[record['root_index']] / record['actual_path']


def inspect_vtf(path):
    with path.open('rb') as stream:
        data = stream.read(96)
    size = path.stat().st_size
    if len(data) < 64 or data[:4] != b'VTF\0':
        return {'status': 'invalid_vtf_header'}
    major, minor, header = struct.unpack_from('<III', data, 4)
    width, height = struct.unpack_from('<HH', data, 16)
    flags = struct.unpack_from('<I', data, 20)[0]
    image_format = struct.unpack_from('<i', data, 52)[0]
    mip_count = data[56]
    result = {'version': [major, minor], 'dimensions': [width, height],
              'flags': flags, 'alpha': bool(flags & 0x3000),
              'cubemap': bool(flags & 0x4000), 'image_format': image_format,
              'mip_count': mip_count, 'payload_bytes': size - header}
    if major != 7 or minor > 5:
        result['status'] = 'unsupported_vtf_version'
    elif header < 64 or header >= size or not width or not height or not mip_count:
        result['status'] = 'invalid_vtf_dimensions_or_payload'
    else:
        # Header verification cannot establish visual appearance or full codec integrity.
        result['status'] = 'verified_header_unverified_pixels'
    return result


def walk_values(node):
    if isinstance(node, dict):
        for key, value in node.items():
            if isinstance(value, dict):
                yield from walk_values(value)
            else:
                yield key, value


def kv_pairs(text):
    """Parse KeyValues without losing repeated wave keys in rndwave blocks."""
    seq=list(tokens(text))
    pos=0
    def group(nested=False):
        nonlocal pos
        result=[]
        while pos<len(seq):
            key=seq[pos]; pos+=1
            if key=='}':
                if not nested:
                    raise ValueError('unexpected closing brace')
                return result
            if key=='{' or pos>=len(seq):
                raise ValueError('missing key/value')
            value=seq[pos]; pos+=1
            if value=='{':
                value=group(True)
            elif value=='}':
                raise ValueError('missing value')
            result.append((key.casefold(),value))
        if nested:
            raise ValueError('unclosed block')
        return result
    return group()


def walk_pairs(node):
    for key,value in node:
        if isinstance(value,list):
            yield from walk_pairs(value)
        else:
            yield key,value


def resolve_hud_spec(node):
    """Mirror the v18 HUD loader's conservative legacy resolution fallback."""
    if not isinstance(node,dict) or node.get('file') or 'font' in node:
        return node,None
    for resolution in ('640','320'):
        child=node.get(resolution)
        if isinstance(child,dict) and child.get('file'):
            chosen=dict(child)
            file=chosen['file']
            if isinstance(file,str) and '/' not in file.replace('\\','/') and ':' not in file:
                chosen['file']='sprites/'+file
            return chosen,resolution
    return node,None


class BranchAudit:
    def __init__(self, roots):
        self.index = AssetIndex(roots)
        self.materials = {}
        self.textures = {}
        self.models = {}
        self.sound_files = {}
        self.sound_events = collections.defaultdict(list)
        self.sound_script_errors = []
        self.sound_script_records = []
        self.sound_manifest=self.index.resolve('scripts/game_sounds_manifest.txt')
        self.sound_manifest_verified=False
        # The manifest is authoritative. Definitions in unregistered scripts are
        # only fallback evidence and never proof the runtime will load an event.
        sound_paths = set()
        manifest_path=self.index.disk_path(self.sound_manifest)
        if manifest_path:
            try:
                doc=kv_pairs(manifest_path.read_text(encoding='latin1'))
                sound_paths.update(value for key,value in walk_pairs(doc) if key=='precache_file' and isinstance(value,str))
                self.sound_manifest_verified=True
            except (OSError,ValueError) as exc:
                self.sound_manifest['content']={'status':'invalid_sound_manifest','error':str(exc)}
        else:
            for files in self.index.files:
                for paths in files.values():
                    for path in paths:
                        if path.casefold().startswith('scripts/') and ('game_sounds' in path.casefold() or path.casefold()=='scripts/sounds.txt') and path.endswith('.txt'):
                            sound_paths.add(path)
        for rel in sorted(sound_paths):
            record = self.index.resolve(rel)
            self.sound_script_records.append(record)
            path = self.index.disk_path(record)
            if path is None:
                continue
            try:
                parsed = kv_pairs(path.read_text(encoding='latin1'))
                for event, node in parsed:
                    if isinstance(node, list):
                        for key, wave in walk_pairs(node):
                            if key == 'wave' and isinstance(wave, str):
                                self.sound_events[event].append(wave.lstrip('*#@<>^)}$!?'))
            except (ValueError, OSError) as exc:
                self.sound_script_errors.append({'path': rel, 'status':'invalid_sound_script','error': str(exc)})

    def texture(self, value):
        if isinstance(value, str) and (value.casefold() in RUNTIME_TEXTURES or value.casefold().startswith('_rt_')):
            return {'path': value, 'status': 'runtime_generated_texture'}
        record = self.index.resolve(value, 'materials', '.vtf')
        key = str(record['path'])
        if key not in self.textures:
            path = self.index.disk_path(record)
            if path:
                record['content'] = inspect_vtf(path)
            self.textures[key] = record
        return self.textures[key]

    def material(self, value, stack=()):
        record = self.index.resolve(value, 'materials', '.vmt')
        key = str(record['path'])
        identity=key.casefold()
        if identity in stack:
            return {'path': record['path'], 'status': 'cyclic_material_include'}
        if key in self.materials:
            return self.materials[key]
        self.materials[key] = record
        path = self.index.disk_path(record)
        if path:
            try:
                text = path.read_text(encoding='latin1')
                record['shader'] = next(iter(tokens(text)), '')
                data = kv_parse(text)
                record['textures'] = []
                record['includes'] = []
                for name, raw in walk_values(data):
                    if name in TEXTURE_KEYS and isinstance(raw, str) and raw.strip():
                        dep = self.texture(raw)
                        record['textures'].append({'parameter': name, 'path': dep['path'], 'status': dep['status']})
                    if name == 'include' and isinstance(raw, str):
                        dep = self.material(raw, stack + (identity,))
                        record['includes'].append({'path': dep['path'], 'status': dep['status']})
                record['sampling_parameters'] = {k:v for k,v in walk_values(data)
                    if k in {'$basetexturetransform', '$bumptransform', '$alpha', '$alphatest', '$translucent', '$vertexalpha', '$nocull'}}
                record['visual_alignment_verified'] = False
            except (ValueError, OSError) as exc:
                record['content_error'] = str(exc)
                record['content']={'status':'invalid_vmt','error':str(exc)}
        return record

    def model(self, value):
        record = self.index.resolve(value, 'models', '.mdl')
        key = str(record['path'])
        if key in self.models:
            return self.models[key]
        self.models[key] = record
        path = self.index.disk_path(record)
        if path:
            from source_asset_formats import inspect_mdl
            record['content'] = inspect_mdl(path)
            content = record['content']
            stem = record['path'][:-4]
            # This engine's v35/36 embeds vertex data; do not require retail VVD.
            record['companions'] = [self.index.resolve(stem+ext) for ext in
                ('.dx7_2bone.vtx', '.dx80.vtx', '.dx7_3bone.vtx', '.dx90.vtx', '.vvd', '.phy', '.qc')]
            vtx_present=any('root_index' in d for d in record['companions'][:3])
            for companion in record['companions']:
                companion['required_individually']=False
                if companion['status']=='missing':
                    companion['status']='optional_absent'
            record['render_data_status']='variant_file_found_runtime_unverified' if vtx_present else 'unverified_missing_render_variant'
            record['companion_policy'] = 'Version-specific; supplied v35/36 does not require VVD; PHY/QC optional for rendering. VTX checksum/layout/runtime compatibility still requires engine tests.'
            record['materials'] = []
            for name in content.get('material_names', []):
                candidates = []
                for directory in content.get('material_directories', []) or ['']:
                    rel = '/'.join(x.strip('/') for x in (directory, name) if x.strip('/'))
                    dep = self.index.resolve(rel, 'materials', '.vmt')
                    candidates.append(dep)
                    if self.index.disk_path(dep):
                        found = self.material(rel)
                        record['materials'].append({'name': name, 'path': found['path'], 'status': found['status']})
                        break
                else:
                    record['materials'].append({'name': name, 'status': 'missing' if self.index.roots else 'unverified', 'searched': candidates})
        return record

    def sound(self, name):
        if name.casefold().endswith(('.wav', '.mp3')):
            waves = [name.lstrip('*#@<>^)}$!?')]
        else:
            waves = self.sound_events.get(name.casefold(), [])
        result = {'event': name, 'waves': []}
        if not self.index.roots:
            result['status'] = 'unverified_no_asset_directory'
        elif not waves:
            result['status'] = 'missing_sound_event'
        else:
            result['status'] = 'manifest_resolved_event' if self.sound_manifest_verified else 'definition_found_manifest_unverified'
            for wave in sorted(set(waves)):
                dep = self.index.resolve(wave, 'sound')
                self.sound_files[str(dep['path']).casefold()] = dep
                result['waves'].append(dep)
        return result

    def hud(self, name, node):
        result = {'name': name}
        node,resolution=resolve_hud_spec(node)
        if resolution:
            result['legacy_resolution_block']=resolution
        if not isinstance(node, dict):
            return dict(result, status='invalid_sprite_data')
        if 'font' in node:
            return dict(result, status='font_glyph_source_unsupported', font=node.get('font'), character=node.get('character'))
        if not isinstance(node.get('file'),str) or not node['file'] or len(node['file'])>=64 or len(name)>=64:
            return dict(result,status='invalid_hud_material_or_buffer')
        try:
            x, y, w, h = [int(node.get(k, 0)) for k in ('x','y','width','height')]
        except (ValueError, TypeError):
            return dict(result, status='invalid_rectangle')
        result['rectangle'] = [x,y,w,h]
        if min(x,y) < 0 or min(w,h) < 1 or max(x+w,y+h) > 8192:
            return dict(result, status='invalid_rectangle')
        mat = self.material(node.get('file', ''))
        result['material'] = mat['path']
        if self.index.disk_path(mat) is None:
            return dict(result, status=mat['status'])
        bases = [t for t in mat.get('textures', []) if t['parameter'] == '$basetexture']
        if not bases:
            return dict(result, status='no_direct_base_texture_patch_or_runtime_review')
        tex = self.textures[str(bases[0]['path'])]
        result['texture'] = tex['path']
        dims = tex.get('content', {}).get('dimensions')
        result['texture_dimensions'] = dims
        if tex.get('content', {}).get('status') != 'verified_header_unverified_pixels':
            return dict(result, status=tex.get('content', {}).get('status', tex['status']))
        result['status'] = 'verified_rectangle' if x+w <= dims[0] and y+h <= dims[1] else 'outside_texture_bounds'
        result['artwork_match_and_visual_scale_verified'] = False
        return result


def source_literals(path):
    if not path.is_file():
        return []
    text = path.read_text(encoding='latin1')
    # Remove comments before collecting quoted paths/events; comments aren't dependencies.
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    return re.findall(r'"([^"\r\n]+)"', text)


def audit_branch(branch, roots, inventory, npc_inventory, all_materials=False):
    audit = BranchAudit(roots)
    weapons = []
    build_file = ROOT/'modernize/weapon_build_report.json'
    build = json.loads(build_file.read_text())['branches'].get(branch, {}) if build_file.is_file() else {}
    builds = {r['classname']:r for r in build.get('weapons', [])}
    slots = collections.defaultdict(list)
    for entry in inventory:
        cls = entry['classname']
        record = {'classname': cls, 'branch': branch, 'source': entry['implementation_file'],
                  'build': builds.get(cls), 'runtime_verified': False,
                  'grant_exclusion_reason':entry.get('grant_exclusion_reason'),
                  'implementation_status':entry.get('implementation_status','source_present_runtime_unverified')}
        script = audit.index.resolve(f'scripts/{cls}.txt')
        record['script'] = script
        path = audit.index.disk_path(script)
        if path:
            try:
                data = kv_parse(path.read_text(encoding='latin1'))
                record['viewmodel'] = audit.model(data.get('viewmodel', ''))
                record['worldmodel'] = audit.model(data.get('playermodel', ''))
                record['ammo'] = {k:v for k,v in data.items() if k in ('primary_ammo', 'secondary_ammo', 'clip_size', 'clip2_size', 'default_clip', 'default_clip2')}
                record['slot'] = [data.get('bucket'), data.get('bucket_position')]
                slots[tuple(record['slot'])].append(cls)
                record['hud'] = [audit.hud(k,v) for k,v in data.get('texturedata', {}).items()] if isinstance(data.get('texturedata', {}),dict) else []
                for icon in record['hud']:
                    icon['source_script']=script['path']
                sounds = data.get('sounddata', {})
                record['sounds'] = [audit.sound(v) for k,v in walk_values(sounds) if isinstance(v,str)]
                record['status'] = 'inspected_content_runtime_unverified'
            except (ValueError, OSError) as exc:
                record.update(status='invalid_script', error=str(exc))
        else:
            record['status'] = script['status']
        # Projectile/effect models from the actual implementation must be checked too.
        record['implementation_models'] = [audit.model(x) for x in sorted(set(source_literals(ROOT/entry['implementation_file']))) if x.casefold().endswith('.mdl')]
        weapons.append(record)
    npcs = []
    if branch == 'hl2':
        for entry in npc_inventory:
            models = entry.get('model_references') or ([entry['model_reference']] if entry.get('model_reference') else [])
            models = [v if isinstance(v,str) else v.get('path','') for v in models]
            literals = source_literals(ROOT/entry['source'])
            record = {'classname':entry['name'], 'source':entry['source'],
                'source_registration':entry.get('registration_active'),
                'models':[audit.model(m) for m in models],
                'literal_sound_files':[audit.sound(x) for x in sorted(set(literals)) if x.casefold().endswith(('.wav','.mp3'))],
                'runtime_verified':False}
            npcs.append(record)
    players = []
    if branch == 'hl2':
        policy = (ROOT/'modernize/hl2_playermodel_policy.h').read_text()
        for label, model, portrait in re.findall(r'\{\s*"([^"]+)"\s*,\s*"(models/[^"]+)"\s*,\s*"([^"]+)"\s*\}', policy):
            dep = audit.model(model)
            content = dep.get('content', {})
            sequences = content.get('sequence_names', [])
            players.append({'label':label, 'model':dep, 'portrait':portrait,
                'idle_sequence_candidates':[s for s in sequences if 'idle' in s.casefold()],
                'preview_compatibility':'runtime_unverified' if audit.index.disk_path(dep) else dep['status'],
                'branch_policy':'HL2/HL2MP allowlist only; installed files filtered at runtime',
                'runtime_verified':False})
    # Materials referenced directly by restored implementations (sprites/effects included).
    sources = {e['implementation_file'] for e in inventory}
    if branch == 'hl2':
        sources.update(e['source'] for e in npc_inventory)
    effect_materials = []
    for source in sorted(sources):
        for literal in source_literals(ROOT/source):
            if literal.casefold().endswith(('.vmt','.vft')) or literal.startswith(('sprites/','effects/')):
                if not literal.casefold().endswith(('.mdl','.wav','.cpp','.h')):
                    effect_materials.append({'source':source, 'material':audit.material(literal)})
    if all_materials:
        material_paths=set()
        for files in audit.index.files:
            for names in files.values():
                material_paths.update(n for n in names if n.casefold().startswith('materials/') and n.casefold().endswith('.vmt'))
        for number,rel in enumerate(sorted(material_paths),1):
            audit.material(rel)
            if number%1000==0:
                print(f'{branch}: scanned {number}/{len(material_paths)} material definitions',flush=True)
    unused = []
    # Candidates only: maps, dynamic names, archives, and external mods are outside this closure.
    for i, files in enumerate(audit.index.files):
        for paths in files.values():
            for rel in paths:
                if (i,rel) not in audit.index.used and rel.casefold().endswith(('.mdl','.vmt','.vtf')):
                    unused.append({'root_index':i,'path':rel})
    registry = {b:set(x['classname'] for x in all_inventory.get(b,[])) for b in BRANCHES}
    cross = []
    unregistered_scripts = []
    for files in audit.index.files:
        for paths in files.values():
            for path in paths:
                candidate = Path(path).stem.casefold()
                if not path.casefold().startswith('scripts/weapon_') or not path.endswith('.txt'):
                    continue
                if candidate not in registry[branch]:
                    others = [b for b in BRANCHES if b != branch and candidate in registry[b]]
                    unregistered_scripts.append({'path':path,'classname':candidate,'registered_other_branches':others})
                    if others:
                        cross.append(candidate)
    return {'search_roots':[str(p) for p in audit.index.roots], 'weapons':weapons,
        'npcs':npcs, 'player_models':players, 'models':list(audit.models.values()),
        'materials':list(audit.materials.values()), 'textures':list(audit.textures.values()),
        'sound_files':list(audit.sound_files.values()), 'sound_script_errors':audit.sound_script_errors,
        'sound_manifest':audit.sound_manifest,'sound_scripts':audit.sound_script_records,
        'pickup_assets':{'hud_animations':audit.index.resolve('scripts/HudAnimations.txt'),
            'sound_events':[audit.sound('Player.PickupWeapon'),audit.sound('HL2Player.PickupWeapon')],
            'visual_animation_quality':'runtime_unverified'},
        'effect_materials':effect_materials,'all_material_definitions_scanned':all_materials,
        'slot_collisions':[{'slot':list(k),'weapons':v} for k,v in slots.items() if len(v)>1],
        'cross_branch_script_candidates':sorted(set(cross)),
        'cross_branch_note':'Script candidates outside branch registration only; shared names and intentional unused content require review, never auto-imported into grants.',
        'unregistered_weapon_scripts':unregistered_scripts,
        'unreferenced_candidates':unused,
        'unreferenced_note':'Outside this audited closure only; NOT safe-to-delete orphan classification. Maps/dynamic references/archives are not scanned.'}


all_inventory = {}


def audit_shader_sources():
    includes = []
    shader_files = []
    for folder in ('materialsystem/stdshaders', 'modernize/shaders'):
        for path in sorted((ROOT/folder).rglob('*')):
            if not path.is_file() or path.suffix.casefold() not in ('.hlsl','.glsl','.vsh','.psh','.fxc','.vert','.frag'):
                continue
            shader_files.append(path.relative_to(ROOT).as_posix())
            text = path.read_text(encoding='latin1')
            for inc in re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]', text, re.M):
                candidates = [path.parent/inc, ROOT/'materialsystem/stdshaders'/inc, ROOT/'public'/inc]
                found = next((p for p in candidates if p.is_file()),None)
                includes.append({'source':path.relative_to(ROOT).as_posix(), 'include':inc,
                    'status':'verified_source_file' if found else 'missing_or_generated_build_input',
                    'resolved':found.relative_to(ROOT).as_posix() if found else None})
    registrations = collections.defaultdict(list)
    for path in sorted((ROOT/'materialsystem/stdshaders').glob('*.cpp')):
        text=path.read_text(encoding='latin1')
        text=re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
        for shader in re.findall(r'(?:BEGIN_(?:VS_)?SHADER|DEFINE_FALLBACK_SHADER)\s*\(\s*([A-Za-z0-9_]+)',text):
            registrations[shader.casefold()].append(path.relative_to(ROOT).as_posix())
    return {'files':shader_files, 'includes':includes, 'shader_registrations':dict(registrations),
        'permutation_status':'Source and include audit only. Generated headers, compiler permutations, and GPU output require shader toolchains and renderer tests.',
        'backend_status':{'dx9':'existing backend, Windows/GPU unverified',
            'dx11':'experimental shader source; backend unavailable', 'dx12':'experimental shader source; backend unavailable',
            'vulkan':'experimental shader source; backend unavailable', 'opengl':'experimental shader source; backend unavailable'}}


def collect_problems(node, branch, context='', out=None, asset_path=None):
    if out is None:
        out=[]
    if isinstance(node, dict):
        asset_path=node.get('path') or node.get('source_script') or asset_path
        state=node.get('status','')
        if state in {'missing','case_mismatch','invalid_path','invalid_script','invalid_vtf_header',
                     'invalid_vtf_dimensions_or_payload','outside_texture_bounds','missing_sound_event',
                     'cyclic_material_include','unsupported_vtf_version','unsupported_version','invalid',
                     'unverified_no_asset_directory','invalid_vmt','invalid_sound_manifest','invalid_sound_script',
                     'font_glyph_source_unsupported','invalid_hud_material_or_buffer'}:
            out.append({'branch':branch,'context':context,'path':node.get('path') or node.get('event') or node.get('texture') or node.get('material') or asset_path,
                'status':state,'actual_path':node.get('actual_path'),
                'required_action':'Supply authorized branch-matching source content or correct reference after visual verification; geometry/rig/UV/animation cannot be reconstructed from a classname.' if state=='missing' else 'Inspect reported content/reference; no automatic lossy conversion or substitution.'})
        for k,v in node.items():
            if k != 'unreferenced_candidates':
                collect_problems(v,branch,context+'/'+k,out,asset_path)
    elif isinstance(node,list):
        for i,v in enumerate(node):
            collect_problems(v,branch,context+'/'+str(i),out,asset_path)
    return out


def write_markdown(report, out):
    lines=['# v18 content validation', '',
        'This report verifies source structure, exact path casing, parsed headers, and dependency references. It does not verify compilation, gameplay, UV alignment, animation quality, or visual artwork identity.', '',
        'The source archive contains no game MDL, VVD, VTX, PHY, QC, SMD, VMT, VTF, weapon scripts, or sounds. External local asset roots are read only and are not included in either v18 archive.', '']
    for branch,data in report['branches'].items():
        lines += [f'## {branch}', '',f"Search roots: {', '.join(data['search_roots']) or 'none (unverified)'}", '',
            f"Weapons: {len(data['weapons'])}; models inspected/referenced: {len(data['models'])}; materials: {len(data['materials'])}; textures: {len(data['textures'])}.", '',
            '| Classname | Script | Viewmodel | World model | HUD |', '|---|---|---|---|---|']
        for w in data['weapons']:
            fmt=lambda v: (str(v.get('path',''))+' ['+v.get('status','unverified')+']').replace('|','\\|')
            hud='; '.join(h['name']+': '+h['status'] for h in w.get('hud',[])) or 'unverified'
            lines.append('| '+' | '.join([w['classname'],fmt(w['script']),fmt(w.get('viewmodel',{})),fmt(w.get('worldmodel',{})),hud])+' |')
        lines += ['', 'Material assignments, dimensions, sequence names, companions, sounds, source build inclusion, and detailed dependency results are recorded in ASSET_VALIDATION_V18.json.', '']
    lines += ['## Outstanding content problems','',f"{len(report['problems'])} distinct contextual problems; see MISSING_ASSETS_V18.json for exact paths and reasons.", '',
        'The missing list is a dependency closure of the audited weapons, NPCs, models and effects, not a claim that every map or dynamically constructed engine resource was analyzed.', '']
    (out/'ASSET_VALIDATION_V18.md').write_text('\n'.join(lines),encoding='utf-8')
    missing=['# v18 missing and invalid assets', '',
        'No unavailable proprietary asset has been reconstructed without geometry, rig, animation or artwork source. Referenced local assets remain external. All runtime behavior is unverified.', '',
        '| Branch | Exact reference | State |', '|---|---|---|']
    unique=sorted(set((x['branch'],str(x['path']),x['status']) for x in report['problems']))
    for branch,path,status in unique:
        missing.append(f'| {branch} | `{path}` | {status} |')
    missing += ['', 'For absent branch roots (CS and TF2 in the supplied local installation), every dependency remains unverified. The source-only report lists exact script names for all registered weapons. Provide branch-matching, authorized game content and re-run the auditor. For models, original QC/SMD or equivalent rigged geometry and animation sources are needed to repair proportions, hands, UVs, attachment placement, and animation quality; class registrations are insufficient.', '']
    (out/'MISSING_ASSETS_V18.md').write_text('\n'.join(missing),encoding='utf-8')
    players=['# v18 multiplayer model assets','', '| Label | Model | File/content | Idle candidates | Preview |','|---|---|---|---|---|']
    for p in report['branches']['hl2']['player_models']:
        model=p['model']
        players.append('| '+' | '.join([p['label'],model['path'],model['status']+' / '+model.get('content',{}).get('status','not inspected'),', '.join(p['idle_sequence_candidates']) or 'none verified',p['preview_compatibility']])+' |')
    players += ['', 'Only the HL2/HL2MP policy is used. File availability does not establish player animation, collision, networking, or renderer compatibility. The new real StudioRender preview requires Windows compilation and visual testing.', '']
    (out/'MULTIPLAYER_MODEL_AUDIT_V18.md').write_text('\n'.join(players),encoding='utf-8')


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for branch in BRANCHES:
        parser.add_argument('--assets-'+branch,type=Path,action='append',default=[])
    parser.add_argument('--output-dir',type=Path,default=ROOT/'modernize/reports')
    parser.add_argument('--source-only',action='store_true',help='Write independent source-only JSON; no local roots inferred')
    parser.add_argument('--all-materials',action='store_true',help='Also inspect every unpacked material definition in the provided roots')
    parser.add_argument('--strict',action='store_true',help='Return nonzero for reported content problems')
    args=parser.parse_args(argv)
    for branch in BRANCHES:
        for root in getattr(args,'assets_'+branch):
            if not root.is_dir():
                parser.error(f'Asset root not found: {root}')
    global all_inventory
    inventory_document=json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text())
    all_inventory=inventory_document['branches']
    npc_inventory=json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
    report={'schema':1, 'runtime_verified':False, 'external_assets_packaged':False,
        'unregistered_prototypes':inventory_document.get('unregistered_prototypes',{}),
        'asset_state_definitions':{
            'verified_file':'Exact path exists; header/content has its own independent state. Visual/runtime correctness not established.',
            'repaired':'Source fix or exact-case reference patch; patch application is hash-checked separately.',
            'reconstructed':'No model/texture/animation artwork reconstructed: sufficient authorized authoring source was not supplied.',
            'missing':'Required referenced path absent from the explicitly provided branch roots.',
            'unverified':'Branch roots or supported metadata unavailable; never promoted to working content.',
            'optional_absent':'Alternative renderer variant or authoring/collision companion, not automatically a runtime fault.'},
        'scope':'Content/reference checks, not visual or gameplay certification. Unprovided branches are unverified.',
        'branches':{b:audit_branch(b,getattr(args,'assets_'+b),all_inventory[b],npc_inventory,args.all_materials) for b in BRANCHES},
        'shaders':audit_shader_sources()}
    for branch in report['branches'].values():
        for material in branch['materials']:
            shader=material.get('shader','').casefold()
            material['shader_source_candidates']=report['shaders']['shader_registrations'].get(shader,[])
            material['shader_source_status']='patch_inherits_shader' if shader=='patch' else ('source_registered_build_unverified' if material['shader_source_candidates'] else 'unverified_shader_registration')
    report['problems']=[]
    for branch,data in report['branches'].items():
        report['problems'] += collect_problems(data,branch)
    report['summary']={b:{'weapons':len(v['weapons']),'models':len(v['models']),'materials':len(v['materials']),
        'textures':len(v['textures']),'problems':sum(p['branch']==b for p in report['problems'])} for b,v in report['branches'].items()}
    args.output_dir.mkdir(parents=True,exist_ok=True)
    name='SOURCE_ONLY_ASSETS_V18.json' if args.source_only else 'ASSET_VALIDATION_V18.json'
    (args.output_dir/name).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if not args.source_only:
        (args.output_dir/'MISSING_ASSETS_V18.json').write_text(json.dumps(report['problems'],indent=2)+'\n',encoding='utf-8')
        write_markdown(report,args.output_dir)
    print(json.dumps(report['summary'],indent=2))
    return 1 if args.strict and report['problems'] else 0


if __name__=='__main__':
    sys.exit(main())
