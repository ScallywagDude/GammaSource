//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hud_numericdisplay.h"
#include "hud.h"
#include "../modernize/hud_color_client.h"
#include "iclientmode.h"

#include <Color.h>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudNumericDisplay::CHudNumericDisplay(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iValue = 0;
	m_LabelText[0] = 0;
	m_iSecondaryValue = 0;
	m_bDisplayValue = true;
	m_bDisplaySecondaryValue = false;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
// HL2MODERN_2002_FONTS: use installed 2002 fonts when present, with stock fallback.
void CHudNumericDisplay::ApplySchemeSettings(vgui::IScheme *scheme)
{
    BaseClass::ApplySchemeSettings(scheme);
#ifdef HL2_CLIENT_DLL
    vgui::HFont numbers=scheme->GetFont("HudNumbers2",IsProportional());
    vgui::HFont glow=scheme->GetFont("HudNumbersGlow2",IsProportional());
    vgui::HFont label=scheme->GetFont("Default2",IsProportional());
    if(numbers) m_hNumberFont=numbers;
    if(glow) m_hNumberGlowFont=glow;
    if(label) m_hTextFont=label;
#endif
}

void CHudNumericDisplay::SetDisplayValue(int value)
{
	m_iValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetSecondaryValue(int value)
{
	m_iSecondaryValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetShouldDisplayValue(bool state)
{
	m_bDisplayValue = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetShouldDisplaySecondaryValue(bool state)
{
	m_bDisplaySecondaryValue = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetLabelText(const wchar_t *text)
{
	wcsncpy(m_LabelText, text, sizeof(m_LabelText) / sizeof(wchar_t));
	m_LabelText[(sizeof(m_LabelText) / sizeof(wchar_t)) - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: paints a number at the specified position
//-----------------------------------------------------------------------------
void CHudNumericDisplay::PaintNumbers(HFont font, int xpos, int ypos, int value)
{
	surface()->DrawSetTextFont(font);
	wchar_t unicode[6];
	swprintf(unicode, L"%d", value);
	// adjust the position to take into account 3 characters
	int charWidth = surface()->GetCharacterWidth(font, '0');
	if (value < 100)
	{
		xpos += charWidth;
	}
	if (value < 10)
	{
		xpos += charWidth;
	}

	surface()->DrawSetTextPos(xpos, ypos);
	for (wchar_t *ch = unicode; *ch != 0; ch++)
	{
		surface()->DrawUnicodeChar(*ch);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNumericDisplay::PaintBackground( void )
{
	float alpha = m_flAlphaOverride / 255.0f;
	Color bgColor = GetBgColor();
	const Color originalBackground=bgColor; // HL2MODERN_BACKGROUND_ALPHA
	bgColor[3] *= alpha;
	SetBgColor( bgColor );

	BaseClass::PaintBackground();
	SetBgColor(originalBackground);
}

//-----------------------------------------------------------------------------
// Purpose: renders the vgui panel
//-----------------------------------------------------------------------------
void CHudNumericDisplay::Paint()
{
	// HL2MODERN_THREE_COUNTERS: retain the 2002 fonts, amber and compact row.
#ifdef HL2_CLIENT_DLL
    const bool health=!Q_stricmp(GetName(),"HudHealth");
    const bool suit=!Q_stricmp(GetName(),"HudSuit");
    const bool energy=!Q_stricmp(GetName(),"HudSuitPower");
    if(health || suit || energy)
    {
        SetBounds((int)XRES(16+(suit?114:energy?228:0)),ScreenHeight()-(int)YRES(48),
            (int)XRES(108),(int)YRES(36));
        text_xpos=XRES(8); text_ypos=YRES(0);
        digit_xpos=XRES(38); digit_ypos=YRES(9);
    }
#endif
    float alpha = m_flAlphaOverride / 255.0f;
	Color fgColor = GetFgColor();
#ifdef HL2_CLIENT_DLL
    // Red caution animations remain readable; normal counters follow the palette.
    if (!(fgColor[0]>fgColor[1]*2 && fgColor[1]<80))
        fgColor=HL2ModernHudColor(Color(247,192,84,fgColor[3]));
#endif
	fgColor[3] *= alpha;
	// Do not write multiplied alpha back into the animated foreground.


	if (m_bDisplayValue)
	{
		// draw our numbers
		surface()->DrawSetTextColor(fgColor);
		PaintNumbers(m_hNumberFont, digit_xpos, digit_ypos, m_iValue);

		// draw the overbright blur
		for (float fl = m_flBlur; fl > 0.0f; fl -= 1.0f)
		{
			if (fl >= 1.0f)
			{
				PaintNumbers(m_hNumberGlowFont, digit_xpos, digit_ypos, m_iValue);
			}
			else
			{
				// draw a percentage of the last one
				Color col = fgColor;
				col[3] *= fl;
				surface()->DrawSetTextColor(col);
				PaintNumbers(m_hNumberGlowFont, digit_xpos, digit_ypos, m_iValue);
			}
		}
	}

	// total ammo
	if (m_bDisplaySecondaryValue)
	{
		//fgColor = m_Ammo2Color;
		//fgColor[3] *= alpha;
		surface()->DrawSetTextColor(fgColor);
		PaintNumbers(m_hSmallNumberFont, digit2_xpos, digit2_ypos, m_iSecondaryValue);
	}

	// draw our name
	//fgColor = m_TextColor;
	//fgColor[3] *= alpha;

	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(fgColor);
	surface()->DrawSetTextPos(text_xpos, text_ypos);
	for (wchar_t *wch = m_LabelText; *wch != 0; wch++)
	{
		surface()->DrawUnicodeChar(*wch);
	}
}



