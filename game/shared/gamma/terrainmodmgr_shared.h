//========= GammaSource ============//
//
// 2003 terrain deformation, shared by client and server.
//
//=================================//

#ifndef TERRAINMODMGR_SHARED_H
#define TERRAINMODMGR_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "igammaterrainmod.h"
#include "tier1/interface.h"

#define MIN_TMOD_RADIUS		1
#define MAX_TMOD_RADIUS		1000

// The engine interface that applies terrain mods (NULL on an engine without it).
inline IGammaTerrainMod *GammaTerrainMod()
{
	static IGammaTerrainMod *s_pTerrainMod = NULL;
	static bool s_bLooked = false;
	if ( !s_bLooked )
	{
		s_bLooked = true;
		CreateInterfaceFn factory = Sys_GetFactory( "engine" DLL_EXT_STRING );
		if ( factory )
			s_pTerrainMod = (IGammaTerrainMod *)factory( GAMMA_TERRAINMOD_INTERFACE_VERSION, NULL );
	}
	return s_pTerrainMod;
}

#endif // TERRAINMODMGR_SHARED_H
