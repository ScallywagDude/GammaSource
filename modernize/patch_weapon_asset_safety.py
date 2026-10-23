#!/usr/bin/env python3
"""Harden legacy weapon-script slot indexes and HUD sprite atlas metadata.

Idempotent patch over unmodified 2003 files. No weapon models or artwork are
invented. The HUD icon sizing patch remains a separate operation.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def patch(rel, before, after, marker):
    path=ROOT/rel
    raw=path.read_bytes()
    crlf=b'\r\n' in raw
    text=raw.decode('latin1').replace('\r\n','\n')
    if marker in text:
        if marker=='HL2MODERN_HUD_SPRITE_BOUNDS' and 'HL2MODERN_HUD_LEGACY_RESOLUTION' in text:
            # v18 reads coordinates from a selected legacy resolution node;
            # the original bounded copy/rectangle safeguards still apply.
            for required in ('HL2ModernHud::SelectSpriteData(pTemp)',
                             'HL2ModernHud::SpriteMaterialName(texture,normalizedTexture,sizeof(normalizedTexture),spriteData!=pTemp)',
                             'strlen(shortName) >= sizeof(((CHudTexture *)0)->szShortName)',
                             'strlen(texture) >= sizeof(((CHudTexture *)0)->szTextureFile)',
                             'wide > 8192-left || tall > 8192-top',
                             'Q_strncpy( tex->szTextureFile, texture, sizeof(tex->szTextureFile) )'):
                assert required in text,(rel,'v18 guard missing: '+required)
            return
        assert after in text, (rel, 'marker present but required guard missing')
        return
    assert text.count(before)==1, (rel, 'expected exactly one original block')
    changed=text.replace(before,after,1)
    path.write_bytes((changed.replace('\n','\r\n') if crlf else changed).encode('latin1'))

patch('cl_dll/hud.cpp', '''				CHudTexture *tex = new CHudTexture();

				// Key Name is the sprite name
				strcpy( tex->szShortName, pTemp->GetName() );
				strcpy( tex->szTextureFile, pTemp->GetString( "file" ) );
				tex->rc.left	= pTemp->GetInt( "x", 0 );
				tex->rc.top		= pTemp->GetInt( "y", 0 );
				tex->rc.right	= pTemp->GetInt( "width", 0 )	+ tex->rc.left;
				tex->rc.bottom	= pTemp->GetInt( "height", 0 )	+ tex->rc.top;

				list.Insert( tex->szShortName, tex );''', '''				// HL2MODERN_HUD_SPRITE_BOUNDS: do not overflow the fixed
				// CHudTexture buffers or accept malformed atlas coordinates.
				// A bad sprite is ignored, leaving other weapon icons usable.
				const char *shortName = pTemp->GetName();
				const char *texture = pTemp->GetString( "file", "" );
				const int left = pTemp->GetInt( "x", 0 );
				const int top = pTemp->GetInt( "y", 0 );
				const int wide = pTemp->GetInt( "width", 0 );
				const int tall = pTemp->GetInt( "height", 0 );
				// 8192 is a conservative upper limit for legacy atlas rectangles;
				// test before addition so large input cannot overflow an int.
				if ( !shortName || !*shortName || !texture || !*texture ||
					strlen(shortName) >= sizeof(((CHudTexture *)0)->szShortName) ||
					strlen(texture) >= sizeof(((CHudTexture *)0)->szTextureFile) ||
					left < 0 || top < 0 || wide <= 0 || tall <= 0 ||
					left > 8192 || top > 8192 || wide > 8192-left || tall > 8192-top )
				{
					pTemp = pTemp->GetNextKey();
					continue;
				}
				CHudTexture *tex = new CHudTexture();
				Q_strncpy( tex->szShortName, shortName, sizeof(tex->szShortName) );
				Q_strncpy( tex->szTextureFile, texture, sizeof(tex->szTextureFile) );
				tex->rc.left = left;
				tex->rc.top = top;
				tex->rc.right = left + wide;
				tex->rc.bottom = top + tall;
				list.Insert( tex->szShortName, tex );''', 'HL2MODERN_HUD_SPRITE_BOUNDS')

patch('game_shared/weapon_parse.cpp', '''	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );''', '''	// HL2MODERN_WEAPON_SLOT_BOUNDS: scripts can contain cut weapon slots
	// outside the fixed HUD inventory arrays. Clamp before anyone indexes.
	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );
	if ( iSlot < 0 ) iSlot = 0;
	if ( iSlot >= MAX_WEAPON_SLOTS ) iSlot = MAX_WEAPON_SLOTS-1;
	if ( iPosition < 0 ) iPosition = 0;
	if ( iPosition >= MAX_WEAPON_POSITIONS ) iPosition = MAX_WEAPON_POSITIONS-1;''', 'HL2MODERN_WEAPON_SLOT_BOUNDS')
print('HUD sprite strings/rectangles and weapon script slots bounded.')
