"""Hand fixes for the client files ported from the 2003 cl_dll (exact text or re:regex)."""
import re
from pathlib import Path

D = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game\client\gamma")

FIX = {
    'c_extinguisher.cpp': [
        ("SetupEntityRenderHandle( RENDER_GROUP_TRANSLUCENT_ENTITY );", "ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_TRANSLUCENT_ENTITY );"),
        ("CBeamSeg\t\tseg;", "BeamSeg_t\t\tseg;"),
        ("beamDraw.Start( numPoints, pMat );", "CMatRenderContextPtr pRenderContext( materials );\n\tbeamDraw.Start( pRenderContext, numPoints, pMat );"),
    ],
    # 2018 follows bones with SetParent( entity, attachment ) on the server; the 2003 draw override wrote
    # into the old bone cache, which no longer exists
    # c_combineguard.cpp is hand-rewritten for the 2018 particle API; do not re-port it
    'c_weapon_sticklauncher.cpp': [
        (r're:\tint InternalDrawModel\( int flags \)\n\t\{.*?return BaseClass::InternalDrawModel\( flags \);\n\t\}\n', ""),
    ],
}

import sys
only = set(sys.argv[1:])
for name, fixes in FIX.items():
    if only and name not in only:
        continue
    p = D / name
    if not p.exists():
        continue
    t = p.read_text(encoding='latin-1').replace('\r\n', '\n')
    for old, new in fixes:
        if old.startswith('re:'):
            t, n = re.subn(old[3:], new, t, flags=re.S)
            if not n:
                print('  regex fix not applied in %s: %r' % (name, old[:60]))
            continue
        if old in t:
            t = t.replace(old, new)
        elif not new or new not in t:
            print('  fix not applied in %s: %r' % (name, old[:60]))
    p.write_text(t, encoding='latin-1')
print('client fixups done')
