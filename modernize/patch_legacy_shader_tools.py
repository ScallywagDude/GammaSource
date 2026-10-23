#!/usr/bin/env python3
"""Route legacy DX9 shader scripts to configurable tools and fail on errors."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def once(source, before, after):
    if after in source:
        return source
    if source.count(before) != 1:
        raise ValueError('Legacy shader tool patch target missing or ambiguous: '+before[:80])
    return source.replace(before, after, 1)


def restore(relative, source):
    if relative.endswith('fxc_prep.pl'):
        before = r'$cmd = "..\\..\\devtools\\bin\\fxc.exe ";'
        after = r'$cmd = ($ENV{"HL2_MODERN_FXC_COMMAND"} || "..\\..\\devtools\\bin\\fxc.exe") . " ";'
        if after not in source:
            if source.count(before) != 2:
                raise ValueError('FXC command targets missing or ambiguous')
            source = source.replace(before, after)
        source = once(source, '\t\tsystem "$cmd > shit.txt 2>&1";',
                      '\t\tunlink "tmpshader.h";\n\t\tsystem "$cmd > shit.txt 2>&1";\n'
                      '\t\tif ( $? != 0 ) { die "shader compiler failed: " . `type shit.txt`; }')
    elif relative.endswith('vsh_prep.pl'):
        source = once(source, r'$vsa = "..\\..\\devtools\\bin\\vsa9";',
                      r'$vsa = $ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} || "..\\..\\devtools\\bin\\vsa9";')
        source = once(source, r'$vsa = "..\\..\\devtools\\bin\\vsa8_1";',
                      r'$vsa = $ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} ? $ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} . " --legacy-vertex8" : "..\\..\\devtools\\bin\\vsa8_1";')
        source = once(source, '\t\t\t\t&TranslateErrorMessages( @vsaoutput );',
                      '\t\t\t\tif ( $? != 0 ) { die "vertex shader assembler failed: @vsaoutput"; }\n'
                      '\t\t\t\t&TranslateErrorMessages( @vsaoutput );')
    elif relative.endswith('psh_prep.pl'):
        source = once(source, r'$cmd = "..\\..\\devtools\\bin\\psa9 /Zi /Fh /nologo "',
                      r'$cmd = ($ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} || "..\\..\\devtools\\bin\\psa9") . " /Zi /Fh /nologo "')
        source = once(source, r'$cmd = "..\\..\\devtools\\bin\\psa8_1 -h -p "',
                      r'$cmd = ($ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} ? $ENV{"HL2_MODERN_ASSEMBLER_COMMAND"} . " --legacy-pixel8" : "..\\..\\devtools\\bin\\psa8_1") . " -h -p "')
        source = once(source, '\tsystem $cmd || die $!;',
                      '\tsystem $cmd;\n\tif ( $? != 0 ) { die "pixel shader assembler failed: $cmd\\n"; }')
    elif relative.endswith('common_vs_fxc.h'):
        before = 'float3 LinearToGamma( const float3 linear )'
        after = 'float3 LinearToGamma( const float3 linearValue )'
        if after not in source:
            if source.count(before) != 1:
                raise ValueError('Legacy gamma helper target missing or ambiguous')
            start = source.index(before)
            end = source.index('\n}', start)+2
            # Modern HLSL reserves linear as an interpolation keyword. Rename
            # this parameter only; keep both original gamma formulas intact.
            source = source[:start] + re.sub(r'\blinear\b', 'linearValue', source[start:end]) + source[end:]
    else:
        raise ValueError('Unexpected shader script: '+relative)
    return source


def main():
    for relative in ('devtools/bin/fxc_prep.pl', 'devtools/bin/vsh_prep.pl',
                     'devtools/bin/psh_prep.pl', 'materialsystem/stdshaders/common_vs_fxc.h'):
        path = ROOT/relative
        original = path.read_bytes()
        crlf = b'\r\n' in original
        source = original.decode('latin1').replace('\r\n', '\n')
        updated = restore(relative, source)
        if updated != source:
            path.write_bytes((updated.replace('\n', '\r\n') if crlf else updated).encode('latin1'))
    print('Legacy shader tool routing restored; original shader sources and bytecode profiles are retained.')


if __name__ == '__main__':
    main()
