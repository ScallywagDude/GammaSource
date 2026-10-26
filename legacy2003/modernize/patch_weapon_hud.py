#!/usr/bin/env python3
"""Give weapon HUD icons a bounded aspect-correct drawing area (HL2 only)."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'cl_dll/hl2_hud/hud_weaponselection.cpp'
raw=p.read_bytes();crlf=b'\r\n' in raw
s=raw.decode('latin1').replace('\r\n','\n')
if 'HL2MODERN_WEAPON_HUD_FIT' not in s:
    needle='#include "iclientmode.h"'; assert s.count(needle)==1
    s=s.replace(needle,needle+'\n#include "../../modernize/weapon_hud_layout.h"',1)
    before='''				if ( pWeapon->GetSpriteActive() )
				{
					if (pWeapon == pSelectedWeapon)
					{
						col[3] = m_flSelectionAlphaOverride;
					}
					pWeapon->GetSpriteActive()->DrawSelf( xpos + m_flIconXPos, ypos + m_flIconYPos, col );
				}'''
    after='''				// HL2MODERN_WEAPON_HUD_FIT: each weapon uses its script-provided
				// sprite. Never stretch a wide icon to a square or draw over text.
				const CHudTexture *sprite = pWeapon->GetSpriteActive();
				if (!sprite) sprite = pWeapon->GetSpriteInactive();
				if (sprite)
				{
					if (pWeapon == pSelectedWeapon)
						col[3] = m_flSelectionAlphaOverride;
					const int availableW=(int)(largeBoxWide-2*m_flIconXPos);
					const int availableH=(int)(m_flTextYPos-m_flIconYPos-2);
					HL2ModernHud::Rect fitted;
					if(HL2ModernHud::FitIcon(sprite->Width(),sprite->Height(),
						(int)(xpos+m_flIconXPos),(int)(ypos+m_flIconYPos),
						availableW,availableH,fitted))
					{
						sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
					}
				}'''
    assert s.count(before)==1
    s=s.replace(before,after,1)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))
print('HL2 HUD icons now fit without aspect distortion.')
