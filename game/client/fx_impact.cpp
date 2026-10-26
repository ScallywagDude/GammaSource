//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#include "cbase.h"
#include "decals.h"
#include "materialsystem/imaterialvar.h"
#include "IEffects.h"
#include "fx.h"
#include "fx_impact.h"
#include "view.h"
#ifdef TF_CLIENT_DLL
#include "cdll_util.h"
#endif
#include "engine/IStaticPropMgr.h"
#include "c_impact_effects.h"
#include "tier0/vprof.h"
#ifdef GAMMA_PORTALGUN
#include "materialsystem/itexture.h"
#include "gamma/gamma_impact_materials.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar  r_drawflecks( "r_drawflecks", "1" );
extern ConVar r_drawmodeldecals;

ImpactSoundRouteFn g_pImpactSoundRouteFn = NULL;

#ifdef GAMMA_PORTALGUN
static ConVar gamma_impact_debug( "gamma_impact_debug", "0", FCVAR_CHEAT, "Report actual bullet decal selection and surface physics." );

static void GammaDescribeImpactMaterial( const char *label, const char *name, const char *group )
{
	IMaterial *material = materials->FindMaterial( name, group, false );
	bool foundBase = false, foundBump = false;
	IMaterialVar *base = material->FindVar( "$basetexture", &foundBase, false );
	IMaterialVar *bump = material->FindVar( "$bumpmap", &foundBump, false );
	const char *baseName = foundBase && base->IsTexture() && base->GetTextureValue() ? base->GetTextureValue()->GetName() : "";
	const char *bumpName = foundBump && bump->IsTexture() && bump->GetTextureValue() ? bump->GetTextureValue()->GetName() : "";
	Msg( "GAMMA_IMPACT_MATERIAL label=%s material=%s error=%d shader=%s base=%s bump=%s frames=%d\n",
		label, name, IsErrorMaterial( material ) ? 1 : 0, material->GetShaderName(), baseName, bumpName, material->GetNumAnimationFrames() );
}

static void GammaDescribeDecal( const char *group, int index )
{
	const char *name = decalsystem->GetDecalNameForIndex( index );
	Msg( "GAMMA_IMPACT_DECAL group=%s index=%d material=%s\n", group, index, name );
	if ( name[0] )
		GammaDescribeImpactMaterial( group, name, TEXTURE_GROUP_DECAL );
}

CON_COMMAND_F( gamma_impact_info, "Print selected impact/blood materials and sprite precache indices. Use r_printdecalinfo for the actual decal pool.", FCVAR_CHEAT )
{
	static const char *groups[] = { "Impact.Concrete", "Impact.Metal", "Impact.Wood", "Impact.Glass", "Impact.Flesh", "Impact.BloodyFlesh", "Impact.AlienFlesh", "Blood", "YellowBlood" };
	for ( int i = 0; i < ARRAYSIZE( groups ); ++i )
		GammaDescribeDecal( groups[i], decalsystem->GetDecalIndexForName( groups[i] ) );
	static const char *blood[] = { "effects/blood", "effects/blood2", "effects/blood_gore", "effects/blood_drop", "effects/blood_puff", "sprites/bloodspray", "sprites/blood" };
	for ( int i = 0; i < ARRAYSIZE( blood ); ++i )
	{
		char restored[MAX_PATH];
		Q_snprintf( restored, sizeof( restored ), "gamma_2003/%s", blood[i] );
		GammaDescribeImpactMaterial( blood[i], Gamma2003ImpactMaterial( blood[i], restored ), TEXTURE_GROUP_CLIENT_EFFECTS );
	}
	extern short g_sModelIndexBloodSpray, g_sModelIndexBloodDrop;
	Msg( "GAMMA_IMPACT_SPRITES spray=%d drop=%d lowViolence=%d\n", g_sModelIndexBloodSpray, g_sModelIndexBloodDrop, UTIL_IsLowViolence() ? 1 : 0 );
}

CON_COMMAND_F( gamma_impact_probe, "Trace at the crosshair and add a native impact or blood/bloodspray effect: gamma_impact_probe [impact|blood|yellow|spray].", FCVAR_CHEAT )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;
	Vector direction;
	AngleVectors( player->EyeAngles(), &direction );
	Vector start = player->EyePosition();
	trace_t trace;
	UTIL_TraceLine( start, start + direction * MAX_TRACE_LENGTH, MASK_SHOT, player, COLLISION_GROUP_NONE, &trace );
	if ( trace.fraction == 1.0f || !trace.m_pEnt || (trace.surface.flags & SURF_SKY) )
	{
		Msg( "GAMMA_IMPACT_PROBE no_surface=1\n" );
		return;
	}
	surfacedata_t *surface = physprops->GetSurfaceData( trace.surface.surfaceProps );
	const char *mode = args.ArgC() > 1 ? args[1] : "impact";
	Msg( "GAMMA_IMPACT_PROBE mode=%s ent=%d hitbox=%d surface=%s prop=%d gameMaterial=%c normal=%.3f,%.3f,%.3f friction=%.3f density=%.3f\n",
		mode, trace.m_pEnt->entindex(), trace.hitbox, trace.surface.name, trace.surface.surfaceProps, surface->game.material,
		trace.plane.normal.x, trace.plane.normal.y, trace.plane.normal.z, surface->physics.friction, surface->physics.density );
	GammaDescribeImpactMaterial( "surface", trace.surface.name, TEXTURE_GROUP_WORLD );
	if ( !Q_stricmp( mode, "blood" ) || !Q_stricmp( mode, "yellow" ) )
	{
		int color = !Q_stricmp( mode, "yellow" ) ? BLOOD_COLOR_YELLOW : BLOOD_COLOR_RED;
		UTIL_BloodDrips( trace.endpos + trace.plane.normal * 4.0f, trace.plane.normal, color, 30 );
		UTIL_BloodDecalTrace( &trace, color );
	}
	else if ( !Q_stricmp( mode, "spray" ) )
	{
		CEffectData data;
		data.m_vOrigin = trace.endpos + trace.plane.normal * 4.0f;
		data.m_vNormal = trace.plane.normal;
		data.m_flScale = 4.0f;
		data.m_nColor = BLOOD_COLOR_RED;
		data.m_fFlags = FX_BLOODSPRAY_ALL;
		DispatchEffect( "bloodspray", data );
	}
	else
	{
		Vector origin = trace.endpos;
		trace_t impactTrace;
		const char *group = GetImpactDecal( trace.m_pEnt, surface->game.material, DMG_BULLET );
		bool hit = Impact( origin, start, surface->game.material, DMG_BULLET, trace.hitbox, trace.m_pEnt, impactTrace );
		Msg( "GAMMA_IMPACT_PROBE hit=%d group=%s fraction=%.4f\n", hit ? 1 : 0, group, impactTrace.fraction );
		if ( hit )
			PerformCustomEffects( origin, impactTrace, direction, surface->game.material, 1 );
	}
}
#endif

//==========================================================================================================================
// RAGDOLL ENUMERATOR
//==========================================================================================================================
CRagdollEnumerator::CRagdollEnumerator( Ray_t& shot, int iDamageType )
{
	m_rayShot = shot;
	m_iDamageType = iDamageType;
	m_bHit = false;
}

IterationRetval_t CRagdollEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
	if ( pEnt == NULL )
		return ITERATION_CONTINUE;

	C_BaseAnimating *pModel = static_cast< C_BaseAnimating * >( pEnt );

	// If the ragdoll was created on this tick, then the forces were already applied on the server
	if ( pModel == NULL || WasRagdollCreatedOnCurrentTick( pEnt ) )
		return ITERATION_CONTINUE;

	IPhysicsObject *pPhysicsObject = pModel->VPhysicsGetObject();
	if ( pPhysicsObject == NULL )
		return ITERATION_CONTINUE;

	trace_t tr;
	enginetrace->ClipRayToEntity( m_rayShot, MASK_SHOT, pModel, &tr );

	if ( tr.fraction < 1.0 )
	{
		pModel->ImpactTrace( &tr, m_iDamageType, NULL );
		m_bHit = true;

		//FIXME: Yes?  No?
		return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool FX_AffectRagdolls( Vector vecOrigin, Vector vecStart, int iDamageType )
{
	// don't do this when lots of ragdolls are simulating
	if ( s_RagdollLRU.CountRagdolls(true) > 1 )
		return false;
	Ray_t shotRay;
	shotRay.Init( vecStart, vecOrigin );

	CRagdollEnumerator ragdollEnum( shotRay, iDamageType );
	partition->EnumerateElementsAlongRay( PARTITION_CLIENT_RESPONSIVE_EDICTS, shotRay, false, &ragdollEnum );

	return ragdollEnum.Hit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RagdollImpactCallback( const CEffectData &data )
{
	FX_AffectRagdolls( data.m_vOrigin, data.m_vStart, data.m_nDamageType );
}

DECLARE_CLIENT_EFFECT( "RagdollImpact", RagdollImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Impact( Vector &vecOrigin, Vector &vecStart, int iMaterial, int iDamageType, int iHitbox, C_BaseEntity *pEntity, trace_t &tr, int nFlags, int maxLODToDecal )
{
	VPROF( "Impact" );

	Assert ( pEntity );

	// Clear out the trace
	memset( &tr, 0, sizeof(trace_t));
	tr.fraction = 1.0f;

	// Setup our shot information
	Vector shotDir = vecOrigin - vecStart;
	float flLength = VectorNormalize( shotDir );
	Vector traceExt;
	VectorMA( vecStart, flLength + 8.0f, shotDir, traceExt );

	// Attempt to hit ragdolls
	
	bool bHitRagdoll = false;
	
	if ( !pEntity->IsClientCreated() )
	{
		bHitRagdoll = FX_AffectRagdolls( vecOrigin, vecStart, iDamageType );
	}

	if ( (nFlags & IMPACT_NODECAL) == 0 )
	{
		const char *pchDecalName = GetImpactDecal( pEntity, iMaterial, iDamageType );
		int decalNumber = decalsystem->GetDecalIndexForName( pchDecalName );
#ifdef GAMMA_PORTALGUN
		if ( gamma_impact_debug.GetBool() )
		{
			GammaDescribeDecal( pchDecalName, decalNumber );
			Msg( "GAMMA_IMPACT_TRACE ent=%d hitbox=%d gameMaterial=%c damage=%d\n", pEntity->entindex(), iHitbox, iMaterial, iDamageType );
		}
#endif
		if ( decalNumber == -1 )
			return false;

		bool bSkipDecal = false;

#ifdef TF_CLIENT_DLL
		// Don't show blood decals if we're filtering them out (Pyro Goggles)
		if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_PYRO ) || UTIL_IsLowViolence() )
		{
			if ( V_strstr( pchDecalName, "Flesh" ) )
			{
				bSkipDecal = true;
			}
		}
#endif

		if ( !bSkipDecal )
		{
			if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
			{
				staticpropmgr->AddDecalToStaticProp( vecStart, traceExt, iHitbox - 1, decalNumber, true, tr );
			}
			else if ( pEntity )
			{
				// Here we deal with decals on entities.
				pEntity->AddDecal( vecStart, traceExt, vecOrigin, iHitbox, decalNumber, true, tr, maxLODToDecal );
			}
		}
	}
	else
	{
		// Perform the trace ourselves
		Ray_t ray;
		ray.Init( vecStart, traceExt );

		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			// Special case for world entity with hitbox (that's a static prop)
			ICollideable *pCollideable = staticpropmgr->GetStaticPropByIndex( iHitbox - 1 ); 
			enginetrace->ClipRayToCollideable( ray, MASK_SHOT, pCollideable, &tr );
		}
		else
		{
			if ( !pEntity )
				return false;

			enginetrace->ClipRayToEntity( ray, MASK_SHOT, pEntity, &tr );
		}
	}

	// If we found the surface, emit debris flecks
	bool bReportRagdollImpacts = (nFlags & IMPACT_REPORT_RAGDOLL_IMPACTS) != 0;
	if ( ( tr.fraction == 1.0f ) || ( bHitRagdoll && !bReportRagdollImpacts ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *GetImpactDecal( C_BaseEntity *pEntity, int iMaterial, int iDamageType )
{
	char const *decalName;
	if ( !pEntity )
	{
		decalName = "Impact.Concrete";
	}
	else
	{
		decalName = pEntity->DamageDecal( iDamageType, iMaterial );
	}

	// See if we need to offset the decal for material type
	return decalsystem->TranslateDecalForGameMaterial( decalName, iMaterial );
}

//-----------------------------------------------------------------------------
// Purpose: Perform custom effects based on the Decal index
//-----------------------------------------------------------------------------
static ConVar cl_new_impact_effects( "cl_new_impact_effects", "0" );

struct ImpactEffect_t
{
	const char *m_pName;
	const char *m_pNameNoFlecks;
};

static ImpactEffect_t s_pImpactEffect[26] = 
{
	{ "impact_antlion",		NULL },							// CHAR_TEX_ANTLION
	{ NULL,					NULL },							// CHAR_TEX_BLOODYFLESH	
	{ "impact_concrete",	"impact_concrete_noflecks" },	// CHAR_TEX_CONCRETE		
	{ "impact_dirt",		NULL },							// CHAR_TEX_DIRT			
	{ NULL,					NULL },							// CHAR_TEX_EGGSHELL		
	{ NULL,					NULL },							// CHAR_TEX_FLESH			
	{ NULL,					NULL },							// CHAR_TEX_GRATE			
	{ NULL,					NULL },							// CHAR_TEX_ALIENFLESH		
	{ NULL,					NULL },							// CHAR_TEX_CLIP			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_PLASTIC		
	{ "impact_metal",		NULL },							// CHAR_TEX_METAL			
	{ "impact_dirt",		NULL },							// CHAR_TEX_SAND			
	{ NULL,					NULL },							// CHAR_TEX_FOLIAGE		
	{ "impact_computer",	NULL },							// CHAR_TEX_COMPUTER		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_SLOSH			
	{ "impact_concrete",	"impact_concrete_noflecks" },	// CHAR_TEX_TILE			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ "impact_metal",		NULL },							// CHAR_TEX_VENT			
	{ "impact_wood",		"impact_wood_noflecks" },		// CHAR_TEX_WOOD			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ "impact_glass",		NULL },							// CHAR_TEX_GLASS			
	{ "warp_shield_impact", NULL },							// CHAR_TEX_WARPSHIELD		
};

static void SetImpactControlPoint( CNewParticleEffect *pEffect, int nPoint, const Vector &vecImpactPoint, const Vector &vecForward, C_BaseEntity *pEntity )
{
	Vector vecImpactY, vecImpactZ;
	VectorVectors( vecForward, vecImpactY, vecImpactZ ); 
	vecImpactY *= -1.0f;

	pEffect->SetControlPoint( nPoint, vecImpactPoint );
	pEffect->SetControlPointOrientation( nPoint, vecForward, vecImpactY, vecImpactZ );
	pEffect->SetControlPointEntity( nPoint, pEntity );
}

static void PerformNewCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	bool bNoFlecks = !r_drawflecks.GetBool();
	if ( !bNoFlecks )
	{
		bNoFlecks = ( ( nFlags & FLAGS_CUSTIOM_EFFECTS_NOFLECKS ) != 0  );
	}

	// Compute the impact effect name
	const ImpactEffect_t &effect = s_pImpactEffect[ iMaterial - 'A' ];
	const char *pImpactName = effect.m_pName;
	if ( bNoFlecks && effect.m_pNameNoFlecks )
	{
		pImpactName = effect.m_pNameNoFlecks;
	}
	if ( !pImpactName )
		return;

	CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, pImpactName );
	if ( !pEffect->IsValid() )
		return;

	Vector	vecReflect;
	float	flDot = DotProduct( shotDir, tr.plane.normal );
	VectorMA( shotDir, -2.0f * flDot, tr.plane.normal, vecReflect );

	Vector vecShotBackward;
	VectorMultiply( shotDir, -1.0f, vecShotBackward );

	Vector vecImpactPoint = ( tr.fraction != 1.0f ) ? tr.endpos : vecOrigin;
	Assert( VectorsAreEqual( vecOrigin, tr.endpos, 1e-1 ) );

	SetImpactControlPoint( pEffect.GetObject(), 0, vecImpactPoint, tr.plane.normal, tr.m_pEnt ); 
	SetImpactControlPoint( pEffect.GetObject(), 1, vecImpactPoint, vecReflect,		tr.m_pEnt ); 
	SetImpactControlPoint( pEffect.GetObject(), 2, vecImpactPoint, vecShotBackward,	tr.m_pEnt ); 
	pEffect->SetControlPoint( 3, Vector( iScale, iScale, iScale ) );
	if ( pEffect->m_pDef->ReadsControlPoint( 4 ) )
	{
		Vector vecColor;
		GetColorForSurface( &tr, &vecColor );
		pEffect->SetControlPoint( 4, vecColor );
	}
}

void PerformCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;

	if ( cl_new_impact_effects.GetInt() )
	{
		PerformNewCustomEffects( vecOrigin, tr, shotDir, iMaterial, iScale, nFlags );
		return;
	}

	bool bNoFlecks = !r_drawflecks.GetBool();
	if ( !bNoFlecks )
	{
		bNoFlecks = ( ( nFlags & FLAGS_CUSTIOM_EFFECTS_NOFLECKS ) != 0  );
	}

	// Cement and wood have dust and flecks
	if ( ( iMaterial == CHAR_TEX_CONCRETE ) || ( iMaterial == CHAR_TEX_TILE ) )
	{
		FX_DebrisFlecks( vecOrigin, &tr, iMaterial, iScale, bNoFlecks );
	}
	else if ( iMaterial == CHAR_TEX_WOOD )
	{
		FX_DebrisFlecks( vecOrigin, &tr, iMaterial, iScale, bNoFlecks );
	}
	else if ( ( iMaterial == CHAR_TEX_DIRT ) || ( iMaterial == CHAR_TEX_SAND ) )
	{
		FX_DustImpact( vecOrigin, &tr, iScale );
	}
	else if ( iMaterial == CHAR_TEX_ANTLION )
	{
		FX_AntlionImpact( vecOrigin, &tr );
	}
	else if ( ( iMaterial == CHAR_TEX_METAL ) || ( iMaterial == CHAR_TEX_VENT ) )
	{
		Vector	reflect;
		float	dot = shotDir.Dot( tr.plane.normal );
		reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

		reflect[0] += random->RandomFloat( -0.2f, 0.2f );
		reflect[1] += random->RandomFloat( -0.2f, 0.2f );
		reflect[2] += random->RandomFloat( -0.2f, 0.2f );

		FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );
	}
	else if ( iMaterial == CHAR_TEX_COMPUTER )
	{
		Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

		g_pEffects->Sparks( offset );
	}
	else if ( iMaterial == CHAR_TEX_WARPSHIELD )
	{
		QAngle vecAngles;
		VectorAngles( -shotDir, vecAngles );
		DispatchParticleEffect( "warp_shield_impact", vecOrigin, vecAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound for an impact. If tr contains a valid hit, use that. 
//			If not, use the passed in origin & surface.
//-----------------------------------------------------------------------------
void PlayImpactSound( CBaseEntity *pEntity, trace_t &tr, Vector &vecServerOrigin, int nServerSurfaceProp )
{
	VPROF( "PlayImpactSound" );
	surfacedata_t *pdata;
	Vector vecOrigin;

	// If the client-side trace hit a different entity than the server, or
	// the server didn't specify a surfaceprop, then use the client-side trace 
	// material if it's valid.
	if ( tr.DidHit() && (pEntity != tr.m_pEnt || nServerSurfaceProp == 0) )
	{
		nServerSurfaceProp = tr.surface.surfaceProps;
	}
	pdata = physprops->GetSurfaceData( nServerSurfaceProp );
	if ( tr.fraction < 1.0 )
	{
		vecOrigin = tr.endpos;
	}
	else
	{
		vecOrigin = vecServerOrigin;
	}

	// Now play the esound
	if ( pdata->sounds.bulletImpact )
	{
		const char *pbulletImpactSoundName = physprops->GetString( pdata->sounds.bulletImpact );
		
		if ( g_pImpactSoundRouteFn )
		{
			g_pImpactSoundRouteFn( pbulletImpactSoundName, vecOrigin );
		}
		else
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, NULL, pbulletImpactSoundName, pdata->soundhandles.bulletImpact, &vecOrigin );
		}

		return;
	}

#ifdef _DEBUG
	Msg("***ERROR: PlayImpactSound() on a surface with 0 bulletImpactCount!\n");
#endif //_DEBUG
}


void SetImpactSoundRoute( ImpactSoundRouteFn fn )
{
	g_pImpactSoundRouteFn = fn;
}


//-----------------------------------------------------------------------------
// Purpose: Pull the impact data out
// Input  : &data - 
//			*vecOrigin - 
//			*vecAngles - 
//			*iMaterial - 
//			*iDamageType - 
//			*iHitbox - 
//			*iEntIndex - 
//-----------------------------------------------------------------------------
C_BaseEntity *ParseImpactData( const CEffectData &data, Vector *vecOrigin, Vector *vecStart, 
	Vector *vecShotDir, short &nSurfaceProp, int &iMaterial, int &iDamageType, int &iHitbox )
{
	C_BaseEntity *pEntity = data.GetEntity( );
	*vecOrigin = data.m_vOrigin;
	*vecStart = data.m_vStart;
	nSurfaceProp = data.m_nSurfaceProp;
	iDamageType = data.m_nDamageType;
	iHitbox = data.m_nHitBox;

	*vecShotDir = (*vecOrigin - *vecStart);
	VectorNormalize( *vecShotDir );

	// Get the material from the surfaceprop
	surfacedata_t *psurfaceData = physprops->GetSurfaceData( data.m_nSurfaceProp );
	iMaterial = psurfaceData->game.material;

	return pEntity;
}

