#!/usr/bin/env python3
"""Restore the disabled owner-bound remote manhack using existing 2003 art.

Templates contain the new source implementation. The original source files'
copyright/preamble bytes are retained, as are their checkout line endings.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MARKER='HL2MODERN_REMOTE_MANHACK'


def main():
    for filename in ('player_manhack.cpp','weapon_manhack.cpp','player_manhack.h'):
        path=ROOT/'dlls/hl2_dll'/filename
        template=(ROOT/'modernize/overlays'/filename).read_bytes().decode('latin1').replace('\r\n','\n')
        original=path.read_bytes() if path.exists() else b''
        current=original.decode('latin1').replace('\r\n','\n')
        # Preserve subsequent compiler-only adjustments on an already-restored
        # tree. New template revisions should use an explicit migration step.
        if MARKER in current:
            continue
        prefix=current.split('#include',1)[0] if filename.endswith('.cpp') else ''
        updated=prefix+template
        path.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in original else updated).encode('latin1'))
    client=ROOT/'cl_dll/hl2_hud/c_weapon__stubs_hl2.cpp'
    original=client.read_bytes()
    source=original.decode('latin1').replace('\r\n','\n')
    if 'STUB_WEAPON_CLASS( player_manhack,' not in source:
        source=source.replace('#include "c_weapon__stubs.h"','#include "c_weapon__stubs.h"\n#include "c_basecombatcharacter.h"',1)
        source+='\n// HL2MODERN_REMOTE_MANHACK: network the original remote camera/body.\nSTUB_WEAPON_CLASS( player_manhack, Player_Manhack, C_BaseCombatCharacter );\n'
        client.write_bytes((source.replace('\n','\r\n') if b'\r\n' in original else source).encode('latin1'))
    player=ROOT/'dlls/player.cpp'
    original=player.read_bytes()
    source=original.decode('latin1').replace('\r\n','\n')
    include='#include "hl2_dll/player_manhack.h"'
    if include not in source:
        source=source.replace('#include "cbase.h"','#include "cbase.h"\n#ifdef HL2_DLL\n'+include+'\n#endif',1)
    if 'HL2ModernManhackPlayerCommand(this, ucmd);' not in source:
        needle='\tPlayerMove()->RunCommand(this, ucmd, moveHelper);'
        if source.count(needle)!=1:raise ValueError('player command hook target missing or ambiguous')
        source=source.replace(needle,'#ifdef HL2_DLL\n\tHL2ModernManhackPlayerCommand(this, ucmd);\n#endif\n'+needle,1)
    updated=(source.replace('\n','\r\n') if b'\r\n' in original else source).encode('latin1')
    if updated!=original:player.write_bytes(updated)
    print('Original remote manhack control path restored; linked build/gameplay validation required.')


if __name__=='__main__':main()
