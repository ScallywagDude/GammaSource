#!/usr/bin/env python3
"""Display an honest fallback label for an iconless HL2 weapon selection slot.

No textures or models are invented. The source's per-weapon icon is preferred,
with original aspect-preserving sizing, and the weapon's normal name is retained.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'cl_dll/hl2_hud/hud_weaponselection.cpp'
raw=p.read_bytes();crlf=b'\r\n' in raw
src=raw.decode('latin1').replace('\r\n','\n')
if 'HL2MODERN_MISSING_WEAPON_ICON' not in src:
    old='''				if (sprite)
				{
					if (pWeapon == pSelectedWeapon)'''
    new='''				bool bIconDrawn = false; // HL2MODERN_MISSING_WEAPON_ICON
				if (sprite)
				{
					if (pWeapon == pSelectedWeapon)'''
    assert src.count(old)==1
    src=src.replace(old,new,1)
    old='''						sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
					}
				}

				// draw text'''
    new='''						sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
						bIconDrawn = true;
					}
				}
				if (!bIconDrawn)
				{
					// Never draw a stale or unrelated icon for a prototype.
					// The existing weapon name appears below this placeholder.
					surface()->DrawSetTextColor(col);
					surface()->DrawSetTextFont(m_hTextFont);
					surface()->DrawSetTextPos(xpos + (largeBoxWide/2) - 12,
						ypos + (int)m_flIconYPos);
					const char *fallback = "WPN";
					for (const char *c = fallback; *c; ++c)
						surface()->DrawUnicodeChar((wchar_t)*c);
				}

				// draw text'''
    assert src.count(old)==1
    src=src.replace(old,new,1)
    p.write_bytes((src.replace('\n','\r\n') if crlf else src).encode('latin1'))
print('Source HUD now labels weapons whose sprites are missing or invalid.')
