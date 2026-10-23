#ifndef HL2MODERN_LIGHTING_PARAMETERS_H
#define HL2MODERN_LIGHTING_PARAMETERS_H
// Stable, dependency-free numerical policy for the legacy gamma tables.
// Does not change ordinary valid settings or the 2003 gamma transfer curve.
namespace HL2ModernLighting
{
    inline void SanitizeGammaParameters( float &screenGamma, float &texGamma, float &overbright )
    {
        // Ordered comparisons reject NaN; bounds also reject infinities.
        // Gamma <= 0 would make pow() and reciprocal lookup generation invalid.
        if ( !( screenGamma > 0.0f && screenGamma <= 8.0f ) )
            screenGamma = 2.2f;
        if ( !( texGamma > 0.0f && texGamma <= 8.0f ) )
            texGamma = 2.2f;
        if ( !( overbright > 0.0f && overbright <= 16.0f ) )
            overbright = 1.0f;
    }

    inline float ClampCosine( float cosine )
    {
        if ( !( cosine >= -1.0f ) ) return -1.0f;
        if ( cosine > 1.0f ) return 1.0f;
        return cosine;
    }

    inline float Saturate( float x )
    {
        // Includes NaN handling so tests can model the shader-domain policy.
        if ( !( x >= 0.0f ) ) return 0.0f;
        return x > 1.0f ? 1.0f : x;
    }

    inline float SchlickFresnel5( float normalDotEye )
    {
        const float m = Saturate( 1.0f - ClampCosine( normalDotEye ) );
        const float m2 = m * m;
        return m2 * m2 * m;
    }
}
#endif
