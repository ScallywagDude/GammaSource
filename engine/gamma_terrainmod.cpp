//========= GammaSource ============//
//
// IGammaTerrainMod: the 2003 engine's ApplyTerrainMod for client and server.
//
//=================================//

#include "quakedef.h"
#include "igammaterrainmod.h"
#include "terrainmod_functions.h"
#include "cmodel_engine.h"
#include "gl_model_private.h"
#include "disp.h"
#include "server.h"
#include "tier1/interface.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CM_ApplyTerrainMod( ITerrainMod *pMod );
IDispInfo *DispInfo_IndexArray( HDISPINFOARRAY hArray, int iElement );

class CGammaTerrainMod : public IGammaTerrainMod
{
public:
	virtual void ApplyTerrainModServer( TerrainModType type, const CTerrainModParams &params )
	{
		CM_ApplyTerrainMod( SetupTerrainMod( type, params ) );
	}

	virtual void ApplyTerrainModClient( TerrainModType type, const CTerrainModParams &params )
	{
		Apply( type, params, NULL, 0 );
	}

	virtual int ApplyTerrainModSpeculative( TerrainModType type, const CTerrainModParams &params,
		CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts )
	{
		return Apply( type, params, pSpeculativeVerts, nMaxVerts );
	}

private:
	int Apply( TerrainModType type, const CTerrainModParams &params, CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts )
	{
		ITerrainMod *pMod = SetupTerrainMod( type, params );
		Vector modMin, modMax;
		pMod->GetBBox( modMin, modMax );

		int nModifiedVerts = 0;
		model_t *pWorld = host_state.worldmodel;
		if ( pWorld )
		{
			for ( int iDisp = 0; iDisp < pWorld->brush.pShared->numDispInfos; iDisp++ )
			{
				IDispInfo *pDisp = DispInfo_IndexArray( pWorld->brush.pShared->hDispInfos, iDisp );
				Vector bbMin, bbMax;
				pDisp->GetBoundingBox( bbMin, bbMax );
				if ( !IsBoxIntersectingBox( modMin, modMax, bbMin, bbMax ) )
					continue;
				if ( pSpeculativeVerts )
					nModifiedVerts += pDisp->ApplyTerrainMod_Speculative( pMod, &pSpeculativeVerts[nModifiedVerts], nMaxVerts - nModifiedVerts );
				else
					pDisp->ApplyTerrainMod( pMod );
			}
		}

		// A local server has already moved the collision.
		if ( !pSpeculativeVerts && !sv.IsActive() )
			CM_ApplyTerrainMod( pMod );

		return nModifiedVerts;
	}
};

static CGammaTerrainMod s_GammaTerrainMod;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGammaTerrainMod, IGammaTerrainMod, GAMMA_TERRAINMOD_INTERFACE_VERSION, s_GammaTerrainMod );
