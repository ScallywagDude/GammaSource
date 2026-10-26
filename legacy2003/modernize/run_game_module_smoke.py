#!/usr/bin/env python3
"""Load rebuilt x86 game DLLs with rebuilt dependencies; not a gameplay test."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('branch', choices=['HL2', 'HL1', 'TF2', 'CounterStrike'])
    parser.add_argument('--mode', choices=['Debug', 'Release'], default='Release')
    parser.add_argument('--client', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt' or os.environ.get('VSCMD_ARG_TGT_ARCH') != 'x86':
        parser.error('Run from an x86 Visual Studio developer prompt.')
    kind = 'client' if args.client else 'server'
    output = ROOT / '.modern_build_logs/module_load' / args.mode / args.branch / kind
    output.mkdir(parents=True, exist_ok=True)
    executable = output / 'module_load.exe'
    with (output / 'compile.log').open('w') as log:
        result = subprocess.run(['cl', '/nologo', '/MT', '/EHsc',
            str(ROOT / 'modernize/tests/game_module_smoke.cpp'), '/Fo' + str(output) + '\\',
            '/Fe' + str(executable)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        return result.returncode
    for dependency in ('tier0', 'vstdlib'):
        shutil.copy2(ROOT / dependency / 'build' / args.mode / (dependency + '.dll'), output)
    source = ROOT / ('cl_dll' if args.client else 'dlls') / 'build' / (args.mode + ' ' + args.branch) / (kind + '.dll')
    module = output / source.name
    shutil.copy2(source, module)
    interfaces = ['VClient007'] if args.client else ['ServerGameDLL002', 'ServerGameEnts001', 'ServerGameClients002']
    with (output / 'runtime.log').open('w') as log:
        result = subprocess.run([str(executable), str(module)] + interfaces, cwd=output,
                                stdout=log, stderr=subprocess.STDOUT)
    (output / 'report.json').write_text(json.dumps({'branch': args.branch, 'mode': args.mode,
        'kind': kind, 'return_code': result.returncode, 'module_loaded': result.returncode == 0,
        'gameplay_verified': False}, indent=2) + '\n')
    print((output / 'runtime.log').read_text())
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
