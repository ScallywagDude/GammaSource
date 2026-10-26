"""Measure authored 2003 four-corner poses against 2018 triangle blending.

Reads original MDLs only. Uses the existing validated integer-frame decoder;
the selected Alyx aim grid has one frame per corner, so no temporal resampling
or converted-channel quantization is involved.
"""
import argparse
import collections
import importlib.util
import json
import math
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    'gamma_sample_audit', Path(__file__).with_name('audit_legacy_animation_samples.py'))
SAMPLES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SAMPLES)


def blend(a, b, t):
    p, q = a
    r, s = b
    if sum(x * y for x, y in zip(q, s)) < 0:
        s = tuple(-x for x in s)
    return (tuple(x * (1 - t) + y * t for x, y in zip(p, r)),
            SAMPLES.qnorm(tuple(x * (1 - t) + y * t for x, y in zip(q, s))))


def triangle(ix, iy, x, y):
    if (ix + iy) % 2 == 0:
        if x > y:
            corners, weights = ((0, 0), (1, 0), (1, 1)), [1 - x, x - y]
        else:
            corners, weights = ((1, 1), (0, 1), (0, 0)), [x, y - x]
    elif x + y > 1:
        corners, weights = ((1, 0), (1, 1), (0, 1)), [1 - y, x - 1 + y]
    else:
        corners, weights = ((0, 1), (0, 0), (1, 0)), [y, 1 - x - y]
    if weights[1] < 0.001:
        weights[1] = 0
    return corners, weights + [1 - sum(weights)]


def audit(path):
    d, version = SAMPLES.normalize_original(path.read_bytes())
    i = lambda offset: SAMPLES.i(d, offset)
    f = lambda offset: SAMPLES.f(d, offset)
    nb, ns = i(156), i(188)
    rows, inventory = [], []
    max_angle = max_pos = 0.0
    grids = collections.Counter()
    non_delta_fractional = []
    flags = collections.Counter()
    for sequence in range(ns):
        o = i(192) + 4284 * sequence
        gx, gy = i(o + 4156), i(o + 4160)
        flags[i(o + 8)] += 1
        grids[(gx, gy)] += 1
        label = SAMPLES.name(d, o + i(o))
        weight_index = i(o + 4248)
        weights = [f(o + weight_index + 4 * b) if weight_index else 1 for b in range(nb)]
        if not i(o + 8) & 4 and any(0 < w < 1 for w in weights):
            non_delta_fractional.append(label)
        if gx < 2 or gy < 2:
            continue
        animations = [[i(o + 56 + 4 * (x * 32 + y)) for y in range(gy)] for x in range(gx)]
        inventory.append({'sequence': sequence, 'label': label, 'grid': [gx, gy],
                          'corner_frames': sorted({i(i(184) + 92 * a + 12)
                                                   for col in animations for a in col})})
        for ix in range(gx - 1):
            for iy in range(gy - 1):
                for bone in range(nb):
                    if weights[bone] <= 0:
                        continue
                    bo = i(160) + 196 * bone
                    corners = {}
                    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
                        ao = i(184) + 92 * animations[ix + x][iy + y]
                        corners[(x, y)] = SAMPLES.old_sample(d, bo, ao + i(ao + 48) + 32 * bone, 0)
                    for x in (0.1, 0.25, 0.5, 0.75, 0.9):
                        for y in (0.1, 0.25, 0.5, 0.75, 0.9):
                            old = blend(blend(corners[0, 0], corners[1, 0], x),
                                        blend(corners[0, 1], corners[1, 1], x), y)
                            tri, w = triangle(ix, iy, x, y)
                            if w[1] < 0.001:
                                new = blend(corners[tri[0]], corners[tri[2]], w[2] / (w[0] + w[2]))
                            else:
                                new = blend(blend(corners[tri[0]], corners[tri[1]], w[1] / (w[0] + w[1])),
                                            corners[tri[2]], w[2])
                            dot = min(1, abs(sum(a * b for a, b in zip(old[1], new[1]))))
                            angle = math.degrees(2 * math.acos(dot))
                            distance = math.sqrt(sum((a - b) ** 2 for a, b in zip(old[0], new[0])))
                            max_angle, max_pos = max(max_angle, angle), max(max_pos, distance)
                            if angle > 0.01 or distance > 0.001:
                                rows.append({'sequence': sequence, 'label': label, 'bone': bone,
                                             'bone_name': SAMPLES.name(d, bo + i(bo)), 'bone_weight': weights[bone],
                                             'cell': [ix, iy], 'local_blend': [x, y],
                                             'rotation_degrees': angle, 'position_units': distance})
    rows.sort(key=lambda row: row['rotation_degrees'], reverse=True)
    return {'original': str(path), 'version': version, 'sequences': ns,
            'grid_inventory': [{'grid': list(grid), 'count': count} for grid, count in grids.items()],
            'sequence_flags': {hex(flag): count for flag, count in flags.items()},
            'non_delta_fractional_weight_sequences': non_delta_fractional,
            'two_dimensional_sequences': inventory, 'changed_pose_samples': len(rows),
            'max_local_rotation_difference_degrees': max_angle,
            'max_local_position_difference_units': max_pos, 'worst': rows[:10],
            'scope': 'Local authored sequence poses at frame zero, before outer layer weighting; not a runtime/root-motion test.'}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('models', type=Path, nargs='+')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    result = {'models': [audit(path) for path in args.models]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2))
    for row in result['models']:
        print(Path(row['original']).name, 'changed samples', row['changed_pose_samples'],
              'max degrees', row['max_local_rotation_difference_degrees'],
              'max position', row['max_local_position_difference_units'])
