"""Compare actual old and converted per-sequence/model KeyValues payloads."""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path

from audit_legacy_animation_samples import i, name, normalize_original


def payload(data, offset, size):
    if size == 0:
        return b''
    if size < 0 or offset < 0 or offset + size > len(data):
        raise ValueError('Invalid KeyValues span: %d + %d / %d' % (offset, size, len(data)))
    return data[offset:offset + size]


def kind(raw):
    text = raw.decode('latin1')
    found = re.search(r'"?faceposer"?\s*\{.*?"?type"?\s+"?([^\s"}]+)', text, re.I | re.S)
    return found.group(1).lower() if found else None


def metadata(data, modern=False):
    stride, index_field, size_field = (212, 172, 176) if modern else (4284, 4264, 4268)
    rows = []
    for sequence in range(i(data, 188)):
        at = i(data, 192) + stride * sequence
        relative, size = i(data, at + index_field), i(data, at + size_field)
        raw = payload(data, at + relative, size)
        rows.append({'sequence': sequence, 'label': name(data, at + i(data, at + (4 if modern else 0))),
                     'relative_offset': relative, 'bytes': size, 'type': kind(raw), 'payload': raw})
    return rows, payload(data, i(data, 312), i(data, 316))


def audit(original, converted=None):
    old, version = normalize_original(original.read_bytes())
    before, model_before = metadata(old)
    after, model_after = metadata(converted.read_bytes(), True) if converted else (None, None)
    mismatches = []
    if after is not None:
        if len(before) != len(after):
            mismatches.append({'issue': 'sequence count changed'})
        for source, dest in zip(before, after):
            if source['label'] != dest['label'] or source['payload'] != dest['payload']:
                mismatches.append({'sequence': source['sequence'], 'label': source['label'],
                                   'source_bytes': source['bytes'], 'converted_bytes': dest['bytes'],
                                   'source_type': source['type'], 'converted_type': dest['type']})
        if model_before != model_after:
            mismatches.append({'issue': 'global model KeyValues changed'})
    rows = [{k: v for k, v in row.items() if k != 'payload'} for row in before if row['bytes']]
    return {'original': str(original), 'version': version, 'converted': str(converted) if converted else None,
            'sequences': len(before), 'source_sequences_with_keyvalues': len(rows),
            'source_sequence_bytes': sum(row['bytes'] for row in before),
            'source_negative_keyvalue_offsets': sum(row['bytes'] > 0 and row['relative_offset'] < 0 for row in before),
            'source_faceposer_types': dict(collections.Counter(row['type'] for row in before if row['type'])),
            'converted_sequences_with_keyvalues': sum(bool(row['bytes']) for row in after) if after is not None else None,
            'model_keyvalue_bytes': len(model_before),
            'model_keyvalue_sha256': hashlib.sha256(model_before).hexdigest(),
            'mismatches': mismatches, 'original_metadata': rows}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('original', type=Path)
    ap.add_argument('--converted', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    result = audit(args.original, args.converted)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('mismatches', 'original_metadata')}, indent=2))
    print('KeyValues mismatches: %d' % len(result['mismatches']))
    return 1 if result['mismatches'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
