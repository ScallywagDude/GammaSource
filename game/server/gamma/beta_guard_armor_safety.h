// GammaSource 2003 Guard: bounded selection of surviving armor pieces.
// Header is deliberately independent of the engine for C++98 regression tests.
#ifndef GAMMASOURCE_BETA_GUARD_ARMOR_SAFETY_H
#define GAMMASOURCE_BETA_GUARD_ARMOR_SAFETY_H

inline int BetaGuardCountIntactArmor( const bool *eligible, int count )
{
	if ( !eligible || count <= 0 ) return 0;
	int remaining = 0;
	for ( int i = 0; i < count; ++i )
		if ( eligible[i] ) ++remaining;
	return remaining;
}

// rank is a uniform random ordinal in [0, remaining). No retry loop.
inline int BetaGuardPickIntactArmor( const bool *eligible, int count, int rank )
{
	if ( !eligible || count <= 0 || rank < 0 ) return -1;
	for ( int i = 0; i < count; ++i )
	{
		if ( !eligible[i] ) continue;
		if ( rank-- == 0 ) return i;
	}
	return -1;
}

#endif
