#!/usr/bin/env python3
"""HL1 bucket arrangement adapted to the original Source HUD and weapon icons.

Behavior reference: https://github.com/ValveSoftware/halflife/blob/master/cl_dll/ammo.cpp
This is a new Source/VGUI implementation; it does not copy GoldSrc source text.
"""
from pathlib import Path
from restore_hopwire_combat import method
ROOT=Path(__file__).resolve().parents[1]
def restore(source):
    if 'HL2MODERN_BUCKET_LOCALIZATION' in source:return source
    if '#include "../../modernize/hl1_weapon_buckets.h"' not in source:
        source=source.replace('#include "../../modernize/weapon_hud_layout.h"','#include "../../modernize/weapon_hud_layout.h"\n#include "../../modernize/hl1_weapon_buckets.h"',1)
    if '#include <vgui/ILocalize.h>' not in source:source=source.replace('#include <vgui/IScheme.h>','#include <vgui/IScheme.h>\n#include <vgui/ILocalize.h>',1)
    body=(ROOT/'modernize/overlays/hl1_weapon_bucket_paint.inc').read_text()
    source=method(source,'void CHudWeaponSelection::Paint()',body)
    source=source.replace('Color numberColor = m_NumberColor;', 'Color numberColor = gHUD.m_clrNormal;')
    source=source.replace('if ( iSlot > MAX_WEAPON_SLOTS )','if ( iSlot<0 || iSlot>=MAX_WEAPON_SLOTS )')
    return source
def main():
    path=ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp';raw=path.read_bytes()
    source=raw.decode('latin1').replace('\r\n','\n');updated=restore(source)
    if source!=updated:path.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in raw else updated).encode('latin1'))
    print('HL1 bucket weapon selection applied with existing 2002 colors and fitted original icons.')
if __name__=='__main__':main()
