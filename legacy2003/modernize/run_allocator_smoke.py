#!/usr/bin/env python3
"""Build and execute the allocator DLL-boundary test in an x86 VS prompt."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(mode):
    output = ROOT / '.modern_build_logs' / 'allocator' / mode
    output.mkdir(parents=True, exist_ok=True)
    steps = []

    def execute(name, args):
        with (output / (name + '.log')).open('w', encoding='utf-8') as log:
            result = subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        steps.append({'step': name, 'return_code': result.returncode})
        return result.returncode == 0

    try:
        for project in ('tier0', 'vstdlib'):
            if not execute(project, ['msbuild', str(ROOT / project / (project + '.vcxproj')),
                                    '/nologo', '/m:1', '/t:Build', '/v:minimal',
                                    '/p:Configuration=' + mode, '/p:Platform=Win32']):
                return False
        flags = ['/MTd', '/D_DEBUG'] if mode == 'Debug' else ['/MT', '/DNDEBUG']
        executable = output / 'allocator.exe'
        if not execute('compile', ['cl', '/nologo', '/EHsc', '/W3'] + flags + [
                '/I' + str(ROOT / 'public'), str(ROOT / 'modernize/tests/allocator_runtime_smoke.cpp'),
                str(ROOT / 'public/tier0/memoverride.cpp'), '/Fo' + str(output) + '\\',
                '/Fe' + str(executable), '/Fd' + str(output / 'compiler.pdb'), '/link',
                str(ROOT / 'tier0/build' / mode / 'tier0.lib'),
                str(ROOT / 'vstdlib/build' / mode / 'vstdlib.lib')]):
            return False
        for project in ('tier0', 'vstdlib'):
            shutil.copy2(ROOT / project / 'build' / mode / (project + '.dll'), output)
        return execute('runtime', [str(executable)])
    finally:
        (output / 'report.json').write_text(json.dumps({'mode': mode, 'steps': steps,
            'passed': len(steps) == 4 and all(s['return_code'] == 0 for s in steps)}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', nargs='?', choices=['Debug', 'Release', 'Both'], default='Both')
    args = parser.parse_args()
    if os.name != 'nt' or os.environ.get('VSCMD_ARG_TGT_ARCH') != 'x86' or not shutil.which('cl'):
        parser.error('Run from an x86 Visual Studio developer prompt.')
    modes = ('Debug', 'Release') if args.mode == 'Both' else (args.mode,)
    results = [run(mode) for mode in modes]
    print('Allocator DLL tests:', ', '.join(mode + '=' + ('passed' if ok else 'failed')
          for mode, ok in zip(modes, results)))
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
