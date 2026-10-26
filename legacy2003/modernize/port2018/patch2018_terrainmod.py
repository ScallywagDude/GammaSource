"""2018 engine: bring back 2003 terrain deformation (displacement terrain mods)."""
from pathlib import Path
R = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")

def edit(rel, old, new, count=1):
    p = R / rel
    d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
    o = old.encode().replace(b"\n", nl); n = new.encode().replace(b"\n", nl)
    if n in d:
        return
    assert d.count(o) == count, (rel, old[:60], d.count(o))
    p.write_bytes(d.replace(o, n))

# --- render verts remember their map position
edit("engine/disp_defs.h", "\tVector\t\t\tm_vPos;\n\t// These are necessary for mat_normals to work",
     "\tVector\t\t\tm_vPos;\n\tVector\t\t\tm_vOriginalPos;\t// GammaSource: map position, terrain mods are relative to it\n\t// These are necessary for mat_normals to work")
edit("engine/disp_mapload.cpp", "\t\tpCoreDisp->GetVert( i, m_Verts[i].m_vPos );\n",
     "\t\tpCoreDisp->GetVert( i, m_Verts[i].m_vPos );\n\t\tm_Verts[i].m_vOriginalPos = m_Verts[i].m_vPos;\n")

# --- IDispInfo / CDispInfo
edit("engine/idispinfo.h", "\tvirtual void\tComputeLightmapAndTextureCoordinate( RayDispOutput_t const& uv, Vector2D* luv, Vector2D* tuv ) = 0;\n};",
     "\tvirtual void\tComputeLightmapAndTextureCoordinate( RayDispOutput_t const& uv, Vector2D* luv, Vector2D* tuv ) = 0;\n\n"
     "\t// GammaSource: 2003 terrain deformation\n"
     "\tvirtual void\tApplyTerrainMod( class ITerrainMod *pMod ) = 0;\n"
     "\tvirtual int\t\tApplyTerrainMod_Speculative( class ITerrainMod *pMod, class CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts ) = 0;\n};")
edit("engine/disp.h", "\tvirtual void\t\tGetBoundingBox( Vector& bbMin, Vector& bbMax );\n",
     "\tvirtual void\t\tGetBoundingBox( Vector& bbMin, Vector& bbMax );\n"
     "\tvirtual void\t\tApplyTerrainMod( class ITerrainMod *pMod );\n"
     "\tvirtual int\t\t\tApplyTerrainMod_Speculative( class ITerrainMod *pMod, class CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts );\n")

impl = '''

//-----------------------------------------------------------------------------
// GammaSource: 2003 terrain deformation. Moves the render verts (and the static
// vertex buffer) relative to their map positions.
//-----------------------------------------------------------------------------
#include "terrainmod.h"

void CDispInfo::ApplyTerrainMod( ITerrainMod *pMod )
{
	Vector bbMin( 1e24, 1e24, 1e24 );
	Vector bbMax( -1e24, -1e24, -1e24 );

	int nVerts = NumVerts();
	CMeshBuilder mb;
	mb.BeginModify( m_pMesh->m_pMesh, m_iVertOffset, nVerts );
	for ( int iVert = 0; iVert < nVerts; iVert++ )
	{
		Vector &vPos = m_Verts[iVert].m_vPos;
		if ( m_AllowedVerts.Get( iVert ) && pMod->ApplyMod( vPos, m_Verts[iVert].m_vOriginalPos ) )
			mb.Position3f( VectorExpand( vPos ) );
		VectorMin( vPos, bbMin, bbMin );
		VectorMax( vPos, bbMax, bbMax );
		mb.AdvanceVertex();
	}
	mb.EndModify();

	// never shrink: the bounds also drive leaf culling
	VectorMin( bbMin, m_BBoxMin, m_BBoxMin );
	VectorMax( bbMax, m_BBoxMax, m_BBoxMax );
}

int CDispInfo::ApplyTerrainMod_Speculative( ITerrainMod *pMod, CSpeculativeTerrainModVert *pSpeculativeVerts, int nMaxVerts )
{
	int nModified = 0;
	if ( nMaxVerts <= 0 )
		return 0;

	int nVerts = NumVerts();
	for ( int iVert = 0; iVert < nVerts; iVert++ )
	{
		if ( !m_AllowedVerts.Get( iVert ) )
			continue;
		pSpeculativeVerts[nModified].m_vNew = m_Verts[iVert].m_vPos;
		const Vector &vOriginalPos = m_Verts[iVert].m_vOriginalPos;
		if ( pMod->ApplyMod( pSpeculativeVerts[nModified].m_vNew, vOriginalPos ) )
		{
			pSpeculativeVerts[nModified].m_vOriginal = vOriginalPos;
			pSpeculativeVerts[nModified].m_vCurrent = m_Verts[iVert].m_vPos;
			if ( ++nModified >= nMaxVerts )
				break;
		}
	}
	return nModified;
}
'''
p = R / "engine/disp_interface.cpp"
d = p.read_text(errors="surrogateescape")
if "CDispInfo::ApplyTerrainMod" not in d:
    # before the trailing memdbg-off / end of file
    p.write_text(d.rstrip() + "\n" + impl, errors="surrogateescape")

# --- collision tree
edit("public/dispcoll_common.h", "\tCDispVector<Vector>\t\t\t\tm_aVerts;\t\t\t\t\t\t\t\t// Displacement verts.\n",
     "\tCDispVector<Vector>\t\t\t\tm_aVerts;\t\t\t\t\t\t\t\t// Displacement verts.\n"
     "\tCDispVector<Vector>\t\t\t\tm_aOriginalVerts;\t\t\t\t\t\t// GammaSource: map positions for terrain mods\n")
edit("public/dispcoll_common.h", "\tvirtual bool Create( CCoreDispInfo *pDisp );\n",
     "\tvirtual bool Create( CCoreDispInfo *pDisp );\n\n"
     "\t// GammaSource: 2003 terrain deformation\n"
     "\tvoid ApplyTerrainMod( class ITerrainMod *pMod );\n")
edit("public/dispcoll_common.cpp", "\t// Copy vertex data.\n\tfor ( int iVert = 0; iVert < m_aVerts.Count(); iVert++ )\n\t{\n\t\tpDisp->GetVert( iVert, m_aVerts[iVert] );\n\t}\n",
     "\t// Copy vertex data.\n\tfor ( int iVert = 0; iVert < m_aVerts.Count(); iVert++ )\n\t{\n\t\tpDisp->GetVert( iVert, m_aVerts[iVert] );\n\t}\n"
     "\tm_aOriginalVerts.SetSize( m_aVerts.Count() );\n"
     "\tfor ( int iVert = 0; iVert < m_aVerts.Count(); iVert++ )\n\t{\n\t\tm_aOriginalVerts[iVert] = m_aVerts[iVert];\n\t}\n")
coll = '''
//-----------------------------------------------------------------------------
// GammaSource: 2003 terrain deformation for collision. Moves the verts, then
// rebuilds the triangle planes, the AABB tree boxes and the edge-plane cache.
//-----------------------------------------------------------------------------
#include "terrainmod.h"

void CDispCollTree::ApplyTerrainMod( ITerrainMod *pMod )
{
	bool bChanged = false;
	for ( int i = 0; i < m_aVerts.Count(); i++ )
	{
		bChanged |= pMod->ApplyMod( m_aVerts[i], m_aOriginalVerts[i] );
	}
	if ( !bChanged )
		return;

	for ( int iTri = 0; iTri < m_aTris.Count(); ++iTri )
	{
		m_aTris[iTri].CalcPlane( m_aVerts );
		m_aTris[iTri].FindMinMax( m_aVerts );
	}
	AABBTree_CalcBounds();

#ifdef ENGINE_DLL
	if ( m_hCache != INVALID_MEMHANDLE )
	{
		g_DispCollTriCache.DestroyResource( m_hCache );
		m_hCache = INVALID_MEMHANDLE;
	}
#endif
	Uncache();
}
'''
p = R / "public/dispcoll_common.cpp"
d = p.read_text(errors="surrogateescape")
if "CDispCollTree::ApplyTerrainMod" not in d:
    p.write_text(d.rstrip() + "\n" + coll, errors="surrogateescape")

# --- cmodel: apply to every tree the mod touches; leaf lists are built with room for mods
edit("engine/cmodel_disp.cpp", "\t\tpDispTree->GetBounds( mins, maxs );\n\n\t\t// UNDONE: The rendering code did this, do we need it?",
     "\t\tpDispTree->GetBounds( mins, maxs );\n"
     "\t\t// GammaSource: leave room for terrain mods (disp_modlimit / _up / _down)\n"
     "\t\tmins -= Vector( 80, 80, 20 );\n\t\tmaxs += Vector( 80, 80, 80 );\n\n\t\t// UNDONE: The rendering code did this, do we need it?")
cm = '''
//-----------------------------------------------------------------------------
// GammaSource: 2003 terrain deformation on the collision trees
//-----------------------------------------------------------------------------
#include "terrainmod.h"

void CM_ApplyTerrainMod( ITerrainMod *pMod )
{
	Vector modMin, modMax;
	pMod->GetBBox( modMin, modMax );

	for ( int i = 0; i < g_DispCollTreeCount; i++ )
	{
		CDispCollTree *pDispTree = &g_pDispCollTrees[i];
		if ( pDispTree->GetPower() == 0 )
			continue;
		Vector bbMin, bbMax;
		pDispTree->GetBounds( bbMin, bbMax );
		if ( IsBoxIntersectingBox( modMin, modMax, bbMin, bbMax ) )
			pDispTree->ApplyTerrainMod( pMod );
	}
}
'''
p = R / "engine/cmodel_disp.cpp"
d = p.read_text(errors="surrogateescape")
if "CM_ApplyTerrainMod" not in d:
    p.write_text(d.rstrip() + "\n" + cm, errors="surrogateescape")

# --- interface for the game DLLs
(R / "public/igammaterrainmod.h").write_text('''//========= GammaSource ============//
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
''')

(R / "engine/gamma_terrainmod.cpp").write_text('''//========= GammaSource ============//
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
''')

w = R / "engine/wscript"
t = w.read_text()
if "gamma_terrainmod.cpp" not in t:
    t = t.replace("\t\t'cmodel_disp.cpp',\n", "\t\t'cmodel_disp.cpp',\n\t\t'gamma_terrainmod.cpp',\n\t\t'terrainmod_functions.cpp',\n", 1)
    w.write_text(t)
print("ok")
