// Shared, deterministic policy for the 2002 HUD and its optional RGB override.
#ifndef HL2MODERN_HUD_PALETTE_H
#define HL2MODERN_HUD_PALETTE_H
namespace HL2ModernHud
{
inline int ColorByte(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }
inline int EnergyValue(float value)
{
    if (!(value >= 0.0f)) return 0; // also rejects NaN
    return value >= 100.0f ? 100 : (int)(value + 0.5f);
}
}
#endif
