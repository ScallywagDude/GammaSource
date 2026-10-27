//========= GammaSource: ported from the 2003 code (cl_dll/hud_numericdisplay.cpp) ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hud_numericdisplay.h"
#include "hud.h"
#include "hud_color_client.h"
#include "beta_hud_geometry.h"
#include "iclientmode.h"
#include <string.h>

#include <Color.h>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CHudNumericDisplay::PaintLabel( void )
{
	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextColor( GetFgColor() );
	vgui::surface()->DrawSetTextPos( text_xpos, text_ypos );
	vgui::surface()->DrawUnicodeString( m_LabelText );
}

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudNumericDisplay::CHudNumericDisplay(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hBetaNumbers = 0;
	m_hBetaLabel = 0;
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
    // Distinct compact, unboxed beta typography. Create font handles once;
    // preserve the scheme fonts if the system face cannot be loaded.
    if(!m_hBetaNumbers) m_hBetaNumbers=surface()->CreateFont();
    if(!m_hBetaLabel) m_hBetaLabel=surface()->CreateFont();
    if(m_hBetaNumbers && !surface()->SetFontGlyphSet(m_hBetaNumbers,
        "Bahnschrift Condensed",34,700,0,0,vgui::ISurface::FONTFLAG_ANTIALIAS))
        m_hBetaNumbers=m_hNumberFont;
    if(m_hBetaLabel && !surface()->SetFontGlyphSet(m_hBetaLabel,
        "Bahnschrift Condensed",14,800,0,0,vgui::ISurface::FONTFLAG_ANTIALIAS))
        m_hBetaLabel=m_hTextFont;
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

// Only the four main numeric counters are redrawn as the screenshot's
// original unboxed strip. Other CHudNumericDisplay users are unaffected.
bool CHudNumericDisplay::IsBetaCounter()
{
    const char *name=GetName();
    return name && (!Q_stricmp(name,"HudHealth") || !Q_stricmp(name,"HudSuit") ||
        !Q_stricmp(name,"HudAmmo") || !Q_stricmp(name,"HudAmmoSecondary") ||
        !Q_stricmp(name,"HudSuitPower"));
}

// The beta counters sit at fixed beta positions. HUD animation scripts written
// for the retail layout (WeaponUsesClips, WeaponUsesSecondaryAmmo, ...) move
// and resize HudAmmo/HudAmmoSecondary for a moment on every weapon switch, so
// restore the beta bounds before the panel is drawn each frame. Colour and
// alpha animations are unaffected.
void CHudNumericDisplay::ApplyBetaCounterBounds()
{
    const char *name=GetName();
    const int kind=!Q_stricmp(name,"HudHealth")?0:!Q_stricmp(name,"HudSuit")?1:
        !Q_stricmp(name,"HudAmmoSecondary")?3:!Q_stricmp(name,"HudSuitPower")?4:2;
    const GammaBetaHud::Rect box=GammaBetaHud::CounterRect(ScreenWidth(),ScreenHeight(),kind);
    int x,y,w,h;
    GetBounds(x,y,w,h);
    if(x!=box.x||y!=box.y||w!=box.w||h!=box.h) SetBounds(box.x,box.y,box.w,box.h);
}

void CHudNumericDisplay::PaintTraverse(bool Repaint, bool allowForce)
{
#ifdef HL2_CLIENT_DLL
    if(IsBetaCounter()) ApplyBetaCounterBounds();
#endif
    BaseClass::PaintTraverse(Repaint,allowForce);
}

// 2002 bitmap HUD assets are defined by scripts/mod_textures.txt and drawn
// through the engine's existing CHudTexture atlas loader. No substitute art is
// generated; install the original sprites/hud1.vtf for authentic glyphs.
void CHudNumericDisplay::PaintBetaLabel(const char *sprite, const wchar_t *fallback,
    int x, int y, Color tint)
{
    CHudTexture *image=gHUD.GetIcon(sprite);
    if(image && image->Width()>0 && image->Height()>0)
    {
        image->DrawSelf(x,y,tint);
        return;
    }
    // A readable fallback if the original bitmap resources aren't installed.
    surface()->DrawSetTextFont(m_hBetaLabel?m_hBetaLabel:m_hTextFont);
    surface()->DrawSetTextColor(tint);
    surface()->DrawSetTextPos(x,y);
    for(const wchar_t *p=fallback;*p;++p) surface()->DrawUnicodeChar(*p);
}

void CHudNumericDisplay::PaintBetaValue(int x,int y,int value,int digits,
    int glyphWidth,int glyphHeight,Color tint)
{
    if(value<0)value=0;
    char number[16];
    Q_snprintf(number,sizeof(number),"%d",value);
    int len=(int)strlen(number);
    if(len>10) len=10;
    int columns=(len>digits)?len:digits;
    if(columns>10)columns=10;
    const int leading=columns-len;
    CHudTexture *faint=gHUD.GetIcon("number_0_faint");
    for(int column=0;column<columns;++column)
    {
        char iconName[24];
        const bool empty=column<leading;
        const char digit=empty?'0':number[column-leading];
        if(digit<'0'||digit>'9')continue;
        if(empty && !faint) continue;
        Q_snprintf(iconName,sizeof(iconName),"number_%c",digit);
        CHudTexture *image=empty?faint:gHUD.GetIcon(iconName);
        const int px=x+column*glyphWidth;
        if(image && image->Width()>0 && image->Height()>0)
        {
            Color drawColor=tint;
            if(empty)drawColor[3]=(drawColor[3]*150)/255;
            image->DrawSelf(px,y,glyphWidth,glyphHeight,drawColor);
        }
        else if(!empty)
        {
            // Keep values visible while users stage the authentic bitmap atlas.
            surface()->DrawSetTextFont(m_hBetaNumbers?m_hBetaNumbers:m_hNumberFont);
            surface()->DrawSetTextColor(tint);
            surface()->DrawSetTextPos(px,y);
            surface()->DrawUnicodeChar((wchar_t)digit);
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNumericDisplay::PaintBackground( void )
{
	if(IsBetaCounter())return; // no rounded counter panels
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
#ifdef HL2_CLIENT_DLL
    if(IsBetaCounter())
    {
        const char *name=GetName();
        const bool isHealth=!Q_stricmp(name,"HudHealth");
        const bool isSuit=!Q_stricmp(name,"HudSuit");
        const bool isSecondary=!Q_stricmp(name,"HudAmmoSecondary");
        const bool isEnergy=!Q_stricmp(name,"HudSuitPower");
        const int alpha=GammaBetaHud::Clamp((int)m_flAlphaOverride,0,255);
        Color tint=HL2ModernHudColor(isSecondary?Color(211,61,46,alpha):Color(233,208,111,alpha));
        if(!isSecondary)
        {
            PaintBetaLabel(isHealth?"health_label":isSuit?"battery_label":isEnergy?"energy_label":"ammo_label",
                isHealth?L"HEALTH":isSuit?L"HEV":isEnergy?L"ENERGY":L"AMMO",0,0,tint);
        }
        if(m_bDisplayValue)
        {
            // Original hud1: 20x28 bitmap glyphs, including faint leading zeros.
            const int columns=isSecondary?2:isHealth||isSuit||isEnergy?3:2;
            PaintBetaValue(0,isSecondary?23:17,m_iValue,columns,
                isSecondary?12:20,isSecondary?17:28,tint);
        }
        if(m_bDisplaySecondaryValue && !isSecondary)
        {
            // Early HUD uses an unboxed vertical ammo separator.
            const int split=62;
            CHudTexture *bar=gHUD.GetIcon("ammo_bar");
            if(bar && bar->Width()>0 && bar->Height()>0)
                bar->DrawSelf(split-3,20,9,28,tint);
            else
            {
                surface()->DrawSetColor(tint);
                surface()->DrawFilledRect(split,20,split+2,47);
            }
            PaintBetaValue(split+7,25,m_iSecondaryValue,3,12,17,tint);
        }
        return;
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



