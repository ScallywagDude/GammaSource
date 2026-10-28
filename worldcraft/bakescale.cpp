//========= GammaSource: Hammer++ Bake Model Scale ============//
//
// Tools > Bake Model Scale: for each selected prop with a "modelscale" other than 1, writes the
// model's meshes (LOD 0, from the .mdl/.vvd/.dx90.vtx) and its collision hulls (from the .phy) as
// SMDs, compiles a copy at that scale with studiomdl (models/<model>_scale<N>.mdl), and points
// the prop at the copy with modelscale 1. Meant for static props, which the engine can't scale.
// Undo step: "Bake Model Scale". Uses the studiomdl that Compile Selection to Model found.
//
//=============================================================================//

#include "stdafx.h"
#include "worldcraft.h"
#include "MapDoc.h"
#include "MainFrm.h"
#include "MapEntity.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "StatusBarIDs.h"
#include "Selection.h"
#include "GameConfig.h"
#include "studio.h"
#include "optimize.h"
#include "vphysics_interface.h"
#include "vcollide_parse.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "tier3/tier3.h"
#include "utlbuffer.h"
#include <shlobj.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// from propper.cpp
extern bool Propper_FindStudiomdl( char *pszPath, int nSize );
extern bool Propper_RunStudiomdl( const char *pszStudiomdl, const char *pszGameDir, const char *pszQC, const char *pszLog );

static bool ReadGameFile( const char *pszPath, CUtlBuffer &buf )
{
	buf.Purge();
	return g_pFullFileSystem->ReadFile( pszPath, "GAME", buf );
}

// Writes LOD 0 of the model as an SMD in model space. False if the files can't be read.
static bool WriteModelSMD( const char *pszModel, studiohdr_t *pHdr, FILE *fp )
{
	char szBase[MAX_PATH], szPath[MAX_PATH];
	V_StripExtension( pszModel, szBase, sizeof( szBase ) );

	CUtlBuffer vvdBuf, vtxBuf;
	V_snprintf( szPath, sizeof( szPath ), "%s.vvd", szBase );
	if ( !ReadGameFile( szPath, vvdBuf ) )
		return false;
	V_snprintf( szPath, sizeof( szPath ), "%s.dx90.vtx", szBase );
	if ( !ReadGameFile( szPath, vtxBuf ) )
		return false;

	vertexFileHeader_t *pVvd = (vertexFileHeader_t *)vvdBuf.Base();
	OptimizedModel::FileHeader_t *pVtx = (OptimizedModel::FileHeader_t *)vtxBuf.Base();
	if ( pVvd->id != MODEL_VERTEX_FILE_ID || pVtx->version != OPTIMIZED_MODEL_FILE_VERSION )
		return false;

	// LOD 0 vertices: the fixup table puts them back in order when there is one
	const mstudiovertex_t *pAll = (const mstudiovertex_t *)( (byte *)pVvd + pVvd->vertexDataStart );
	CUtlVector<const mstudiovertex_t *> verts;
	if ( pVvd->numFixups )
	{
		const vertexFileFixup_t *pFixups = (const vertexFileFixup_t *)( (byte *)pVvd + pVvd->fixupTableStart );
		for ( int i = 0; i < pVvd->numFixups; i++ )
		{
			if ( pFixups[i].lod < 0 )
				continue;
			for ( int v = 0; v < pFixups[i].numVertexes; v++ )
				verts.AddToTail( &pAll[pFixups[i].sourceVertexID + v] );
		}
	}
	else
	{
		for ( int v = 0; v < pVvd->numLODVertexes[0]; v++ )
			verts.AddToTail( &pAll[v] );
	}

	fprintf( fp, "version 1\nnodes\n0 \"root\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\ntriangles\n" );

	short *pSkin = pHdr->pSkinref( 0 );
	for ( int bp = 0; bp < pHdr->numbodyparts && bp < pVtx->numBodyParts; bp++ )
	{
		mstudiobodyparts_t *pBodyPart = pHdr->pBodypart( bp );
		OptimizedModel::BodyPartHeader_t *pVtxBodyPart = pVtx->pBodyPart( bp );
		// the first model of each body part (the default body group)
		if ( pBodyPart->nummodels < 1 || pVtxBodyPart->numModels < 1 )
			continue;
		mstudiomodel_t *pModel = pBodyPart->pModel( 0 );
		OptimizedModel::ModelHeader_t *pVtxModel = pVtxBodyPart->pModel( 0 );
		if ( pVtxModel->numLODs < 1 )
			continue;
		OptimizedModel::ModelLODHeader_t *pLOD = pVtxModel->pLOD( 0 );
		int nModelVertStart = pModel->vertexindex / sizeof( mstudiovertex_t );

		for ( int m = 0; m < pModel->nummeshes && m < pLOD->numMeshes; m++ )
		{
			mstudiomesh_t *pMesh = pModel->pMesh( m );
			OptimizedModel::MeshHeader_t *pVtxMesh = pLOD->pMesh( m );
			if ( pHdr->version == 49 )
				pVtxMesh->flags |= OptimizedModel::MESH_IS_MDL49;	// v49 strip group headers are bigger

			char szMaterial[MAX_PATH];
			int nTexture = pSkin[pMesh->material];
			V_strncpy( szMaterial, pHdr->pTexture( nTexture )->pszName(), sizeof( szMaterial ) );
			V_strlower( szMaterial );
			V_FixSlashes( szMaterial, '/' );

			for ( int g = 0; g < pVtxMesh->numStripGroups; g++ )
			{
				OptimizedModel::StripGroupHeader_t *pGroup = pVtxMesh->pStripGroup( g );
				for ( int i = 0; i + 2 < pGroup->numIndices; i += 3 )
				{
					fprintf( fp, "%s\n", szMaterial );
					// vtx triangles are clockwise; SMD wants counter-clockwise
					static const int s_Order[3] = { 0, 2, 1 };
					for ( int k = 0; k < 3; k++ )
					{
						const OptimizedModel::Vertex_t *pVert = pGroup->pVertex( *pGroup->pIndex( i + s_Order[k] ) );
						int nVert = nModelVertStart + pMesh->vertexoffset + pVert->origMeshVertID;
						if ( nVert < 0 || nVert >= verts.Count() )
							continue;
						const mstudiovertex_t &v = *verts[nVert];
						fprintf( fp, "0 %f %f %f %f %f %f %f %f\n", v.m_vecPosition.x, v.m_vecPosition.y, v.m_vecPosition.z,
							v.m_vecNormal.x, v.m_vecNormal.y, v.m_vecNormal.z, v.m_vecTexCoord.x, 1.0f - v.m_vecTexCoord.y );
					}
				}
			}
		}
	}
	fprintf( fp, "end\n" );
	return true;
}

// The collision hulls as triangles. False if the model has none.
static bool WriteCollisionSMD( MDLHandle_t hModel, FILE *fp, float *pflMass )
{
	vcollide_t *pCollide = g_pMDLCache->GetVCollide( hModel );
	if ( !pCollide || pCollide->solidCount < 1 || !g_pPhysicsCollision )
		return false;

	*pflMass = 0;
	IVPhysicsKeyParser *pParse = g_pPhysicsCollision->VPhysicsKeyParserCreate( pCollide->pKeyValues );
	while ( !pParse->Finished() )
	{
		if ( !V_stricmp( pParse->GetCurrentBlockName(), "solid" ) )
		{
			solid_t solid;
			pParse->ParseSolid( &solid, NULL );
			*pflMass += solid.params.mass;
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	g_pPhysicsCollision->VPhysicsKeyParserDestroy( pParse );

	fprintf( fp, "version 1\nnodes\n0 \"root\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\ntriangles\n" );
	for ( int s = 0; s < pCollide->solidCount; s++ )
	{
		Vector *pVerts = NULL;
		int nVerts = g_pPhysicsCollision->CreateDebugMesh( pCollide->solids[s], &pVerts );
		for ( int i = 0; i + 2 < nVerts; i += 3 )
		{
			Vector vNormal = CrossProduct( pVerts[i + 1] - pVerts[i], pVerts[i + 2] - pVerts[i] );
			VectorNormalize( vNormal );
			fprintf( fp, "phy\n" );
			for ( int k = 0; k < 3; k++ )
			{
				fprintf( fp, "0 %f %f %f %f %f %f 0 0\n", pVerts[i + k].x, pVerts[i + k].y, pVerts[i + k].z, vNormal.x, vNormal.y, vNormal.z );
			}
		}
		g_pPhysicsCollision->DestroyDebugMesh( nVerts, pVerts );
	}
	fprintf( fp, "end\n" );
	return true;
}

// Compiles the scaled copy; returns false (and says why) on failure.
static bool BakeModel( const char *pszStudiomdl, const char *pszModel, float flScale, char *pszOutModel, int nOutSize )
{
	MDLHandle_t hModel = g_pMDLCache->FindMDL( pszModel );
	studiohdr_t *pHdr = ( hModel != MDLHANDLE_INVALID ) ? g_pMDLCache->GetStudioHdr( hModel ) : NULL;
	if ( !pHdr )
	{
		if ( hModel != MDLHANDLE_INVALID )
			g_pMDLCache->Release( hModel );
		return false;
	}

	// models/props_c17/oildrum001.mdl at 2 -> props_c17/oildrum001_scale2
	char szName[MAX_PATH], szScale[32];
	V_StripExtension( pszModel, szName, sizeof( szName ) );
	V_FixSlashes( szName, '/' );
	if ( !V_strnicmp( szName, "models/", 7 ) )
		memmove( szName, szName + 7, strlen( szName + 7 ) + 1 );
	V_snprintf( szScale, sizeof( szScale ), "%g", flScale );
	for ( char *p = szScale; *p; p++ )
		if ( *p == '.' ) *p = '_';
	V_strncat( szName, "_scale", sizeof( szName ) );
	V_strncat( szName, szScale, sizeof( szName ) );

	const char *pszGameDir = g_pGameConfig->m_szModDir;
	char szSrcDir[MAX_PATH], szTmp[MAX_PATH];
	V_ComposeFileName( pszGameDir, "modelsrc/worldcraft", szTmp, sizeof( szTmp ) );
	V_ComposeFileName( szTmp, szName, szSrcDir, sizeof( szSrcDir ) );
	V_FixSlashes( szSrcDir );
	SHCreateDirectoryEx( NULL, szSrcDir, NULL );

	char szBase[MAX_PATH], szFile[MAX_PATH], szRef[MAX_PATH], szPhys[MAX_PATH], szQC[MAX_PATH], szLog[MAX_PATH];
	V_FileBase( szName, szBase, sizeof( szBase ) );
	V_snprintf( szFile, sizeof( szFile ), "%s_ref.smd", szBase ); V_ComposeFileName( szSrcDir, szFile, szRef, sizeof( szRef ) );
	V_snprintf( szFile, sizeof( szFile ), "%s_phys.smd", szBase ); V_ComposeFileName( szSrcDir, szFile, szPhys, sizeof( szPhys ) );
	V_snprintf( szFile, sizeof( szFile ), "%s.qc", szBase ); V_ComposeFileName( szSrcDir, szFile, szQC, sizeof( szQC ) );
	V_snprintf( szFile, sizeof( szFile ), "%s_studiomdl.log", szBase ); V_ComposeFileName( szSrcDir, szFile, szLog, sizeof( szLog ) );

	bool bOk = false;
	float flMass = 0;
	bool bCollision = false;
	FILE *fp = fopen( szRef, "wt" );
	if ( fp )
	{
		bOk = WriteModelSMD( pszModel, pHdr, fp );
		fclose( fp );
	}
	if ( bOk )
	{
		fp = fopen( szPhys, "wt" );
		if ( fp )
		{
			bCollision = WriteCollisionSMD( hModel, fp, &flMass );
			fclose( fp );
		}

		fp = fopen( szQC, "wt" );
		if ( fp )
		{
			fprintf( fp, "// written by Worldcraft (Bake Model Scale) from %s\n", pszModel );
			fprintf( fp, "$modelname \"%s.mdl\"\n", szName );
			fprintf( fp, "$staticprop\n" );
			fprintf( fp, "$origin 0 0 0 -90\n" );	// the SMDs are in model space already
			fprintf( fp, "$scale %g\n", flScale );
			fprintf( fp, "$surfaceprop \"%s\"\n", pHdr->pszSurfaceProp()[0] ? pHdr->pszSurfaceProp() : "default" );
			for ( int i = 0; i < pHdr->numcdtextures; i++ )
			{
				char szCd[MAX_PATH];
				V_strncpy( szCd, pHdr->pCdtexture( i ), sizeof( szCd ) );
				V_FixSlashes( szCd, '/' );
				fprintf( fp, "$cdmaterials \"%s\"\n", szCd );
			}
			fprintf( fp, "$body body \"%s_ref.smd\"\n", szBase );
			fprintf( fp, "$sequence idle \"%s_ref.smd\"\n", szBase );
			if ( bCollision )
			{
				// mass grows with the volume
				float flNewMass = ( flMass > 0 ? flMass : 50.0f ) * flScale * flScale * flScale;
				fprintf( fp, "$collisionmodel \"%s_phys.smd\"\n{\n\t$concave\n\t$maxconvexpieces 1024\n\t$mass %g\n}\n", szBase, flNewMass );
			}
			fclose( fp );
		}
		else
		{
			bOk = false;
		}
	}
	g_pMDLCache->Release( hModel );

	if ( !bOk )
		return false;

	char szModelRel[MAX_PATH], szModel[MAX_PATH];
	V_snprintf( szModelRel, sizeof( szModelRel ), "models/%s.mdl", szName );
	V_ComposeFileName( pszGameDir, szModelRel, szModel, sizeof( szModel ) );
	V_FixSlashes( szModel );
	DeleteFile( szModel );
	if ( !Propper_RunStudiomdl( pszStudiomdl, pszGameDir, szQC, szLog ) || GetFileAttributes( szModel ) == INVALID_FILE_ATTRIBUTES )
		return false;

	V_strncpy( pszOutModel, szModelRel, nOutSize );
	return true;
}

void BakeModelScaleSelection( CMapDoc *pDoc )
{
	const CMapObjectList *pSelList = pDoc->GetSelection()->GetList();
	CUtlVector<CMapEntity *> entities;
	for ( int i = 0; i < pSelList->Count(); i++ )
	{
		CMapEntity *pEntity = dynamic_cast< CMapEntity * >( pSelList->Element( i ) );
		if ( !pEntity || !pEntity->IsPlaceholder() )
			continue;
		const char *pszModel = pEntity->GetKeyValue( "model" );
		const char *pszScale = pEntity->GetKeyValue( "modelscale" );
		if ( pszModel && pszModel[0] && pszScale && pszScale[0] && fabs( atof( pszScale ) - 1.0 ) > 0.001 && atof( pszScale ) > 0 )
			entities.AddToTail( pEntity );
	}
	if ( !entities.Count() )
	{
		AfxMessageBox( "Select props whose \"modelscale\" is not 1.", MB_ICONINFORMATION | MB_OK );
		return;
	}

	char szStudiomdl[MAX_PATH];
	if ( !Propper_FindStudiomdl( szStudiomdl, sizeof( szStudiomdl ) ) )
		return;

	CWaitCursor wait;
	GetHistory()->MarkUndoPosition( pSelList, "Bake Model Scale" );
	int nBaked = 0;
	CString strFailed;
	for ( int i = 0; i < entities.Count(); i++ )
	{
		CMapEntity *pEntity = entities[i];
		char szModel[MAX_PATH];
		V_strncpy( szModel, pEntity->GetKeyValue( "model" ), sizeof( szModel ) );
		float flScale = (float)atof( pEntity->GetKeyValue( "modelscale" ) );

		char szNewModel[MAX_PATH];
		SetStatusText( SBI_PROMPT, "Baking the model scale with studiomdl..." );
		if ( !BakeModel( szStudiomdl, szModel, flScale, szNewModel, sizeof( szNewModel ) ) )
		{
			strFailed += "\n";
			strFailed += szModel;
			continue;
		}

		GetHistory()->Keep( pEntity );
		pEntity->SetKeyValue( "model", szNewModel );
		pEntity->SetKeyValue( "modelscale", "1" );
		nBaked++;
	}

	if ( nBaked )
	{
		pDoc->SetModifiedFlag();
		pDoc->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}

	char szStatus[128];
	V_snprintf( szStatus, sizeof( szStatus ), "Baked the scale of %d prop%s.", nBaked, nBaked == 1 ? "" : "s" );
	SetStatusText( SBI_PROMPT, szStatus );
	if ( !strFailed.IsEmpty() )
	{
		AfxMessageBox( "These models could not be baked (their files couldn't be read, or studiomdl failed; its output is in <game>/modelsrc/worldcraft):" + strFailed, MB_ICONEXCLAMATION | MB_OK );
	}
}
