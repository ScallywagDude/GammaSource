"""Audit original procedural helper payloads and exercise their authored control poses.

When --converted is supplied, reads actual runtime cache bytes and compares all
procedure fields and corrected transforms on the same source control animation.
Raw animation quantization has its own independent animation-sample audit.
"""
import argparse
import json
import math
import struct
from pathlib import Path

from audit_legacy_animation_samples import i, f, name, old_sample, normalize_original, qnorm


def vec(data, offset, count):
    return struct.unpack_from('<' + 'f' * count, data, offset)


def qmul(a, b):
    x, y, z, w = a
    X, Y, Z, W = b
    return (w * X + x * W + y * Z - z * Y,
            w * Y - x * Z + y * W + z * X,
            w * Z + x * Y - y * X + z * W,
            w * W - x * X - y * Y - z * Z)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def rotate(q, p):
    t = tuple(2 * x for x in cross(q[:3], p))
    u = cross(q[:3], t)
    return tuple(p[k] + q[3] * t[k] + u[k] for k in range(3))


def compose(parent, local):
    p, q = parent
    v, r = local
    t = rotate(q, v)
    return tuple(p[k] + t[k] for k in range(3)), qnorm(qmul(q, r))


def slerp(a, b, amount):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b = tuple(-x for x in b)
        dot = -dot
    dot = min(1, dot)
    if dot > .9995:
        return qnorm(tuple(x * (1 - amount) + y * amount for x, y in zip(a, b)))
    angle = math.acos(dot)
    left, right = math.sin((1 - amount) * angle) / math.sin(angle), math.sin(amount * angle) / math.sin(angle)
    return qnorm(tuple(x * left + y * right for x, y in zip(a, b)))


def procedures(data, modern=False):
    result = {}
    stride, type_field, proc_field = (216, 164, 168) if modern else (196, 148, 152)
    for bone in range(i(data, 156)):
        at = i(data, 160) + stride * bone
        kind = i(data, at + type_field)
        if not kind:
            continue
        proc = at + i(data, at + proc_field)
        control = i(data, proc)
        row = {'bone': bone, 'type': kind, 'control': control,
               'parent': i(data, at + 4), 'name': name(data, at + i(data, at)),
               'flags': i(data, at + (160 if modern else 144))}
        if kind == 2:
            count = i(data, proc + 4)
            assert 1 <= count <= 32
            start = proc + i(data, proc + 8)
            row['payload'] = data[start:start + 48 * count]
            assert len(row['payload']) == 48 * count
            row['triggers'] = [(f(data, start + 48 * k), vec(data, start + 48 * k + 4, 4),
                                vec(data, start + 48 * k + 20, 3), vec(data, start + 48 * k + 32, 4))
                               for k in range(count)]
        elif kind == 1:
            row['axis'] = i(data, proc + 4)
            row['payload'] = data[proc:proc + 176]
            row['positions'] = [vec(data, proc + 8 + 12 * k, 3) for k in range(6)]
            row['quaternions'] = [vec(data, proc + 80 + 16 * k, 4) for k in range(6)]
        else:
            raise ValueError('Unsupported original procedure %d' % kind)
        assert 0 <= control < i(data, 156)
        result[bone] = row
    return result


def solve(proc, control, diagnostics=None):
    if proc['type'] == 2:
        weights = [max(0, 1 - 2 * math.acos(min(1, abs(sum(x * y for x, y in zip(trigger, control))))) * tolerance)
                   for tolerance, trigger, _, _ in proc['triggers']]
        scale = sum(weights)
        if scale <= .001:
            return proc['triggers'][0][2], qnorm(proc['triggers'][0][3])
        q = (0, 0, 0, 0)
        p = (0, 0, 0)
        for weight, (_, _, position, quaternion) in zip(weights, proc['triggers']):
            if not weight:
                continue
            if sum(x * y for x, y in zip(quaternion, q)) < 0:
                q = tuple(-x for x in q)
            q = tuple(x + weight / scale * y for x, y in zip(q, quaternion))
            p = tuple(x + weight / scale * y for x, y in zip(p, position))
        if diagnostics is not None:
            length = math.sqrt(sum(x * x for x in q))
            diagnostics['blended_quaternions'] += 1
            diagnostics['nonunit_before_normalization'] += abs(length - 1) > .0001
            diagnostics['min_length_before_normalization'] = min(diagnostics['min_length_before_normalization'], length)
            diagnostics['max_length_before_normalization'] = max(diagnostics['max_length_before_normalization'], length)
            x, y, z, w = q
            columns = [(1 - 2*y*y - 2*z*z, 2*x*y + 2*w*z, 2*x*z - 2*w*y),
                       (2*x*y - 2*w*z, 1 - 2*x*x - 2*z*z, 2*y*z + 2*w*x),
                       (2*x*z + 2*w*y, 2*y*z - 2*w*x, 1 - 2*x*x - 2*y*y)]
            diagnostics['max_basis_length_error_without_normalization'] = max(
                diagnostics['max_basis_length_error_without_normalization'],
                max(abs(math.sqrt(sum(v*v for v in column)) - 1) for column in columns))
        return p, qnorm(q)
    axis = [0, 0, 0]
    axis[proc['axis']] = 1
    direction = rotate(control, axis)
    chosen = [2 * k + (direction[k] < 0) for k in range(3)]
    a, b, c = [abs(x) for x in direction]
    if a + b <= 0:
        return proc['positions'][chosen[2]], qnorm(proc['quaternions'][chosen[2]])
    total = a + b + c
    q = slerp(proc['quaternions'][chosen[1]], proc['quaternions'][chosen[0]], a / (a + b))
    q = slerp(q, proc['quaternions'][chosen[2]], c / total)
    p = tuple(sum(proc['positions'][chosen[k]][j] * abs(direction[k]) / total for k in range(3))
              for j in range(3))
    return p, q


def error(a, b):
    position = math.sqrt(sum((x - y) ** 2 for x, y in zip(a[0], b[0])))
    dot = min(1, abs(sum(x * y for x, y in zip(a[1], b[1]))))
    return position, 2 * math.degrees(math.acos(dot))


def audit(original_path, converted_path=None):
    data, version = normalize_original(original_path.read_bytes())
    procs = procedures(data)
    modern = procedures(converted_path.read_bytes(), True) if converted_path else None
    mismatches = []
    if modern is not None:
        if procs.keys() != modern.keys():
            mismatches.append('procedure bone set mismatch')
        for bone, proc in procs.items():
            other = modern.get(bone)
            if other is None:
                continue
            for field in ('type', 'control', 'parent', 'payload'):
                if proc[field] != other[field]:
                    mismatches.append([bone, field])
            if proc['flags'] & 4 != other['flags'] & 4:
                mismatches.append([bone, 'BONE_ALWAYS_PROCEDURAL'])
    controls, samples = 0, 0
    max_missing_position = max_missing_angle = max_compare_position = max_compare_angle = 0.0
    worst = []
    normalization = {'blended_quaternions': 0, 'nonunit_before_normalization': 0,
                     'min_length_before_normalization': 1., 'max_length_before_normalization': 0.,
                     'max_basis_length_error_without_normalization': 0.}
    nb = i(data, 156)
    parents = [i(data, i(data, 160) + 196 * b + 4) for b in range(nb)]
    for animation in range(i(data, 180)):
        at = i(data, 184) + 92 * animation
        if i(data, at + 8) & 4:
            continue  # delta poses need a base; the raw-channel audit covers their data
        frames = i(data, at + 12)
        if frames < 1:
            continue
        for frame in sorted({0, (frames - 1) // 4, (frames - 1) // 2, 3 * (frames - 1) // 4, frames - 1}):
            locals_fixed, fixed, dropped, compared = [], [], [], []
            controls += 1
            for bone in range(nb):
                bone_at = i(data, 160) + 196 * bone
                source = old_sample(data, bone_at, at + i(data, at + 48) + 32 * bone, frame)
                local = source
                other_local = source
                if bone in procs and procs[bone]['flags'] & 4:
                    proc = procs[bone]
                    assert proc['control'] < bone, 'control must be available before helper'
                    local = solve(proc, locals_fixed[proc['control']][1], normalization)
                    if modern is not None and bone in modern:
                        other_local = solve(modern[bone], locals_fixed[proc['control']][1])
                    elif modern is None:
                        other_local = local
                locals_fixed.append(local)
                parent = parents[bone]
                fixed.append(compose(fixed[parent], local) if parent >= 0 else local)
                dropped.append(compose(dropped[parent], source) if parent >= 0 else source)
                compared.append(compose(compared[parent], other_local) if parent >= 0 else other_local)
                if bone in procs:
                    samples += 1
                    missing_p, missing_q = error(fixed[-1], dropped[-1])
                    compare_p, compare_q = error(fixed[-1], compared[-1])
                    max_compare_position = max(max_compare_position, compare_p)
                    max_compare_angle = max(max_compare_angle, compare_q)
                    if missing_p > max_missing_position or missing_q > max_missing_angle:
                        worst.append({'animation': name(data, at + i(data, at)), 'frame': frame,
                                      'bone': proc['name'], 'missing_position_units': missing_p,
                                      'missing_rotation_degrees': missing_q})
                    max_missing_position = max(max_missing_position, missing_p)
                    max_missing_angle = max(max_missing_angle, missing_q)
    return {'original': str(original_path), 'version': version,
            'converted': str(converted_path) if converted_path else None,
            'procedural_bones': len(procs), 'payload_mismatches': mismatches,
            'authored_absolute_control_poses': controls, 'helper_transform_samples': samples,
            'max_dropped_helper_position_error_units': max_missing_position,
            'max_dropped_helper_rotation_error_degrees': max_missing_angle,
            'max_preserved_helper_position_error_units': max_compare_position,
            'max_preserved_helper_rotation_error_degrees': max_compare_angle,
            'normalization': normalization,
            'worst_missing_helper_examples': worst[-10:],
            'scope': 'Same authored source control poses isolate payload conversion; solver uses modern acos clamp and normalized final quaternion. Runtime visuals and raw-channel quantization are separate checks.'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('original', type=Path)
    ap.add_argument('--converted', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    result = audit(args.original, args.converted)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'worst_missing_helper_examples'}, indent=2))
    return 1 if result['payload_mismatches'] or result['max_preserved_helper_position_error_units'] > .00001 or result['max_preserved_helper_rotation_error_degrees'] > .001 else 0


if __name__ == '__main__':
    raise SystemExit(main())
