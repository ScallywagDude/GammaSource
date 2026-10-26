"""Check original IK bounds and compare serialized runtime v48 rules/locks byte for byte.

The static-pose exception is measured separately: 2003 studiomdl divided by zero
when normalizing a one-frame rule. Its NaN envelope had zero influence; v48 must
encode that influence with finite zeros while retaining the first error sample.
"""
import argparse
import collections
import json
import math
from pathlib import Path

from audit_legacy_animation_samples import i, f, normalize_original


def require(condition, message):
    if not condition:
        raise ValueError(message)


def span(data, offset, size):
    require(0 <= offset <= len(data) and 0 <= size <= len(data) - offset,
            'out of bounds: %d + %d / %d' % (offset, size, len(data)))
    return data[offset:offset + size]


def finite(data, offset, count):
    span(data, offset, 4 * count)
    require(all(math.isfinite(f(data, offset + 4 * k)) for k in range(count)),
            'nonfinite field at %d' % offset)


def chain(data, number):
    require(0 <= number < i(data, 284), 'invalid chain')
    offset = i(data, 288) + 16 * number
    require(i(data, offset + 8) == 3, 'IK requires three links')
    links = offset + i(data, offset + 12)
    span(data, links, 84)
    require(all(0 <= i(data, links + 28 * k) < i(data, 156) for k in range(3)),
            'invalid chain bone')


def locks(data, owner, count_field, index_field, stride):
    count = i(data, owner + count_field)
    require(0 <= count <= 256, 'invalid lock count')
    offset = owner + i(data, owner + index_field)
    span(data, offset, count * stride)
    result = []
    for k in range(count):
        at = offset + stride * k
        chain(data, i(data, at))
        require(all(math.isfinite(f(data, at + x)) and 0 <= f(data, at + x) <= 1
                    for x in (4, 8)), 'invalid lock weight')
        result.append(span(data, at, 12))
    return result


def sampling_bounds(frames, first, count_errors, envelope, kind):
    """Check the decoder's array contract at regular cycles and envelope edges."""
    start, peak, tail, end = envelope
    cycles = {k / 1000 for k in range(1001)}
    for edge in envelope:
        edge %= 1
        cycles.update(max(0, min(1, edge + epsilon)) for epsilon in (-1e-6, 0, 1e-6))
    checked = 0
    for cycle in cycles:
        adjusted = cycle + (1 if end > 1 and cycle < start else 0)
        if adjusted < start:
            weight = 0
        elif adjusted < peak:
            weight = (adjusted - start) / (peak - start)
        elif adjusted < tail:
            weight = 1
        elif adjusted < end:
            weight = 1 - (adjusted - tail) / (end - tail)
        else:
            weight = 0
        weight = weight * weight * (3 - 2 * weight)
        if weight < .0001 and kind != 3:
            continue
        if weight >= .0001:
            frame = (frames - 1) * cycle
            number = int(frame)
            fraction = frame - number
            if end > 1 and cycle < start:
                number += frames
        else:
            # Modern inactive-ground sampling clamps to the envelope endpoints.
            frame = (frames - 1) * (adjusted - start) + first
            if adjusted < start:
                frame = first
            elif adjusted >= end:
                frame = (frames - 1) * (end - start) + first
            number = int(frame)
            fraction = frame - number
        offset = number - first
        require(0 <= offset < count_errors and
                (fraction < .001 or offset + 1 < count_errors),
                'IK error sample out of span at cycle %.9f: %d + %.9f / %d' %
                (cycle, offset, fraction, count_errors))
        checked += 1
    return checked


def original(data):
    require(0 <= i(data, 284) <= 64, 'invalid chain count')
    span(data, i(data, 288), i(data, 284) * 16)
    rules = []
    for animation in range(i(data, 180)):
        anim = i(data, 184) + 92 * animation
        frames = i(data, anim + 12)
        count = i(data, anim + 52)
        require(0 <= count <= 64, 'invalid rule count')
        table = anim + i(data, anim + 56)
        span(data, table, 108 * count)
        items = []
        for k in range(count):
            at = table + 108 * k
            chain(data, i(data, at + 8))
            require(0 <= i(data, at + 4) <= 4, 'invalid rule type')
            require(-1 <= i(data, at + 12) < i(data, 156), 'invalid rule bone')
            require(0 <= i(data, at + 16) < 12, 'invalid rule slot')
            finite(data, at + 20, 10)
            require(frames >= 1, 'invalid rule frames')
            static = frames == 1 and not all(math.isfinite(f(data, at + x)) for x in (76, 80, 84, 88, 96))
            first = 0 if static else i(data, at + 68)
            if static:
                count_errors = 1
                envelope = bytes(16)
                contact = bytes(4)
            else:
                finite(data, at + 76, 4)
                start, peak, tail, end = [f(data, at + x) for x in (76, 80, 84, 88)]
                require(-1 <= start <= peak <= tail <= end <= 3, 'invalid envelope')
                end_frame = end * (frames - 1)
                last = math.floor(end_frame + .5)
                require(abs(end_frame - last) <= .01, 'fractional end frame')
                count_errors = last - first + 1 + (2 if last >= frames else 0)
                require(1 <= count_errors <= 3 * frames + 4, 'invalid error count')
                envelope = span(data, at + 76, 16)
                finite(data, at + 96, 1)
                contact = span(data, at + 96, 4)
                sampling_bounds(frames, first, count_errors, (start, peak, tail, end), i(data, at + 4))
            error_index = i(data, at + 72)
            require(error_index > 0, 'missing errors')
            finite(data, at + error_index, 7 * count_errors)
            items.append({'common': span(data, at, 60), 'first': first,
                          'envelope': envelope, 'contact': contact,
                          'errors': span(data, at + error_index, 28 * count_errors),
                          'static': static, 'type': i(data, at + 4),
                          'wrapped': not static and f(data, at + 88) > 1})
        rules.append(items)
    sequence_locks = []
    rule_sequences = 0
    for seq in range(i(data, 188)):
        at = i(data, 192) + 4284 * seq
        sequence_locks.append(locks(data, at, 4256, 4260, 12))
        rule_sequences += bool(i(data, at + 4236))
    return {'rules': rules, 'sequence_locks': sequence_locks,
            'global_locks': locks(data, 0, 320, 324, 12), 'rule_sequences': rule_sequences}


def compare(old, new, expected):
    require(i(new, 4) == 48 and i(old, 8) == i(new, 8), 'version/checksum mismatch')
    require(i(old, 180) == i(new, 180), 'animation count mismatch')
    require(i(old, 188) == i(new, 188), 'sequence count mismatch')
    require(i(old, 284) == i(new, 284), 'chain count mismatch')
    for k in range(i(old, 284)):
        source = i(old, 288) + 16 * k
        dest = i(new, 288) + 16 * k
        require(span(old, source + 4, 8) == span(new, dest + 4, 8), 'chain type/length mismatch')
        for link in range(i(old, source + 8)):
            old_link = source + i(old, source + 12) + 28 * link
            new_link = dest + i(new, dest + 12) + 28 * link
            require(i(old, old_link) == i(new, new_link), 'chain link bone mismatch')
            require(span(new, new_link + 4, 24) == bytes(24), 'modern knee/reserved vectors not cleared')
    for animation, rules in enumerate(expected['rules']):
        at = i(new, 184) + 100 * animation
        require(i(new, at + 12) & 0x40000000, 'missing legacy animation marker')
        require(i(new, at + 60) == len(rules), 'rule count mismatch')
        table = at + i(new, at + 64)
        span(new, table, 152 * len(rules))
        for k, rule in enumerate(rules):
            dest = table + 152 * k
            require(span(new, dest, 60) == rule['common'], 'rule common fields mismatch')
            require(span(new, dest + 60, 8) == bytes(8), 'modern-only pointers not cleared')
            require(i(new, dest + 68) == rule['first'], 'error start mismatch')
            require(span(new, dest + 76, 16) == rule['envelope'], 'envelope mismatch')
            require(span(new, dest + 92, 4) == bytes(4), 'unused commit field not cleared')
            require(span(new, dest + 96, 4) == rule['contact'], 'contact mismatch')
            require(span(new, dest + 100, 52) == bytes(52), 'modern-only rule fields not cleared')
            errors = dest + i(new, dest + 72)
            require(span(new, errors, len(rule['errors'])) == rule['errors'], 'error samples mismatch')
    for seq, expected_locks in enumerate(expected['sequence_locks']):
        at = i(new, 192) + 212 * seq
        require(i(new, at + 12) & 0x40000000, 'missing legacy sequence marker')
        require(i(new, at + 144) == i(old, i(old, 192) + 4284 * seq + 4236), 'sequence rule count mismatch')
        require(locks(new, at, 164, 168, 32) == expected_locks, 'sequence locks mismatch')
        start = at + i(new, at + 168)
        for k in range(len(expected_locks)):
            require(span(new, start + 32 * k + 12, 20) == bytes(20), 'lock flags/reserved not cleared')
    require(locks(new, 0, 320, 324, 32) == expected['global_locks'], 'global locks mismatch')
    for k in range(len(expected['global_locks'])):
        require(span(new, i(new, 324) + 32 * k + 12, 20) == bytes(20), 'global lock flags/reserved not cleared')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--cache', type=Path, help='Actual versioned legacy_models_cache/models directory')
    ap.add_argument('--warmup-manifest', type=Path, help='Optional manifest containing exact v37 alias originals')
    ap.add_argument('--cache-only', action='store_true', help='Read only originals with an existing cache path')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    if args.cache and not args.cache.is_dir():
        ap.error('cache directory does not exist')
    if args.cache_only and not args.cache:
        ap.error('--cache-only requires --cache')
    metadata = json.loads(args.inventory.read_text())['models']
    if args.warmup_manifest:
        for alias in json.loads(args.warmup_manifest.read_text())['v37_private_aliases']:
            metadata.append(dict(alias['original'], model=alias['model']))
    seen, rows, failures, excluded = set(), [], [], []
    source_counts, cache_counts = collections.Counter(), collections.Counter()
    for meta in metadata:
        if meta['status'] != 'legacy_studio':
            continue
        cached = args.cache / Path(meta['model']).relative_to('models') if args.cache else None
        if args.cache_only and not cached.is_file():
            continue
        key = (meta['model'].lower(), meta['checksum'], meta['version'])
        if key in seen:
            continue
        seen.add(key)
        path = Path(meta['source'])
        try:
            data, version = normalize_original(path.read_bytes())
        except Exception as error:
            excluded.append({'source': str(path), 'model': meta['model'], 'error': str(error)})
            continue
        try:
            expected = original(data)
            source_counts['models'] += 1
            rules = [r for anim in expected['rules'] for r in anim]
            source_counts['rules'] += len(rules)
            source_counts['static_rules_normalized'] += sum(r['static'] for r in rules)
            source_counts['wrapped_rules'] += sum(r['wrapped'] for r in rules)
            source_counts['sequence_locks'] += sum(map(len, expected['sequence_locks']))
            source_counts['global_locks'] += len(expected['global_locks'])
            if rules or any(expected['sequence_locks']) or expected['global_locks']:
                rows.append({'model': meta['model'], 'version': version,
                             'rules': len(rules), 'static_rules': sum(r['static'] for r in rules),
                             'rule_sequences': expected['rule_sequences'],
                             'locked_sequences': sum(bool(x) for x in expected['sequence_locks'])})
            if args.cache:
                if cached.is_file():
                    new = cached.read_bytes()
                    if i(new, 8) == i(data, 8):
                        compare(data, new, expected)
                        cache_counts['pairs'] += 1
                        cache_counts['rules'] += len(rules)
                        cache_counts['static_rules'] += sum(r['static'] for r in rules)
                        cache_counts['sequence_locks'] += sum(map(len, expected['sequence_locks']))
                    else:
                        cache_counts['checksum_nonmatches_skipped'] += 1
        except Exception as error:
            failures.append({'source': str(path), 'model': meta['model'], 'error': str(error)})
    report = {'source_counts': dict(source_counts), 'cache_counts': dict(cache_counts),
              'failures': failures, 'excluded_original_layouts': excluded, 'models_with_ik': rows,
              'coverage_limit': 'Cache comparison covers only existing outputs with matching original checksum.'}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'models_with_ik'}, indent=2))
    return 1 if failures else (2 if args.cache and not cache_counts['pairs'] else 0)


if __name__ == '__main__':
    raise SystemExit(main())
