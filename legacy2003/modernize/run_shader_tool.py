#!/usr/bin/env python3
"""Run an original shader-preparation script with a working Perl interpreter.

Set HL2_MODERN_PERL to a portable perl.exe, set the HL2ModernPerl MSBuild
property, or put Perl on PATH. The bundled VC6 interpreter is not substituted
or modified. Interpreter and script failures propagate to the project build.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def resolve_perl(override=None, environ=None):
    environ = os.environ if environ is None else environ
    requested = override or environ.get('HL2_MODERN_PERL') or 'perl'
    executable = shutil.which(requested)
    if executable is None and Path(requested).is_file():
        executable = str(Path(requested).resolve())
    if executable is None:
        raise ValueError('Perl was not found. Set HL2_MODERN_PERL to a portable perl.exe or install Perl on PATH.')
    try:
        probe = subprocess.run([executable, '-e', 'print $^V'], capture_output=True,
                               text=True, errors='replace', timeout=10)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError('Unable to run the selected Perl: '+str(exc)) from exc
    version = probe.stdout.strip()
    if probe.returncode or not re.fullmatch(r'v5\.\d+\.\d+', version):
        raise ValueError('Selected Perl failed its version probe: '+(probe.stderr.strip() or version or str(probe.returncode)))
    return executable, version


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--perl', help='Perl executable; overrides HL2_MODERN_PERL and PATH.')
    parser.add_argument('--probe', action='store_true', help='Validate and print the selected Perl only.')
    parser.add_argument('script', nargs='?', type=Path)
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    try:
        executable, version = resolve_perl(args.perl)
        if args.probe:
            print(version+' '+executable)
            return 0
        if args.script is None or not args.script.is_file():
            raise ValueError('Shader preparation script not found: '+str(args.script))
        environment = os.environ.copy()
        helper = str(Path(__file__).with_name('legacy_shader_tool.py'))
        environment.setdefault('HL2_MODERN_FXC_COMMAND', subprocess.list2cmdline([sys.executable, helper, 'compile']))
        environment.setdefault('HL2_MODERN_ASSEMBLER_COMMAND', subprocess.list2cmdline([sys.executable, helper, 'assemble']))
        return subprocess.run([executable, str(args.script), *args.arguments], env=environment, check=False).returncode
    except (OSError, ValueError) as exc:
        print('Shader tool error: '+str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
