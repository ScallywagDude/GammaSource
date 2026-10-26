#ifndef HL2MODERN_HOUNDEYE_SONIC_POLICY_H
#define HL2MODERN_HOUNDEYE_SONIC_POLICY_H

// Legacy Source-compatible C++98 mathematics for the 2003 Houndeye's
// forward sonic cone. This operates on distances and normalized vector dots;
// line-of-sight filtering is handled by the server's existing FVisible rules.
namespace HL2ModernHoundeye
{
    inline float UnitRange(float value)
    {
        if (!(value > 0.0f)) return 0.0f; // includes NaN
        if (value >= 1.0f) return 1.0f;
        return value;
    }

    inline float SonicFalloff(float distance, float radius, float facingDot)
    {
        if (!(radius > 0.0f) || !(distance >= 0.0f)) return 0.0f;
        if (!(distance < radius)) return 0.0f;
        return UnitRange(1.0f - distance / radius) * UnitRange(facingDot);
    }

    inline float BoundedDamage(float configuredDamage, float falloff)
    {
        // Invalid or excessive values do not reach CTakeDamageInfo.
        if (!(configuredDamage > 0.0f) || configuredDamage > 1000.0f)
            return 0.0f;
        return configuredDamage * UnitRange(falloff);
    }
}

#endif // HL2MODERN_HOUNDEYE_SONIC_POLICY_H
