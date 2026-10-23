#!/usr/bin/env python3
"""Apply conservative gamma-input and Fresnel stability fixes to legacy DX9 lighting.

This is a real change to the existing renderer's code paths, NOT a Vulkan/DX11
backend. Run repeatedly without duplicating patches.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def patch(rel, before, after, marker):
    p = root / rel
    raw = p.read_bytes()
    crlf = b'\r\n' in raw
    txt = raw.decode('latin-1').replace('\r\n','\n')
    if marker in txt:
        if after not in txt:
            raise RuntimeError('%s marker exists but patch content missing' % rel)
        print('%s: already patched' % rel)
        return
    if txt.count(before) != 1:
        raise RuntimeError('%s: expected exactly one original anchor, found %d' % (rel, txt.count(before)))
    txt = txt.replace(before, after)
    p.write_bytes(txt.replace('\n','\r\n').encode('latin-1') if crlf else txt.encode('latin-1'))
    print('%s: patched' % rel)

patch('materialsystem/colorspace.cpp',
      '#include "colorspace.h"\n',
      '#include "colorspace.h"\n// HL2MODERN_GAMMA_INPUT_GUARD\n#include "../modernize/lighting_parameters.h"\n',
      'HL2MODERN_GAMMA_INPUT_GUARD')
patch('materialsystem/colorspace.cpp',
      'void ColorSpace::SetGamma( float screenGamma, float texGamma, \n\t\t\t\t\t\t   float overbright, bool allowCheats, bool linearFrameBuffer )\n{',
      'void ColorSpace::SetGamma( float screenGamma, float texGamma, \n\t\t\t\t\t\t   float overbright, bool allowCheats, bool linearFrameBuffer )\n{\n\t// HL2MODERN_SANITIZED_GAMMA_TABLE_INPUTS\n\tHL2ModernLighting::SanitizeGammaParameters( screenGamma, texGamma, overbright );',
      'HL2MODERN_SANITIZED_GAMMA_TABLE_INPUTS')
patch('materialsystem/stdshaders/common_ps_fxc.h',
      '\tHALF fresnel = HALF_CONSTANT(1.0f) - dot( normal, eye );',
      '\t// HL2MODERN_BOUNDED_FRESNEL\n\tHALF fresnel = saturate( HALF_CONSTANT(1.0f) - dot( normal, eye ) );',
      'HL2MODERN_BOUNDED_FRESNEL')
patch('materialsystem/stdshaders/lightmappedgeneric_ps20.fxc',
      '\t\tHALF fresnel = 1.0 - dot( worldSpaceNormal, eyeVect );',
      '\t\t// HL2MODERN_BOUNDED_FRESNEL_LIGHTMAP\n\t\tHALF fresnel = saturate( 1.0 - dot( worldSpaceNormal, eyeVect ) );',
      'HL2MODERN_BOUNDED_FRESNEL_LIGHTMAP')
