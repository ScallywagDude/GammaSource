//========= GammaSource ============//
//
// 2003 terrain deformation, exposed by the engine to the game DLLs
// (the 2018 IVEngineServer/IVEngineClient dropped ApplyTerrainMod).
//
//=================================//

#ifndef IGAMMATERRAINMOD_H
#define IGAMMATERRAINMOD_H
#ifdef _WIN32
#pragma once
#endif

#include "terrainmod.h"

#define GAMMA_TERRAINMOD_INTERFACE_VERSION "GammaTerrainMod001"

abstract_class IGammaTerrainMod
{
public:
	// Server: collision only.
	virtual void	ApplyTerrainModServer( TerrainModType type, const CTerrainModParams &params ) = 0;

	// Client: rendering, plus collision when there is no local server.
	virtual void	ApplyTerrainModClient( TerrainModType type, const CTerrainModParams &params ) = 0;

	// Client: what the mod would do, without applying it. Returns the number of verts written.
	virtual int		ApplyTerrainModSpeculative( TerrainModType type, const CTerrainModParams &params,
		CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts ) = 0;
};

#endif // IGAMMATERRAINMOD_H
