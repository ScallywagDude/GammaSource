#!/usr/bin/env python3
"""Build all 61 original HL2 Win32 steps with MSVC 14.51 and retained logs.

Use --rebuild to clean and compile every ordered project instead of reusing its
current outputs. No project regeneration, download, or global install occurs.
Legacy /SAFESEH:NO remains opt-in with --legacy-unsafe-seh. Shader profiles are
unchanged. A source-wide OS lock prevents competing runs sharing shader temps.
"""
from __future__ import annotations
import argparse
import contextlib
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

from build_driver import plan

ROOT = Path(__file__).resolve().parents[1]
MSVC_VERSION = '14.51.36231'
EXPECTED_STEPS = 61


def normalized_environment(environment):
    """Windows/MSBuild rejects a dictionary containing Path and PATH together."""
    return {key.upper(): str(value) for key, value in environment.items()}


def find_visual_studio(environment):
    candidates = []
    if environment.get('VSINSTALLDIR'):
        candidates.append(Path(environment['VSINSTALLDIR']))
    program_files = Path(environment.get('PROGRAMFILES', r'C:\Program Files'))
    program_files_x86 = Path(environment.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)'))
    vswhere = program_files_x86/'Microsoft Visual Studio/Installer/vswhere.exe'
    if vswhere.is_file():
        result = subprocess.run([str(vswhere), '-all', '-products', '*', '-format', 'json'],
                                capture_output=True, text=True, encoding='utf-8-sig',
                                errors='replace', env=environment, check=False)
        if result.returncode == 0:
            try:
                candidates.extend(Path(item['installationPath']) for item in json.loads(result.stdout))
            except (ValueError, KeyError, TypeError):
                pass
    for base in (program_files, program_files_x86):
        for product in ('Community', 'Professional', 'Enterprise', 'BuildTools'):
            candidates.append(base/'Microsoft Visual Studio/18'/product)
    for candidate in dict.fromkeys(candidates):
        tools = candidate/'VC/Tools/MSVC'/MSVC_VERSION
        msbuild = candidate/'MSBuild/Current/Bin/MSBuild.exe'
        devcmd = candidate/'Common7/Tools/VsDevCmd.bat'
        compiler = any((tools/'bin'/host/'x86/cl.exe').is_file()
                       for host in ('Hostx64', 'HostX64', 'Hostx86', 'HostX86'))
        if (msbuild.is_file() and devcmd.is_file() and compiler
                and (tools/'atlmfc/include/afxwin.h').is_file()
                and (tools/'atlmfc/lib/x86/mfc140.lib').is_file()):
            return candidate, msbuild, devcmd
    raise RuntimeError('Visual Studio with MSVC '+MSVC_VERSION+
                       ' x86 tools and matching MFC headers/libraries was not found.')


def build_environment(devcmd, environment):
    # A temporary batch file avoids ambiguous nested cmd.exe quoting. It changes
    # only this build's child-process environment and is removed automatically.
    with tempfile.TemporaryDirectory(prefix='gamma-win32-env-') as temporary:
        batch = Path(temporary)/'environment.cmd'
        batch.write_text('@echo off\ncall "'+str(devcmd)+'" -arch=x86 -host_arch=x64 '
                         '-vcvars_ver='+MSVC_VERSION+' >nul\n'
                         'if errorlevel 1 exit /b 2\nset\n', encoding='mbcs')
        result = subprocess.run([environment.get('COMSPEC', 'cmd.exe'), '/d', '/u', '/c', str(batch)],
                                capture_output=True, encoding='utf-16le', errors='replace',
                                env=environment, check=False)
    if result.returncode:
        raise RuntimeError('Could not initialize the Visual Studio x86 environment: '+result.stderr.strip())
    merged = dict(environment)
    merged.update(line.split('=', 1) for line in result.stdout.splitlines()
                  if '=' in line and not line.startswith('='))
    merged = normalized_environment(merged)
    merged.update(LC_ALL='C', LC_CTYPE='C', LANG='C', VSLANG='1033', DOTNET_CLI_UI_LANGUAGE='en-US')
    if merged.get('VSCMD_ARG_TGT_ARCH', '').lower() != 'x86':
        raise RuntimeError('Visual Studio did not initialize the required x86 target.')
    selected = merged.get('VCTOOLSVERSION', '').rstrip('\\/')
    if selected != MSVC_VERSION:
        raise RuntimeError('Visual Studio selected MSVC '+selected+'; required '+MSVC_VERSION+'.')
    return merged


def select_shader_tools(environment, tools_dir=None):
    roots = ([tools_dir] if tools_dir else []) + [ROOT/'modernize/tools']
    roots = [Path(path).resolve() for path in roots if path is not None]
    if tools_dir is not None and not Path(tools_dir).is_dir():
        raise RuntimeError('--tools-dir does not name an existing directory.')
    architecture = 'x64' if struct.calcsize('P') == 8 else 'x86'
    patterns = {
        'HL2_MODERN_PERL': ['perl/bin/perl.exe', 'perl.exe', 'strawberry*/perl/bin/perl.exe'],
        'HL2_MODERN_D3DX9': ['directx-june2010/'+architecture+'/d3dx9_31.dll',
                            architecture+'/d3dx9_31.dll', 'd3dx9_31_'+architecture+'/d3dx9_31.dll'],
        'HL2_MODERN_FXC': ['fxc/'+architecture+'/fxc.exe', architecture+'/fxc.exe', 'fxc.exe'],
        'HL2_MODERN_D3DCOMPILER': ['d3dcompiler/'+architecture+'/d3dcompiler_47.dll',
                                  architecture+'/d3dcompiler_47.dll'],
    }
    for variable, globs in patterns.items():
        if environment.get(variable):
            requested = environment[variable].strip('"')
            candidate = shutil.which(requested, path=environment.get('PATH'))
            if candidate is None and Path(requested).is_file():
                candidate = str(Path(requested).resolve())
            if candidate is None:
                raise RuntimeError(variable+' names a tool that does not exist: '+requested)
            environment[variable] = candidate
            continue
        found = next((path for root in roots for pattern in globs
                      for path in sorted(root.glob(pattern), reverse=True) if path.is_file()), None)
        if found:
            environment[variable] = str(found)
    if not environment.get('HL2_MODERN_PERL'):
        perl = shutil.which('perl', path=environment.get('PATH'))
        if perl:
            environment['HL2_MODERN_PERL'] = perl
    # Existing discovery understands installed Windows SDK FXC and the correct
    # D3DCompiler architecture. Temporarily expose the normalized environment.
    previous = dict(os.environ)
    try:
        os.environ.clear()
        os.environ.update(environment)
        from build_modern_shaders import resolve_tool
        from legacy_shader_tool import compiler_dll
        from run_shader_tool import resolve_perl
        if not environment.get('HL2_MODERN_FXC'):
            compiler = resolve_tool('fxc')
            if compiler:
                environment['HL2_MODERN_FXC'] = compiler
        if not environment.get('HL2_MODERN_D3DCOMPILER'):
            environment['HL2_MODERN_D3DCOMPILER'] = compiler_dll()
        _, version = resolve_perl(environ=environment)
        if not environment.get('HL2_MODERN_FXC'):
            raise RuntimeError('Windows SDK FXC not found; set HL2_MODERN_FXC.')
    finally:
        os.environ.clear()
        os.environ.update(previous)
    return version


@contextlib.contextmanager
def source_build_lock():
    """The OS releases this byte lock even if the owning process crashes."""
    import msvcrt
    path = ROOT/'.modern_build_logs/round9-source.lock'
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(str(path), os.O_RDWR | os.O_CREAT)
    with os.fdopen(descriptor, 'r+b') as handle:
        if handle.seek(0, os.SEEK_END) == 0:
            handle.write(b'\0')
            handle.flush()
        handle.seek(0)
        try:
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise RuntimeError('Another Round 9 build owns this source tree. '
                               'Wait for it to finish; shader temporary files are shared.') from error
        try:
            handle.seek(1)
            handle.write(json.dumps({'pid': os.getpid()}).encode('ascii'))
            handle.truncate()
            handle.flush()
            yield
        finally:
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)


def msbuild_command(msbuild, step, legacy_unsafe_seh=False, rebuild=False):
    command = [str(msbuild), str(ROOT/step['project']), '/nologo', '/m:1',
               '/t:'+('Rebuild' if rebuild else 'Build'),
               '/p:Configuration='+step['configuration'], '/p:Platform=Win32',
               '/p:VCToolsVersion='+MSVC_VERSION, '/v:normal', '/clp:Summary', '/nr:false']
    if legacy_unsafe_seh:
        command.append('/p:HL2ModernLegacyUnsafeSEH=true')
    return command


def clean_msbuild_summary(output):
    counts = re.findall(r'^\s*(\d+) Error\(s\)\s*$', output, re.MULTILINE | re.IGNORECASE)
    diagnostics = re.search(r'\b(?:fatal error|error\s+(?:C|LNK|MSB|RC|A)\d+)\b', output)
    return bool(counts) and all(int(count) == 0 for count in counts) and diagnostics is None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('configuration', choices=['Debug', 'Release'])
    parser.add_argument('--tools-dir', type=Path, help='Portable shader dependencies; environment overrides take priority.')
    parser.add_argument('--log-dir', type=Path, help='New or empty directory for all logs and report.json.')
    parser.add_argument('--legacy-unsafe-seh', action='store_true', help='Opt in to existing Win32 /SAFESEH:NO compatibility property.')
    parser.add_argument('--rebuild', action='store_true', help='Invoke MSBuild Rebuild for every ordered step, cleaning its existing outputs first.')
    parser.add_argument('--keep-going', action='store_true', help='Continue after a failed step for diagnostics.')
    parser.add_argument('--dry-run', action='store_true', help='Print the exact 61-step plan and flags without invoking MSBuild.')
    args = parser.parse_args(argv)
    try:
        steps = plan(args.configuration, ROOT)
        if len(steps) != EXPECTED_STEPS or [item['step'] for item in steps] != list(range(1, EXPECTED_STEPS+1)):
            raise RuntimeError('The original build plan does not contain exactly 61 ordered steps.')
        if args.dry_run:
            for step in steps:
                command = msbuild_command('MSBuild.exe', step, args.legacy_unsafe_seh, args.rebuild)
                print(f"{step['step']:02d}/61 {step['project']} [{step['configuration']}]")
                print('  '+subprocess.list2cmdline(command))
            print('Dry run only; full_build_passed=false.')
            return 0
        if os.name != 'nt':
            raise RuntimeError('Actual builds require Windows; --dry-run is available elsewhere.')
        with source_build_lock():
            environment = normalized_environment(os.environ)
            installation, msbuild, devcmd = find_visual_studio(environment)
            environment = build_environment(devcmd, environment)
            perl_version = select_shader_tools(environment, args.tools_dir)
            timestamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
            logs = (args.log_dir or ROOT/'.modern_build_logs'/('round9-'+args.configuration.lower()+'-'+timestamp)).resolve()
            if logs.exists() and any(logs.iterdir()):
                raise RuntimeError('Log directory is not empty; choose a new --log-dir to preserve build evidence.')
            logs.mkdir(parents=True, exist_ok=True)
            report = {'configuration': args.configuration, 'platform': 'Win32', 'expected_steps': EXPECTED_STEPS,
                      'msbuild_target': 'Rebuild' if args.rebuild else 'Build',
                      'full_build_passed': False, 'visual_studio': str(installation), 'msvc_version': MSVC_VERSION,
                      'legacy_unsafe_seh': args.legacy_unsafe_seh, 'perl_version': perl_version,
                      'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'steps': []}
            destination = logs/'report.json'
            def save_report():
                temporary = destination.with_suffix('.json.tmp')
                temporary.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
                temporary.replace(destination)
            save_report()
            print('Win32 '+args.configuration+'; MSVC '+MSVC_VERSION+'; '+perl_version, flush=True)
            for step in steps:
                command = msbuild_command(msbuild, step, args.legacy_unsafe_seh, args.rebuild)
                log_path = logs/(f"{step['step']:02d}_"+Path(step['project']).stem+'.log')
                print(f"[{step['step']}/61] {step['project']} [{step['configuration']}]", flush=True)
                start = time.monotonic()
                with log_path.open('w', encoding='utf-8', errors='replace') as log:
                    result = subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                                            stderr=subprocess.STDOUT, check=False)
                zero_errors = clean_msbuild_summary(log_path.read_text(encoding='utf-8', errors='replace'))
                passed = result.returncode == 0 and zero_errors
                report['steps'].append({**step, 'command': command, 'return_code': result.returncode,
                                        'zero_error_summary': zero_errors, 'passed': passed,
                                        'elapsed_seconds': round(time.monotonic()-start, 2), 'log': str(log_path)})
                save_report()
                print('  '+('PASS' if passed else 'FAIL')+'; '+str(log_path), flush=True)
                if not passed and not args.keep_going:
                    break
            report['full_build_passed'] = (len(report['steps']) == EXPECTED_STEPS
                and all(entry['passed'] for entry in report['steps'])
                and [(entry['project'], entry['configuration']) for entry in report['steps']]
                    == [(entry['project'], entry['configuration']) for entry in steps])
            report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            save_report()
            print('Full build passed: '+str(report['full_build_passed'])+'; '+str(destination), flush=True)
            return 0 if report['full_build_passed'] else 1
    except (OSError, RuntimeError, ValueError) as error:
        print('Build setup failed: '+str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
