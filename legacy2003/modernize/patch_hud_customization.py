#!/usr/bin/env python3
"""2002 HUD palette, archived RGB options and numeric sprint Energy counter."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def patch(rel, marker, edits):
    p=ROOT/rel; raw=p.read_bytes(); crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n')
    if marker in s:return
    for old,new in edits:
        if s.count(old)!=1:raise RuntimeError(f'{rel}: expected one {old[:70]!r}, got {s.count(old)}')
        s=s.replace(old,new,1)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))
patch('gameui/CreateMultiplayerGameServerPage.cpp','HL2MODERN_CONST_MAP_SEARCH',[
('char *str = strstr(filename, "maps");','const char *str = strstr(filename, "maps"); // HL2MODERN_CONST_MAP_SEARCH')])
patch('gameui/GameConsoleDialog.cpp','HL2MODERN_CONST_COMPLETION_SEARCH',[
('char *space = strstr( text, " " );','const char *space = strstr( text, " " ); // HL2MODERN_CONST_COMPLETION_SEARCH')])
patch('gameui/CreateMultiplayerGameServerPage.cpp','HL2MODERN_MUTABLE_MAP_SUFFIX',[
('const char *str = strstr(filename, "maps");','const char *mapStart = strstr(filename, "maps");'),
('if (str)\n\t\t{\n\t\t\tstrncpy(mapname, str + 5,','if (mapStart)\n\t\t{\n\t\t\tstrncpy(mapname, mapStart + 5,'),
('str = strstr(mapname, ".bsp");','mapname[sizeof(mapname)-1]=0;\n\t\tchar *str = strstr(mapname, ".bsp"); // HL2MODERN_MUTABLE_MAP_SUFFIX')])
patch('gameui/OptionsDialog.cpp','HL2MODERN_HUD_OPTIONS',[
('#include "OptionsSubAdvanced.h"','#include "OptionsSubAdvanced.h"\n#include "OptionsSubHUD.h" // HL2MODERN_HUD_OPTIONS'),
('AddPage(new COptionsSubAdvanced(this), "#GameUI_Advanced");','AddPage(new COptionsSubAdvanced(this), "#GameUI_Advanced");\n\tif (HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()))\n\t\tAddPage(new COptionsSubHUD(this), "HUD");')])
patch('cl_dll/hud.cpp','HL2MODERN_HUD_PALETTE',[
('#include "hud_macros.h"','#include "hud_macros.h"\n#include "../modernize/hud_color_client.h"\n// HL2MODERN_HUD_PALETTE: archived options are available to GameUI.\nConVar hud_color_custom("hud_color_custom","0",FCVAR_ARCHIVE,"Override the HUD foreground color");\nConVar hud_color_r("hud_color_r","247",FCVAR_ARCHIVE,"HUD red channel",true,0,true,255);\nConVar hud_color_g("hud_color_g","192",FCVAR_ARCHIVE,"HUD green channel",true,0,true,255);\nConVar hud_color_b("hud_color_b","84",FCVAR_ARCHIVE,"HUD blue channel",true,0,true,255);\nstatic Color s_HudSchemeNormal(247,192,84,255);'),
('\tm_clrNormal = scheme->GetColor( "Normal", Color( 255, 208, 64 ,255 ) );','\tm_clrNormal = scheme->GetColor( "Normal", Color( 255, 208, 64 ,255 ) );\n#ifdef HL2_CLIENT_DLL\n\tm_clrNormal = scheme->GetColor("OldFgColor",Color(247,192,84,255));\n#endif\n\ts_HudSchemeNormal=m_clrNormal;\n\tm_clrNormal=HL2ModernHudColor(s_HudSchemeNormal);'),
('void CHud::UpdateHud( bool bActive )\n{','void CHud::UpdateHud( bool bActive )\n{\n\tm_clrNormal=HL2ModernHudColor(s_HudSchemeNormal);')])
patch('cl_dll/hud_numericdisplay.cpp','HL2MODERN_THREE_COUNTERS',[
('#include "hud_numericdisplay.h"','#include "hud_numericdisplay.h"\n#include "hud.h"\n#include "../modernize/hud_color_client.h"'),
('\tint alpha = m_flAlphaOverride / 255;','\tfloat alpha = m_flAlphaOverride / 255.0f;'),
('\tfloat alpha = m_flAlphaOverride / 255;','''\t// HL2MODERN_THREE_COUNTERS: retain the 2002 fonts, amber and compact row.
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
    float alpha = m_flAlphaOverride / 255.0f;'''),
('\tColor fgColor = GetFgColor();','''\tColor fgColor = GetFgColor();
#ifdef HL2_CLIENT_DLL
    // Red caution animations remain readable; normal counters follow the palette.
    if (!(fgColor[0]>fgColor[1]*2 && fgColor[1]<80))
        fgColor=HL2ModernHudColor(Color(247,192,84,fgColor[3]));
#endif'''),
('\tSetFgColor( fgColor );','\t// Do not write multiplied alpha back into the animated foreground.\n'),
('surface()->DrawSetTextColor(GetFgColor());','surface()->DrawSetTextColor(fgColor);'),
('Color col = GetFgColor();','Color col = fgColor;')])
patch('cl_dll/hl2_hud/hud_battery.cpp','HL2MODERN_HEV_LABEL',[
('SetLabelText(L"SUIT");','SetLabelText(L"HEV SUIT"); // HL2MODERN_HEV_LABEL')])
patch('cl_dll/hud_numericdisplay.cpp','HL2MODERN_BACKGROUND_ALPHA',[
('Color bgColor = GetBgColor();','Color bgColor = GetBgColor();\n\tconst Color originalBackground=bgColor; // HL2MODERN_BACKGROUND_ALPHA'),
('BaseClass::PaintBackground();','BaseClass::PaintBackground();\n\tSetBgColor(originalBackground);')])
patch('cl_dll/hud_numericdisplay.h','HL2MODERN_2002_FONTS',[
('virtual void PaintBackground( void );','virtual void PaintBackground( void );\n\tvirtual void ApplySchemeSettings(vgui::IScheme *scheme); // HL2MODERN_2002_FONTS')])
patch('cl_dll/hud_numericdisplay.cpp','HL2MODERN_2002_FONTS',[
('#include <vgui/ISystem.h>','#include <vgui/ISystem.h>\n#include <vgui/IScheme.h>'),
('void CHudNumericDisplay::SetDisplayValue(int value)','''// HL2MODERN_2002_FONTS: use installed 2002 fonts when present, with stock fallback.
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

void CHudNumericDisplay::SetDisplayValue(int value)''')])

p=ROOT/'cl_dll/hl2_hud/hud_suitpower.h'
if b'HL2MODERN_ENERGY_COUNTER' not in p.read_bytes():
    p.write_text('''// Numeric Energy counter shares Health/HEV rendering and layout.
#ifndef HUD_SUITPOWER_H
#define HUD_SUITPOWER_H
#include "hudelement.h"
#include "hud_numericdisplay.h"
// HL2MODERN_ENERGY_COUNTER
class CHudSuitPower : public CHudElement, public CHudNumericDisplay
{
    DECLARE_CLASS_SIMPLE(CHudSuitPower,CHudNumericDisplay);
public:
    CHudSuitPower(const char *pElementName);
    virtual void Init();
    virtual void Reset();
    virtual void OnThink();
private:
    int m_iEnergy;
};
#endif
''')
p=ROOT/'cl_dll/hl2_hud/hud_suitpower.cpp'
if b'HL2MODERN_ENERGY_COUNTER' not in p.read_bytes():
    p.write_text('''#include "cbase.h"
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "c_basehlplayer.h"
#include "../../modernize/hud_palette.h"
#include "tier0/memdbgon.h"
// HL2MODERN_ENERGY_COUNTER: networked sprint power, never a separate stamina value.
DECLARE_HUDELEMENT(CHudSuitPower);
CHudSuitPower::CHudSuitPower(const char *name)
    : CHudElement(name), CHudNumericDisplay(NULL,"HudSuitPower")
{
    SetHiddenBits(HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
    Init();
}
void CHudSuitPower::Init() { m_iEnergy=-1; SetLabelText(L"Energy"); SetDisplayValue(0); }
void CHudSuitPower::Reset() { Init(); }
void CHudSuitPower::OnThink()
{
    C_BaseHLPlayer *local=(C_BaseHLPlayer*)C_BasePlayer::GetLocalPlayer();
    int value=local?HL2ModernHud::EnergyValue(local->m_HL2Local.m_flSuitPower):0;
    if(value!=m_iEnergy) { m_iEnergy=value; SetDisplayValue(value); }
}
''')
print('2002 HUD palette, Options page and Energy counter applied.')
