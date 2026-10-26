// GammaSource impulse 101 inventory filter.
// Only remove non-playable prop/debug weapon classes from the cheat inventory.
// Annabelle remains a playable grant: it is deliberately NOT excluded.
// Do not unregister them: levels and NPCs may still need to spawn them.
#ifndef GAMMASOURCE_IMPULSE101_WEAPON_FILTER_H
#define GAMMASOURCE_IMPULSE101_WEAPON_FILTER_H

#include <string.h>

static inline bool GammaImpulse101ShouldGrantWeapon( const char *classname )
{
    if ( !classname || !classname[0] )
        return false;

    const char *const excluded[] =
    {
        "weapon_citizensuitcase",  // NPC luggage prop
        "weapon_citizenpackage",  // NPC rations/package prop
        "weapon_cubemap"          // Developer cubemap visualization tool
    };

    for ( unsigned int i = 0; i < sizeof( excluded ) / sizeof( excluded[0] ); ++i )
    {
        if ( strcmp( classname, excluded[i] ) == 0 )
            return false;
    }
    return true;
}

#endif  // GAMMASOURCE_IMPULSE101_WEAPON_FILTER_H
