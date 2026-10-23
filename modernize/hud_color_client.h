#ifndef HL2MODERN_HUD_COLOR_CLIENT_H
#define HL2MODERN_HUD_COLOR_CLIENT_H
#include "hud_palette.h"
#include "ConVar.h"
#include "Color.h"
extern ConVar hud_color_custom, hud_color_r, hud_color_g, hud_color_b;
inline Color HL2ModernHudColor(Color original)
{
    if (!hud_color_custom.GetBool()) return original;
    return Color(HL2ModernHud::ColorByte(hud_color_r.GetInt()),
        HL2ModernHud::ColorByte(hud_color_g.GetInt()),
        HL2ModernHud::ColorByte(hud_color_b.GetInt()),original[3]);
}
#endif
