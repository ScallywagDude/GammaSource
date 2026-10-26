#ifndef HL2_MODERN_CRABSYNTH_CHARGE_POLICY_H
#define HL2_MODERN_CRABSYNTH_CHARGE_POLICY_H

// Engine-independent 2003-compatible charge geometry. Distances are signed
// projections from the Crab Synth's position onto its forward/right axes.
// This deliberately does not decide visibility or deal damage: those are
// server-authoritative checks in the NPC implementation.
namespace HL2ModernCrabCharge
{
    enum Decision
    {
        APPROACH,
        MISS,
        HIT_LEFT,
        HIT_RIGHT,
        HIT_CENTER
    };

    inline Decision Classify(float forwardDistance, float lateralDistance,
                             float reach, float halfWidth, float centerHalfWidth)
    {
        // Reject NaNs, negative/non-sensical ranges, and targets behind the NPC.
        // In particular, the original code hit an enemy even after passing it.
        if (!(reach > 0.0f) || !(halfWidth > 0.0f) ||
            !(centerHalfWidth >= 0.0f) || !(halfWidth >= centerHalfWidth) ||
            !(forwardDistance >= 0.0f))
            return MISS;

        if (forwardDistance > reach)
            return APPROACH;

        // Inverse comparisons reject NaN lateral offsets as misses.
        if (!(lateralDistance >= -halfWidth && lateralDistance <= halfWidth))
            return MISS;
        if (lateralDistance < -centerHalfWidth)
            return HIT_LEFT;
        if (lateralDistance > centerHalfWidth)
            return HIT_RIGHT;
        return HIT_CENTER;
    }
}
#endif
