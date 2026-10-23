#!/usr/bin/env python3
"""Generate elementary Hammer VMF test levels for registered HL2 prototype NPCs.

Each file places exactly one experimental NPC at *map creation* so the entity's
normal Precache/Spawn path can run. These maps do not supply missing assets,
sound scripts, schedules, or guarantee that Hammer/VBSP accepts the result.
"""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'modernize' / 'maps' / 'cut_npcs'
FACE_ORDER = ('x+', 'x-', 'y+', 'y-', 'z+', 'z-')


def face_vertices(bounds, axis):
    (lx, ly, lz), (hx, hy, hz) = bounds
    coords = {
        'x+': ((hx, ly, lz), (hx, hy, lz), (hx, hy, hz)),
        'x-': ((lx, hy, lz), (lx, ly, lz), (lx, ly, hz)),
        'y+': ((hx, hy, lz), (lx, hy, lz), (lx, hy, hz)),
        'y-': ((lx, ly, lz), (hx, ly, lz), (hx, ly, hz)),
        'z+': ((lx, ly, hz), (hx, ly, hz), (hx, hy, hz)),
        'z-': ((lx, hy, lz), (hx, hy, lz), (hx, ly, lz)),
    }
    return coords[axis]


def plane_to_str(vertices):
    return ' '.join('(' + ' '.join(str(t) for t in point) + ')' for point in vertices)


def normal(vertices):
    a, b, c = vertices
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    return (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])


def solid(solid_id, bounds, texture='TOOLS/TOOLSNODRAW', floor=False):
    lines = ['    solid', '    {', f'        "id" "{solid_id}"']
    for idx, axis in enumerate(FACE_ORDER):
        material = 'BRICK/BRICKFLOOR001A' if floor and axis == 'z+' else texture
        vertices = face_vertices(bounds, axis)
        lines.extend([
            '        side', '        {', f'            "id" "{solid_id * 10 + idx + 1}"',
            f'            "plane" "{plane_to_str(vertices)}"',
            f'            "material" "{material}"',
            '            "uaxis" "[1 0 0 0] 0.25"',
            '            "vaxis" "[0 -1 0 0] 0.25"',
            '            "rotation" "0"',
            '            "lightmapscale" "16"',
            '            "smoothing_groups" "0"',
            '        }',
        ])
    lines.extend(['    }'])
    return '\n'.join(lines)


def entity(ent_id, classname, origin, **options):
    def quote(s):
        return str(s).replace('\\', '\\\\').replace('"', '\\"')
    values = [('id', str(ent_id)), ('classname', classname),
              ('origin', ' '.join(str(c) for c in origin)), *options.items()]
    return ('entity\n{\n'
            + ''.join(f'    "{quote(key)}" "{quote(value)}"\n' for key, value in values)
            + '}')


def vmf(name):
    # Minimal sealed box with spawn, basic lighting and a single prototype.
    bounds = [
        ((-512, -512, -64), (512, 512, 0)),       # floor
        ((-512, -512, 256), (512, 512, 272)),     # ceiling
        ((-528, -528, -64), (-512, 528, 272)),    # west wall
        ((512, -528, -64), (528, 528, 272)),      # east wall
        ((-512, -528, -64), (512, -512, 272)),    # south wall
        ((-512, 512, -64), (512, 528, 272)),      # north wall
    ]
    world = '\n'.join(solid(i + 2, b, floor=(i == 0)) for i, b in enumerate(bounds))
    return f'''versioninfo
{{
    "editorversion" "400"
    "editorbuild" "0"
    "mapversion" "1"
    "formatversion" "100"
    "prefab" "0"
}}
visgroups
{{
}}
viewsettings
{{
    "bSnapToGrid" "1"
    "bShowGrid" "1"
    "nGridSpacing" "16"
    "bShow3DGrid" "0"
}}
world
{{
    "id" "1"
    "classname" "worldspawn"
    "skyname" "sky_day01_01"
{world}
}}
{entity(100, 'info_player_start', (-192, 0, 16), angles='0 0 0')}
{entity(101, 'light', (0, 0, 192), _light='255 255 255 600')}
{entity(102, name, (192, 0, 24), angles='0 180 0', targetname='prototype_test_npc')}
cameras
{{
    "activecamera" "-1"
}}
cordons
{{
    "active" "0"
}}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    inventory = json.loads((ROOT / 'modernize' / 'cut_npc_inventory.json').read_text())
    candidates = [entry for entry in inventory if entry['prototype_candidate']]
    for item in candidates:
        (OUT / (item['name'] + '_test.vmf')).write_text(vmf(item['name']), encoding='utf-8')
    readme = '''# Cut NPC disposable map sources

Hammer Editor VMF inputs, one prototype entity per map. Compile with your
working VBSP/VVIS/VRAD tools after configuring the proper HL2 game directory.
The maps are test inputs, NOT runnable BSPs. Some prototypes can crash during
spawn, have missing models/sounds, or lack AI. Do not load on live servers.

These maps attempt to force level-start precaching instead of enabling
`npc_allow_unprecached_create`; only the registered Debug HL2 candidates are
included. The disabled Conscript code is not represented. NPC entity models,
AI behavior and eventual multiplayer replication all require testing.

The generated VMFs use one default Source 2003-era brick floor material and a
simple nodraw room. Update the texture or skybox if your content differs.
'''
    (OUT / 'README.md').write_text(readme, encoding='utf-8')
    print(f'Generated {len(candidates)} NPC VMF maps in {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
