#!/usr/bin/env python3
"""Compile/assemble original DX9 shaders without changing their shader models.

The Windows SDK FXC and D3DCompiler are developer prerequisites. No compiler
binary is bundled. Headers use DWORD tokens because the 2003 registration
code consumes unsigned-int arrays, whereas modern FXC /Fh emits BYTE arrays.
"""
import ctypes
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from build_modern_shaders import resolve_tool


def shader_header(data, symbol='g_shader_main', constant=True):
    if len(data) < 8 or len(data) % 4:
        raise ValueError('Compiler produced truncated DX9 shader bytecode')
    words = struct.unpack('<%dI' % (len(data)//4), data)
    if words[0] >> 16 not in (0xfffe, 0xffff) or words[-1] != 0x0000ffff:
        raise ValueError('Compiler output is not legacy DX9 shader bytecode')
    # fxc_prep.pl replaces the complete declaration line. Keep the brace on
    # its own line so the original registration generator retains it.
    return ('const ' if constant else '')+'DWORD '+symbol+'[] =\n{\n' + ''.join('    0x%08x,\n' % word for word in words) + '};\n'


def compiler_dll():
    override = os.environ.get('HL2_MODERN_D3DCOMPILER')
    if override:
        if not Path(override).is_file():
            raise ValueError('HL2_MODERN_D3DCOMPILER does not name a DLL')
        return override
    arch = 'x64' if struct.calcsize('P') == 8 else 'x86'
    kits = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'))/'Windows Kits/10'
    candidate = kits/'Redist/D3D'/arch/'d3dcompiler_47.dll'
    if candidate.is_file():
        return str(candidate)
    for directory in sorted((kits/'bin').glob('10.*'), reverse=True):
        candidate = directory/arch/'d3dcompiler_47.dll'
        if candidate.is_file():
            return str(candidate)
    raise ValueError('Windows SDK D3DCompiler DLL not found; set HL2_MODERN_D3DCOMPILER')


def blob_bytes(pointer):
    if not pointer:
        return b''
    table = ctypes.cast(pointer, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    get_pointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(table[3])
    get_size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(table[4])
    release = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(table[2])
    try:
        return ctypes.string_at(get_pointer(pointer), get_size(pointer))
    finally:
        release(pointer)


def assemble(source, defines=()):
    if os.name != 'nt':
        raise ValueError('Legacy shader assembly requires the Windows SDK')
    class Macro(ctypes.Structure):
        _fields_ = [('name', ctypes.c_char_p), ('value', ctypes.c_char_p)]
    entries = []
    for define in defines:
        name, _, value = define.partition('=')
        entries.append((name.encode('ascii'), (value or '1').encode('ascii')))
    macros = (Macro * (len(entries)+1))(*(Macro(*entry) for entry in entries), Macro(None, None))
    data = source.read_bytes()
    library = ctypes.WinDLL(compiler_dll())
    function = library.D3DAssemble
    function.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p,
                         ctypes.POINTER(Macro), ctypes.c_void_p, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
    function.restype = ctypes.c_long
    output, errors = ctypes.c_void_p(), ctypes.c_void_p()
    # Preserve the assembly's declared ps/vs version. Debug metadata is not
    # requested here; it is unrelated to the shader's executable instructions.
    status = function(data, len(data), os.fsencode(source.resolve()), macros,
                      ctypes.c_void_p(1), 0, ctypes.byref(output), ctypes.byref(errors))
    diagnostics = blob_bytes(errors)
    bytecode = blob_bytes(output)
    if diagnostics:
        print(diagnostics.rstrip(b'\0').decode('utf8', errors='replace'), file=sys.stderr)
    if status < 0:
        raise ValueError('Shader assembly failed (0x%08x)' % (status & 0xffffffff))
    return bytecode


def output_options(arguments):
    output, defines, rest = None, [], []
    index = 0
    while index < len(arguments):
        value = arguments[index]
        if value.lower() == '/fh':
            # Legacy psa9 uses bare /Fh to mean <input>.h; FXC /Fh has a
            # filename. Its explicit modern form is handled by the caller.
            rest.append(value)
        elif value.lower().startswith('/fh'):
            output = Path(value[3:])
        elif value.lower().startswith('/d') and len(value) > 2:
            defines.append(value[2:])
            rest.append(value)
        else:
            rest.append(value)
        index += 1
    return output, defines, rest


def compile_legacy_d3dx(source, arguments, library_path):
    """Compile the same profile with an explicitly selected legacy D3DX DLL.

    The SDK compiler cannot allocate vs_2_0 temporaries for some original
    lighting permutations. D3DX9_31 from Microsoft's signed June 2010 redist
    retains the earlier compiler. It is optional and is never installed or
    downloaded by this helper. Reject unfamiliar flags rather than silently
    dropping their meaning when trying this compiler.
    """
    if os.name != 'nt':
        raise ValueError('Legacy D3DX compilation requires Windows')
    if not Path(library_path).is_file():
        raise ValueError('HL2_MODERN_D3DX9 does not name a DLL')
    defines, entry, profile = [], 'main', None
    for value in arguments:
        option = value.lower()
        if option.startswith('/d') and len(value) > 2:
            name, _, definition = value[2:].partition('=')
            defines.append((name.encode('ascii'), (definition or '1').encode('ascii')))
        elif option.startswith('/e') and len(value) > 2:
            entry = value[2:]
        elif option.startswith('/t') and len(value) > 2:
            profile = value[2:]
        elif option != '/nologo':
            raise ValueError('Legacy D3DX fallback cannot preserve option: '+value)
    if profile is None:
        raise ValueError('Legacy D3DX fallback requires the original shader profile')
    class Macro(ctypes.Structure):
        _fields_ = [('name', ctypes.c_char_p), ('value', ctypes.c_char_p)]
    macros = (Macro * (len(defines)+1))(*(Macro(*value) for value in defines), Macro(None, None))
    library = ctypes.WinDLL(str(Path(library_path).resolve()))
    function = library.D3DXCompileShaderFromFileA
    function.argtypes = [ctypes.c_char_p, ctypes.POINTER(Macro), ctypes.c_void_p,
                         ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p),
                         ctypes.c_void_p]
    function.restype = ctypes.c_long
    output, errors = ctypes.c_void_p(), ctypes.c_void_p()
    # Flags=0 keeps the original full precision; /Gpp is never introduced.
    # D3DX supplies its standard local-file include handler when pInclude=NULL.
    status = function(str(source.resolve()).encode('mbcs'), macros, None,
                      entry.encode('ascii'), profile.encode('ascii'), 0,
                      ctypes.byref(output), ctypes.byref(errors), None)
    diagnostics = blob_bytes(errors)
    bytecode = blob_bytes(output)
    if diagnostics:
        print(diagnostics.rstrip(b'\0').decode('utf8', errors='replace'), file=sys.stderr)
    if status < 0:
        raise ValueError('Legacy D3DX shader compilation failed (0x%08x)' % (status & 0xffffffff))
    return bytecode


def register_overflow_only(result):
    errors = re.findall(r'\berror\s+(X\d+)\s*:', result.stdout+result.stderr, re.IGNORECASE)
    return bool(errors) and all(error.upper() == 'X4505' for error in errors)


def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments or arguments[0] not in ('assemble', 'compile'):
        print('Usage: legacy_shader_tool.py assemble|compile ORIGINAL_TOOL_ARGUMENTS', file=sys.stderr)
        return 2
    mode = arguments.pop(0)
    try:
        legacy_vertex = '--legacy-vertex8' in arguments
        legacy_pixel = '--legacy-pixel8' in arguments
        arguments = [value for value in arguments if value not in ('--legacy-vertex8','--legacy-pixel8')]
        output, defines, remaining = output_options(arguments)
        if not remaining:
            raise ValueError('Shader source is required')
        source = Path(remaining[-1])
        if not source.is_file():
            raise ValueError('Shader source not found: '+str(source))
        if mode == 'assemble':
            unsupported = [value for value in remaining[:-1]
                           if value.lower() not in ('/fh', '/zi', '/nologo', '-h', '-p') and not value.lower().startswith('/d')]
            if unsupported:
                raise ValueError('Unsupported assembly options: '+repr(unsupported))
            output = output or source.with_suffix('.h')
            bytecode = assemble(source, defines)
        else:
            compiler = os.environ.get('HL2_MODERN_FXC') or resolve_tool('fxc')
            if not compiler:
                raise ValueError('Windows SDK FXC not found; set HL2_MODERN_FXC')
            if output is None:
                raise ValueError('Legacy FXC header output must be specified as /FhFILE')
            with tempfile.TemporaryDirectory() as td:
                binary = Path(td)/'shader.bin'
                command = [compiler, *remaining[:-1], '/Fo'+str(binary), str(source)]
                result = subprocess.run(command, check=False, capture_output=True, text=True, errors='replace')
                if result.returncode and register_overflow_only(result) and '/Gis' not in command:
                    # Strict IEEE evaluation prevents the modern optimizer's
                    # reassociation from exhausting vs_2_0 temporaries in the
                    # original bumped-light shader. Retain both the declared
                    # shader model and full precision; report the retry.
                    print('FXC: retrying register-limited legacy shader with IEEE strictness (/Gis).')
                    command.insert(-1, '/Gis')
                    result = subprocess.run(command, check=False, capture_output=True, text=True, errors='replace')
                legacy = os.environ.get('HL2_MODERN_D3DX9')
                if result.returncode and register_overflow_only(result) and legacy:
                    print('FXC: retrying register-limited shader with the explicitly selected legacy D3DX compiler.')
                    bytecode = compile_legacy_d3dx(source, remaining[:-1], legacy)
                else:
                    print(result.stdout, end='')
                    print(result.stderr, end='', file=sys.stderr)
                    if result.returncode:
                        return result.returncode
                    bytecode = binary.read_bytes()
        output.parent.mkdir(parents=True, exist_ok=True)
        symbol = 'dwVertexShader' if legacy_vertex else 'dwPixelShader' if legacy_pixel else 'g_shader_main'
        output.write_text(shader_header(bytecode,symbol,not (legacy_vertex or legacy_pixel)), encoding='ascii')
        return 0
    except (OSError, ValueError, AttributeError) as exc:
        print('Legacy shader tool error: '+str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
