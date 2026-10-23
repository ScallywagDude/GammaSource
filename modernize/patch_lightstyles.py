#!/usr/bin/env python3
"""Fix stale lightstyle cache invalidation and opt-in interpolated lighting.

Reapplicable against original 2003 engine or an already patched project.
Does not add a render backend or change default lightstyle animation.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]

# Both the full update and incremental update need bounded copies; use a
# distinct marker on the first copy so the script is repeatable.
rel = 'engine/cl_parse.cpp'
p = root / rel
raw = p.read_bytes(); use_crlf = b'\r\n' in raw
code = raw.decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_BOUNDED_LIGHTSTYLE_FROM_NETWORK' not in code:
    before = 'Q_strcpy (cl_lightstyle[i].map,  MSG_ReadString());'
    if code.count(before) != 2:
        raise RuntimeError(f'{rel}: expected both original parse paths, found {code.count(before)}')
    code = code.replace(before,
        'Q_strncpy (cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));')
    anchor='void CL_Parse_LightStyle( void )\n{'
    if code.count(anchor) != 1: raise RuntimeError('LightStyle entry not found')
    code = code.replace(anchor, '// HL2MODERN_BOUNDED_LIGHTSTYLE_FROM_NETWORK\n' + anchor)
    p.write_bytes((code.replace('\n','\r\n') if use_crlf else code).encode('latin-1'))
    print(f'{rel}: bounded both network paths')
else:
    assert code.count('Q_strncpy (cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));') == 2
    print(f'{rel}: already patched')

rel='engine/gl_rlight.cpp'
p=root/rel
raw=p.read_bytes(); use_crlf=b'\r\n' in raw
code=raw.decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_SMOOTH_LIGHTSTYLES' not in code:
    anchor='#include "gl_lightmap.h"'
    assert code.count(anchor)==1
    code=code.replace(anchor,anchor+'\n#include "../modernize/lightstyle_policy.h"')
    start=code.index('void R_AnimateLight (void)\n{')
    end=code.index('\n/*\n=============================================================================',start)
    replacement='''// HL2MODERN_SMOOTH_LIGHTSTYLES
// Interpolate the original 10 Hz patterns when requested. Off by default.
ConVar r_smooth_lightstyles( "r_smooth_lightstyles", "0", FCVAR_ARCHIVE,
    "Interpolate animated lightstyles between 10 Hz samples (0=2003 behavior)." );

void R_AnimateLight (void)
{
    const double now = cl.gettime();
    const bool smooth = r_smooth_lightstyles.GetBool();
    for ( int j = 0; j < MAX_LIGHTSTYLES; ++j )
    {
        const int brightness = HL2ModernLightstyles::Sample(
            cl_lightstyle[j].map, cl_lightstyle[j].length, now, smooth );
        if ( d_lightstylevalue[j] != brightness )
        {
            // Also handles a style being cleared. The original engine reset
            // its intensity to 256 but forgot to invalidate cached lightmaps.
            d_lightstylevalue[j] = brightness;
            d_lightstyleframe[j] = r_framecount;
        }
    }
}
'''
    code=code[:start]+replacement+code[end:]
    p.write_bytes((code.replace('\n','\r\n') if use_crlf else code).encode('latin-1'))
    print(f'{rel}: lightstyle cache fix + opt-in interpolation')
else:
    assert 'd_lightstyleframe[j] = r_framecount;' in code
    print(f'{rel}: already patched')
