#!/usr/bin/env python3
"""Independently check legacy surface inheritance and movement multipliers.

This verifies the content/parser contract; the engine runtime must additionally
confirm GAMMA_MOVEMENT surface_speed/surface_jump and actual displacement.
"""
from pathlib import Path
import argparse, json, re

TOKEN = re.compile(r'//[^\n]*|"([^"\\]*(?:\\.[^"\\]*)*)"|([{}])|([^\s{}"]+)')

def parse(text):
    tokens = []
    for match in TOKEN.finditer(text):
        if match.group(0).startswith('//'):
            continue
        tokens.append(next(value for value in match.groups() if value is not None))
    blocks, pos = [], 0
    while pos < len(tokens):
        name = tokens[pos]; pos += 1
        if pos >= len(tokens) or tokens[pos] != '{':
            raise ValueError('Expected surface block for ' + name)
        pos += 1; values = []
        while pos < len(tokens) and tokens[pos] != '}':
            key, value = tokens[pos:pos + 2]; pos += 2
            if value in ('{', '}'):
                raise ValueError('Nested or incomplete property: ' + key)
            values.append((key.lower(), value))
        if pos >= len(tokens):
            raise ValueError('Unclosed surface: ' + name)
        pos += 1; blocks.append((name.lower(), values))
    return blocks

def evaluate(blocks, missing_factor):
    result = {}
    for name, pairs in blocks:
        current = dict(result.get(name, result.get('default', {
            'maxspeedfactor': missing_factor, 'jumpfactor': missing_factor})))
        for key, value in pairs:
            if key == 'base':
                current = dict(result.get(value.lower(), current))
            elif key in ('maxspeedfactor', 'jumpfactor'):
                current[key] = float(value)
        result[name] = current
    return result

def verify(legacy, overlay):
    old_blocks, new_blocks = parse(legacy), parse(overlay)
    strip = lambda blocks: [(name, [(key, value) for key, value in pairs
        if not (name == 'default' and key in ('maxspeedfactor', 'jumpfactor'))])
        for name, pairs in blocks]
    assert strip(old_blocks) == strip(new_blocks), 'Unrelated surface definitions changed'
    original = evaluate(old_blocks, 0.0)
    parser_fixed = evaluate(old_blocks, 1.0)
    content_fixed = evaluate(new_blocks, 0.0)
    assert parser_fixed == content_fixed, 'Parser and content fixes disagree'
    assert all(values['maxspeedfactor'] > 0 and values['jumpfactor'] > 0
        for values in parser_fixed.values()), 'An unexpected immobile legacy surface remains'
    assert parser_fixed['mud'] == {'maxspeedfactor': 0.5, 'jumpfactor': 0.7}
    explicit = evaluate(parse('"default" { } "blocked" { "maxspeedfactor" "0" "jumpfactor" "0" } "inheritsblocked" { "base" "blocked" }'), 1.0)
    assert explicit['blocked'] == explicit['inheritsblocked'] == {'maxspeedfactor': 0, 'jumpfactor': 0}
    return {'passed': True, 'surfaces': len(parser_fixed),
        'previously_zero_speed': sum(v['maxspeedfactor'] == 0 for v in original.values()),
        'previously_zero_jump': sum(v['jumpfactor'] == 0 for v in original.values()),
        'default': parser_fixed['default'], 'mud': parser_fixed['mud'],
        'explicit_zero_and_inheritance_preserved': True}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    result = verify((args.repo/'gammasource_content/scripts/surfaceproperties.txt').read_text(),
        (args.repo/'gammasource_content_2018/scripts/surfaceproperties.txt').read_text())
    text = json.dumps(result, indent=2)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True); args.out.write_text(text + '\n')
    print(text)

if __name__ == '__main__':
    main()
