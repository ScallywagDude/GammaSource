"""Hand fixes for the client files ported from the 2003 cl_dll (exact text or re:regex)."""
import re
from pathlib import Path

D = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game\client\gamma")
C = D.parent  # files that replace 2018 ones in place (hud_numericdisplay.h)

FIX = {
    'c_extinguisher.cpp': [
        ("SetupEntityRenderHandle( RENDER_GROUP_TRANSLUCENT_ENTITY );", "ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_TRANSLUCENT_ENTITY );"),
        ("CBeamSeg\t\tseg;", "BeamSeg_t\t\tseg;"),
        ("beamDraw.Start( numPoints, pMat );", "CMatRenderContextPtr pRenderContext( materials );\n\tbeamDraw.Start( pRenderContext, numPoints, pMat );"),
    ],
    # 2018 follows bones with SetParent( entity, attachment ) on the server; the 2003 draw override wrote
    # into the old bone cache, which no longer exists
    # c_combineguard.cpp is hand-rewritten for the 2018 particle API; do not re-port it
    # the 2018 CHud::DrawIconProgressBar takes a separate "empty" icon; keep the 2003 look
    # (the empty part is the same icon at a quarter brightness)
    'hud_quickinfo.cpp': [
        (r're:(#include "tier0/memdbgon\.h"\n)', "\\1\n"
         "static void DrawIconProgressBar2003( int x, int y, CHudTexture *icon, float percentage, Color &clr, int type )\n"
         "{\n\tif ( icon == NULL )\n\t\treturn;\n"
         "\tpercentage = clamp( percentage, 0.0f, 1.0f );\n"
         "\tint height = icon->Height(), width = icon->Width();\n"
         "\tColor darkClr = clr;\n\tdarkClr[0] /= 4;\n\tdarkClr[1] /= 4;\n\tdarkClr[2] /= 4;\n"
         "\tif ( type == CHud::HUDPB_VERTICAL )\n\t{\n\t\tint barOfs = height * percentage;\n"
         "\t\ticon->DrawSelfCropped( x, y, 0, 0, width, barOfs, darkClr );\n"
         "\t\ticon->DrawSelfCropped( x, y + barOfs, 0, barOfs, width, height - barOfs, clr );\n\t}\n"
         "\telse\n\t{\n\t\tint barOfs = width * percentage;\n"
         "\t\ticon->DrawSelfCropped( x, y, 0, 0, barOfs, height, darkClr );\n"
         "\t\ticon->DrawSelfCropped( x + barOfs, y, barOfs, 0, width - barOfs, height, clr );\n\t}\n}\n"),
        (r're:gHUD\.DrawIconProgressBar\(', "DrawIconProgressBar2003("),
    ],
    # the 2018 hud_basetimer.cpp calls PaintLabel; keep the 2018 entry point on the 2003 class
    '../hud_numericdisplay.h': [ (r're:(\tvirtual void Paint\(\);)', '\\1\n\tvirtual void PaintLabel();') ],
    'hud_numericdisplay.cpp': [ (r're:(#include "tier0/memdbgon\.h"\n)', '\\1\nvoid CHudNumericDisplay::PaintLabel( void )\n{\n\tvgui::surface()->DrawSetTextFont( m_hTextFont );\n\tvgui::surface()->DrawSetTextColor( GetFgColor() );\n\tvgui::surface()->DrawSetTextPos( text_xpos, text_ypos );\n\tvgui::surface()->DrawUnicodeString( m_LabelText );\n}\n') ],
    # the 2018 hud_damageindicator hooks the Damage message itself
    'hud_health.cpp': [ (r're:[^\n]*HudDamageIndicator_MsgFunc_Damage\( pszName, iSize, pbuf \);[^\n]*\n', '') ],
    'c_weapon_sticklauncher.cpp': [
        (r're:\tint InternalDrawModel\( int flags \)\n\t\{.*?return BaseClass::InternalDrawModel\( flags \);\n\t\}\n', ""),
    ],
}

import sys
only = set(sys.argv[1:])
for name, fixes in FIX.items():
    if only and name not in only:
        continue
    p = (C / name[3:]) if name.startswith('../') else (D / name)
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
