//========= GammaSource: Hammer++ physics simulation ============//
//
// Tools > Simulate Physics: drops the selected model entities (props) with VPhysics, the same
// physics the game uses, against the map's world brushes, and moves them to where they come
// to rest. Undo step: "Simulate Physics".
//
//=============================================================================//

#include "stdafx.h"
#include "worldcraft.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapWorld.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "StatusBarIDs.h"
#include "Selection.h"
#include "vphysics_interface.h"
#include "vcollide_parse.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "tier3/tier3.h"
#include "utlbuffer.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static const float PHYSSIM_GRAVITY = 600.0f;		// sv_gravity
static const float PHYSSIM_TIMESTEP = 1.0f / 66.0f;
static const float PHYSSIM_MAX_TIME = 30.0f;		// give up on props that never settle

// brushes that the game doesn't collide with
static bool IsNonSolidTexture( const char *pszTexture )
{
	static const char *s_pNonSolid[] = { "TOOLSTRIGGER", "TOOLSSKYBOX", "TOOLSHINT", "TOOLSSKIP", "TOOLSAREAPORTAL",
		"TOOLSOCCLUDER", "TOOLSFOG", "TOOLSLIGHT", "TOOLSORIGIN", "TOOLSBLOCKLIGHT", "TOOLSBLOCK_LOS", "TOOLSINVISIBLELADDER" };
	for ( int i = 0; i < ARRAYSIZE( s_pNonSolid ); i++ )
	{
		if ( V_stristr( pszTexture, s_pNonSolid[i] ) )
			return true;
	}
	return false;
}

static bool IsCollidingBrushEntity( CMapEntity *pEntity )
{
	static const char *s_pSolid[] = { "func_detail", "func_brush", "func_wall", "func_breakable", "func_physbox",
		"func_movelinear", "func_door", "func_door_rotating", "func_rotating", "func_tracktrain" };
	for ( int i = 0; i < ARRAYSIZE( s_pSolid ); i++ )
	{
		if ( !V_stricmp( pEntity->GetClassName(), s_pSolid[i] ) )
			return true;
	}
	return false;
}

// same defaults as the game (physics_shared.cpp)
static const objectparams_t s_DefaultObjectParams =
{
	NULL,
	1.0,		// mass
	1.0,		// inertia
	0.1f,		// damping
	0.1f,		// rotdamping
	0.05f,		// rotIntertiaLimit
	"DEFAULT",
	NULL,		// game data
	0.f,		// volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f,		// drag coefficient
	true,		// enable collisions?
};

static void LoadSurfaceProperties( IPhysicsSurfaceProps *pProps )
{
	if ( !pProps || pProps->SurfacePropCount() > 0 )
		return;

	KeyValues *pManifest = new KeyValues( "surfaceproperties_manifest" );
	if ( pManifest->LoadFromFile( g_pFullFileSystem, "scripts/surfaceproperties_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *pSub = pManifest->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey() )
		{
			if ( V_stricmp( pSub->GetName(), "file" ) )
				continue;
			CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
			if ( g_pFullFileSystem->ReadFile( pSub->GetString(), "GAME", buf ) )
			{
				buf.PutChar( 0 );
				pProps->ParseSurfaceData( pSub->GetString(), (const char *)buf.Base() );
			}
		}
	}
	pManifest->deleteThis();

	if ( pProps->SurfacePropCount() == 0 )
	{
		// no game scripts: one material is enough for the simulation
		pProps->ParseSurfaceData( "default", "\"default\" { \"density\" \"2000\" \"elasticity\" \"0.25\" \"friction\" \"0.8\" \"dampening\" \"0.0\" }" );
	}
}

static void AddStaticSolid( IPhysicsEnvironment *pEnv, CMapSolid *pSolid, CUtlVector<CPhysCollide *> &collides )
{
	int nFaces = pSolid->GetFaceCount();
	if ( nFaces < 4 )
		return;

	char szTexture[MAX_PATH];
	pSolid->GetFace( 0 )->GetTextureName( szTexture );
	if ( IsNonSolidTexture( szTexture ) )
		return;

	CUtlVector<float> planes;
	for ( int i = 0; i < nFaces; i++ )
	{
		const PLANE &plane = pSolid->GetFace( i )->plane;
		planes.AddToTail( plane.normal.x );
		planes.AddToTail( plane.normal.y );
		planes.AddToTail( plane.normal.z );
		planes.AddToTail( plane.dist );
	}

	CPhysConvex *pConvex = g_pPhysicsCollision->ConvexFromPlanes( planes.Base(), nFaces, 0.25f );
	if ( !pConvex )
		return;
	CPhysCollide *pCollide = g_pPhysicsCollision->ConvertConvexToCollide( &pConvex, 1 );
	if ( !pCollide )
		return;

	objectparams_t params = s_DefaultObjectParams;
	pEnv->CreatePolyObjectStatic( pCollide, 0, vec3_origin, vec3_angle, &params );
	collides.AddToTail( pCollide );
}

static void AddWorldSolids( IPhysicsEnvironment *pEnv, CMapClass *pParent, CUtlVector<CPhysCollide *> &collides )
{
	const CMapObjectList *pChildren = pParent->GetChildren();
	for ( int i = 0; i < pChildren->Count(); i++ )
	{
		CMapClass *pChild = pChildren->Element( i );
		if ( !pChild->IsVisible() )
			continue;

		CMapSolid *pSolid = dynamic_cast< CMapSolid * >( pChild );
		if ( pSolid )
		{
			AddStaticSolid( pEnv, pSolid, collides );
			continue;
		}

		CMapEntity *pEntity = dynamic_cast< CMapEntity * >( pChild );
		if ( pEntity )
		{
			if ( !pEntity->IsPlaceholder() && IsCollidingBrushEntity( pEntity ) && pEntity->GetSelectionState() == SELECT_NONE )
				AddWorldSolids( pEnv, pEntity, collides );
			continue;
		}

		// groups
		AddWorldSolids( pEnv, pChild, collides );
	}
}

struct SimProp_t
{
	CMapEntity *pEntity;
	IPhysicsObject *pObject;
	MDLHandle_t hModel;
};

void PhysicsSimulateSelection( CMapDoc *pDoc )
{
	IPhysics *pPhysics = g_Factory ? (IPhysics *)g_Factory( VPHYSICS_INTERFACE_VERSION, NULL ) : NULL;
	IPhysicsSurfaceProps *pProps = g_Factory ? (IPhysicsSurfaceProps *)g_Factory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL ) : NULL;
	if ( !pPhysics || !g_pPhysicsCollision || !g_pMDLCache )
	{
		AfxMessageBox( "VPhysics is not available, so the simulation can't run." );
		return;
	}

	// the props to drop: selected point entities with a model
	CUtlVector<CMapEntity *> entities;
	const CMapObjectList *pSelList = pDoc->GetSelection()->GetList();
	for ( int i = 0; i < pSelList->Count(); i++ )
	{
		CMapEntity *pEntity = dynamic_cast< CMapEntity * >( pSelList->Element( i ) );
		if ( pEntity && pEntity->IsPlaceholder() && pEntity->GetKeyValue( "model" ) && pEntity->GetKeyValue( "model" )[0] )
			entities.AddToTail( pEntity );
	}
	if ( !entities.Count() )
	{
		AfxMessageBox( "Select the props to simulate (entities with a model, such as prop_physics).", MB_ICONINFORMATION | MB_OK );
		return;
	}

	LoadSurfaceProperties( pProps );

	CWaitCursor wait;
	IPhysicsEnvironment *pEnv = pPhysics->CreateEnvironment();
	pEnv->SetGravity( Vector( 0, 0, -PHYSSIM_GRAVITY ) );
	pEnv->SetSimulationTimestep( PHYSSIM_TIMESTEP );
	pEnv->SetAirDensity( 2.0f );

	CUtlVector<CPhysCollide *> staticCollides;
	AddWorldSolids( pEnv, pDoc->GetMapWorld(), staticCollides );

	CUtlVector<SimProp_t> props;
	int nNoCollision = 0;
	for ( int i = 0; i < entities.Count(); i++ )
	{
		CMapEntity *pEntity = entities[i];
		MDLHandle_t hModel = g_pMDLCache->FindMDL( pEntity->GetKeyValue( "model" ) );
		vcollide_t *pVCollide = ( hModel != MDLHANDLE_INVALID ) ? g_pMDLCache->GetVCollide( hModel ) : NULL;
		if ( !pVCollide || pVCollide->solidCount < 1 )
		{
			if ( hModel != MDLHANDLE_INVALID )
				g_pMDLCache->Release( hModel );
			nNoCollision++;
			continue;
		}

		// mass, damping and so on from the model's collision data
		objectparams_t params = s_DefaultObjectParams;
		solid_t solid;
		memset( &solid, 0, sizeof( solid ) );
		solid.params = params;
		IVPhysicsKeyParser *pParse = g_pPhysicsCollision->VPhysicsKeyParserCreate( pVCollide->pKeyValues );
		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();
			if ( !V_stricmp( pBlock, "solid" ) )
			{
				pParse->ParseSolid( &solid, NULL );
				break;
			}
			pParse->SkipBlock();
		}
		g_pPhysicsCollision->VPhysicsKeyParserDestroy( pParse );

		// ParseSolid zeroes everything it doesn't read and turns collisions off (the game turns
		// them on when it builds the object), so only take what the model sets
		params.mass = solid.params.mass > 0 ? solid.params.mass : 50.0f;
		if ( solid.params.inertia > 0 ) params.inertia = solid.params.inertia;
		if ( solid.params.damping > 0 ) params.damping = solid.params.damping;
		if ( solid.params.rotdamping > 0 ) params.rotdamping = solid.params.rotdamping;
		if ( solid.params.volume > 0 ) params.volume = solid.params.volume;
		params.enableCollisions = true;

		Vector vecOrigin;
		pEntity->GetOrigin( vecOrigin );
		QAngle angles( 0, 0, 0 );
		const char *pszAngles = pEntity->GetKeyValue( "angles" );
		if ( pszAngles )
			sscanf( pszAngles, "%f %f %f", &angles.x, &angles.y, &angles.z );

		int nMaterial = pProps ? pProps->GetSurfaceIndex( solid.surfaceprop[0] ? solid.surfaceprop : "default" ) : 0;
		if ( nMaterial < 0 )
			nMaterial = 0;

		IPhysicsObject *pObject = pEnv->CreatePolyObject( pVCollide->solids[0], nMaterial, vecOrigin, angles, &params );
		if ( !pObject )
		{
			g_pMDLCache->Release( hModel );
			nNoCollision++;
			continue;
		}
		pObject->EnableMotion( true );
		pObject->EnableGravity( true );
		pObject->Wake();

		int k = props.AddToTail();
		props[k].pEntity = pEntity;
		props[k].pObject = pObject;
		props[k].hModel = hModel;
	}

	// run until everything sleeps
	float flTime = 0;
	while ( props.Count() && flTime < PHYSSIM_MAX_TIME )
	{
		pEnv->Simulate( PHYSSIM_TIMESTEP );
		flTime += PHYSSIM_TIMESTEP;

		bool bAllAsleep = true;
		for ( int i = 0; i < props.Count() && bAllAsleep; i++ )
		{
			if ( !props[i].pObject->IsAsleep() )
				bAllAsleep = false;
		}
		if ( bAllAsleep && flTime > 0.25f )
			break;
	}

	if ( props.Count() )
	{
		GetHistory()->MarkUndoPosition( pSelList, "Simulate Physics" );
		for ( int i = 0; i < props.Count(); i++ )
		{
			Vector vecPos;
			QAngle angles;
			props[i].pObject->GetPosition( &vecPos, &angles );

			GetHistory()->Keep( props[i].pEntity );
			props[i].pEntity->SetOrigin( vecPos );
			char szAngles[128];
			V_snprintf( szAngles, sizeof( szAngles ), "%g %g %g", angles.x, angles.y, angles.z );
			props[i].pEntity->SetKeyValue( "angles", szAngles );
		}
		pDoc->SetModifiedFlag();
		pDoc->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );
	}

	for ( int i = 0; i < props.Count(); i++ )
	{
		pEnv->DestroyObject( props[i].pObject );
		g_pMDLCache->Release( props[i].hModel );
	}
	pPhysics->DestroyEnvironment( pEnv );
	for ( int i = 0; i < staticCollides.Count(); i++ )
	{
		g_pPhysicsCollision->DestroyCollide( staticCollides[i] );
	}

	char szStatus[256];
	V_snprintf( szStatus, sizeof( szStatus ), "Simulated %d prop%s for %.1f s against %d brushes%s.", props.Count(), props.Count() == 1 ? "" : "s", flTime,
		staticCollides.Count(), nNoCollision ? " (props without a collision model were left in place)" : "" );
	SetStatusText( SBI_PROMPT, szStatus );
}
