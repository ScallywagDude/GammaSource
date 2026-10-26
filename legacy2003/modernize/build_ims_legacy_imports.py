#!/usr/bin/env python3
"""Build genuine x86 VC6 runtime imports for the release TalkBack DLL-CRT archive.

TalkBack's C interface keeps its old C++ objects and FILE layout inside its
matching Windows msvcp60/msvcrt runtimes. C imports are IAT-only symbols: the
archive uses __imp_ pointers, while the modern wrapper keeps separate static
CRT function definitions. No runtime implementation or ABI translation is added.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess


def exports_from_x86_dll(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise RuntimeError('Not a PE image: ' + str(path))
    machine, section_count = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    if machine != 0x14c or struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise RuntimeError('Expected genuine x86 runtime DLL: ' + str(path))
    sections = []
    for index in range(section_count):
        row = optional + optional_size + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from('<IIII', data, row + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_offset))

    def offset(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva - address
        raise RuntimeError('Invalid export RVA in ' + str(path))

    export_rva = struct.unpack_from('<I', data, optional + 96)[0]
    directory = offset(export_rva)
    count = struct.unpack_from('<I', data, directory + 24)[0]
    names_rva = struct.unpack_from('<I', data, directory + 32)[0]
    names = offset(names_rva)
    result = set()
    for index in range(count):
        start = offset(struct.unpack_from('<I', data, names + index * 4)[0])
        result.add(data[start:data.index(0, start)].decode('ascii'))
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--talkback', required=True, type=Path)
    parser.add_argument('--dumpbin', required=True, type=Path)
    parser.add_argument('--lib-tool', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    windows = Path(os.environ.get('SystemRoot', r'C:\Windows'))
    system = windows / 'SysWOW64'
    if not system.is_dir():
        system = windows / 'System32'
    runtimes = {name: system / (name + '.dll') for name in ('msvcp60', 'msvcrt')}
    for path in [args.talkback, args.dumpbin, args.lib_tool, *runtimes.values()]:
        if not path.is_file():
            raise RuntimeError('Missing required genuine legacy-runtime dependency: ' + str(path))
    fingerprint = {
        'helper_sha256': digest(Path(__file__)),
        'talkback_sha256': digest(args.talkback),
        'runtime_sha256': {name: digest(path) for name, path in runtimes.items()},
        'lib_tool_sha256': digest(args.lib_tool),
    }
    manifest = output / 'legacy-runtime-imports.json'
    libraries = [output / ('legacy_' + name + '_import.lib') for name in runtimes]
    if manifest.exists() and all(path.exists() for path in libraries):
        saved = json.loads(manifest.read_text(encoding='utf-8'))
        if saved.get('inputs') == fingerprint:
            print('IMS legacy runtime imports: verified cached inputs')
            return

    dump = subprocess.run([str(args.dumpbin), '/nologo', '/symbols', str(args.talkback)],
                          check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    symbols = dump.stdout.decode('utf-8', errors='replace')
    needed = {}
    for line in symbols.splitlines():
        match = re.search(r'UNDEF([^|]*)\|\s+(\S+)', line)
        if not match:
            continue
        name = match[2]
        if name.startswith('__imp_'):
            name = name[6:]
        elif not name.startswith('?'):
            continue
        needed[name] = needed.get(name, False) or '()' in match[1]

    selections = {}
    for name, runtime in runtimes.items():
        exports = exports_from_x86_dll(runtime)
        selected = []
        for symbol, function in sorted(needed.items()):
            exported = symbol if symbol.startswith('?') else symbol[1:] if symbol.startswith('_') else symbol
            if exported not in exports:
                continue
            # IAT-only C imports intentionally omit direct callable thunks. This
            # keeps TalkBack's dllimport calls separate from the modern wrapper
            # static CRT. Both the imported pointer and its real ABI are intact.
            pointer_only = not symbol.startswith('?') or not function
            selected.append(exported + (' DATA' if pointer_only else ''))
        if not selected:
            raise RuntimeError('No matching verified runtime imports for ' + name)
        definition = output / ('legacy_' + name + '.def')
        definition.write_text('LIBRARY ' + name + '.dll\nEXPORTS\n' +
                              ''.join('    ' + item + '\n' for item in selected), encoding='ascii')
        library = output / ('legacy_' + name + '_import.lib')
        subprocess.run([str(args.lib_tool), '/nologo', '/machine:x86',
                        '/def:' + str(definition), '/out:' + str(library)], check=True)
        selections[name] = {'dll': str(runtime), 'exports': selected}
    manifest.write_text(json.dumps({'inputs': fingerprint, 'imports': selections}, indent=2) + '\n', encoding='utf-8')
    print('IMS legacy runtime imports: generated from verified real Windows x86 exports')


if __name__ == '__main__':
    main()
