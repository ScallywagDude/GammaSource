#!/usr/bin/env python3
"""Validate/compile experimental postprocess shaders when external SDK tools exist.

--plan prints exact jobs without requiring optional compiler installations.
--compile fails on unavailable tools; it never silently claims success.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
SRC = ROOT / 'shaders'


def sdk_tool_dirs():
    """Installed Windows SDK binaries, newest version first; no installation."""
    if os.name != 'nt':
        return []
    roots=[]
    if os.environ.get('WindowsSdkDir'):
        roots.append(Path(os.environ['WindowsSdkDir']))
    program_files=os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)')
    roots.append(Path(program_files)/'Windows Kits'/'10')
    directories=[]
    for sdk in dict.fromkeys(roots):
        binary=sdk/'bin'
        if not binary.is_dir():continue
        versions=[p for p in binary.iterdir() if p.is_dir() and re.fullmatch(r'\d+(?:\.\d+)+',p.name)]
        versions.sort(key=lambda p:tuple(int(part) for part in p.name.split('.')),reverse=True)
        for version in versions:
            directories.extend([version/'x64',version/'x86'])
        directories.extend([binary/'x64',binary/'x86'])
    return directories


def resolve_tool(name, tool_dirs=()):
    """Prefer explicit directories, then PATH, then installed Windows SDKs."""
    names=(name+'.exe',name) if os.name=='nt' else (name,name+'.exe')
    def in_directory(directory):
        for filename in names:
            path=Path(directory)/filename
            if path.is_file():return str(path.resolve())
        return None
    for directory in tool_dirs:
        found=in_directory(directory)
        if found:return found
    found=shutil.which(name)
    if found:return found
    for directory in sdk_tool_dirs():
        found=in_directory(directory)
        if found:return found
    return None

def jobs(out):
    h = SRC / 'hlsl' / 'fullscreen_tonemap.hlsl'
    yield 'Direct3D 11 VS', 'fxc', ['/nologo', '/T', 'vs_5_0', '/E', 'VSMain', '/Fo', str(out / 'tonemap_d3d11_vs.dxbc'), str(h)]
    yield 'Direct3D 11 PS', 'fxc', ['/nologo', '/T', 'ps_5_0', '/E', 'PSMain', '/Fo', str(out / 'tonemap_d3d11_ps.dxbc'), str(h)]
    yield 'Direct3D 12 VS', 'dxc', ['-T', 'vs_6_0', '-E', 'VSMain', '-Fo', str(out / 'tonemap_d3d12_vs.dxil'), str(h)]
    yield 'Direct3D 12 PS', 'dxc', ['-T', 'ps_6_0', '-E', 'PSMain', '-Fo', str(out / 'tonemap_d3d12_ps.dxil'), str(h)]
    for stage in ('vert', 'frag'):
        gl = SRC / 'glsl330' / ('fullscreen_tonemap.' + stage)
        vk = SRC / 'vulkan450' / ('fullscreen_tonemap.' + stage)
        yield 'OpenGL 3.3 ' + stage, 'glslangValidator', ['-S', stage, str(gl)]
        yield 'Vulkan ' + stage, 'glslangValidator', ['-V', '-S', stage, '-o', str(out / ('tonemap_vulkan_' + stage + '.spv')), str(vk)]

def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    action = p.add_mutually_exclusive_group(required=True)
    action.add_argument('--plan', action='store_true')
    action.add_argument('--compile', action='store_true')
    p.add_argument('--out-dir', type=Path, default=ROOT / 'shader_artifacts')
    p.add_argument('--tool-dir', type=Path, action='append', default=[],
                   help='Compiler directory override, repeatable; installed Windows SDK binaries are also discovered')
    args = p.parse_args(argv)
    planned = list(jobs(args.out_dir.resolve()))
    if args.plan:
        for name, tool, arg in planned:
            print('%-22s: %-18s [%s] %s' % (name, tool,
                  'available: '+resolve_tool(tool,args.tool_dir) if resolve_tool(tool,args.tool_dir) else 'not installed', ' '.join(arg)))
        return 0
    args.out_dir.mkdir(parents=True, exist_ok=True)
    failures = 0
    for name, tool, arg in planned:
        path = resolve_tool(tool,args.tool_dir)
        if not path:
            print('%s: missing required compiler %s' % (name, tool), file=sys.stderr)
            failures += 1
            continue
        print('Compiling', name, flush=True)
        proc = subprocess.run([path] + arg, check=False)
        if proc.returncode:
            print('%s: shader compiler exited with status %d' % (name, proc.returncode), file=sys.stderr)
            failures += 1
    print('%d/%d shader compile jobs completed' % (len(planned)-failures, len(planned)))
    return 1 if failures else 0

if __name__ == '__main__':
    sys.exit(main())
