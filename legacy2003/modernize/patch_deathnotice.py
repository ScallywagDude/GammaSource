#!/usr/bin/env python3
"""Avoid a null-inflictor dereference in 2003 multiplayer kill notices.

World/environment kills can carry a null inflictor; keep the preselected
"world" death label instead of dereferencing it. Idempotent source patch.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'game_shared/multiplay_gamerules.cpp'
raw=p.read_bytes(); crlf=b'\r\n' in raw
s=raw.decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_NULL_INFLICTOR' not in s:
    needle='''\t\t\telse
\t\t\t{
\t\t\t\tkiller_weapon_name = STRING( pInflictor->m_iClassname );
\t\t\t}
'''
    if s.count(needle)!=1:
        raise RuntimeError('Expected one DeathNotice world kill branch')
    replacement='''\t\t\t// HL2MODERN_NULL_INFLICTOR: some world kills have no inflictor.
\t\t\telse if ( pInflictor )
\t\t\t{
\t\t\t\tkiller_weapon_name = STRING( pInflictor->m_iClassname );
\t\t\t}
'''
    s=s.replace(needle,replacement)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin-1'))
    print('game_shared/multiplay_gamerules.cpp: null-inflictor guard installed')
else:
    assert s.count('HL2MODERN_NULL_INFLICTOR')==1
    print('game_shared/multiplay_gamerules.cpp: already patched')
