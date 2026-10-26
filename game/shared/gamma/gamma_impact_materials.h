// Private 2003 impact assets take priority without replacing mounted game files.
#ifndef GAMMA_IMPACT_MATERIALS_H
#define GAMMA_IMPACT_MATERIALS_H

#include "filesystem.h"
#include "tier1/strtools.h"

inline const char *Gamma2003ImpactMaterial( const char *original, const char *restored )
{
#ifdef GAMMA_PORTALGUN
	char path[MAX_PATH];
	const int length = Q_strlen( restored );
	const bool hasExtension = length >= 4 && !Q_stricmp( restored + length - 4, ".vmt" );
	Q_snprintf( path, sizeof( path ), "materials/%s%s", restored, hasExtension ? "" : ".vmt" );
	if ( filesystem->FileExists( path, "GAME" ) )
		return restored;
#endif
	return original;
}

#define GAMMA_IMPACT_MATERIAL(name) Gamma2003ImpactMaterial( name, "gamma_2003/" name )

#endif
