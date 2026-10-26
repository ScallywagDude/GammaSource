// GammaSource: generated from server/episodic/grenade_hopwire.cpp by modernize/port2018/make_vortexhopwire.py
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Gravity well device
//
//=====================================================================================//

#include "cbase.h"
#include "grenade_vortexhopwire.h"
#include "rope.h"
#include "rope_shared.h"
#include "beam_shared.h"
#include "physics.h"
#include "physics_saverestore.h"
#include "explode.h"
#include "physics_prop_ragdoll.h"
#include "movevars_shared.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hopwire_vortex( "hopwire_vortex", "1", 0, "Enable the black hole on weapon_vortexhopwire" );
ConVar hopwire_trap( "hopwire_trap", "0", 0, "Use manual primary-fire detonation instead of the vortex hopwire timer" );
ConVar hopwire_strider_kill_dist_h( "hopwire_strider_kill_dist_h", "300" );
ConVar hopwire_strider_kill_dist_v( "hopwire_strider_kill_dist_v", "256" );
ConVar hopwire_strider_hits( "hopwire_strider_hits", "1" );
ConVar hopwire_hopheight( "hopwire_hopheight", "400" );

ConVar g_debug_hopwire( "g_debug_hopwire", "0" );

#define	DENSE_BALL_MODEL	"models/props_junk/metal_paintcan001b.mdl"

#define	MAX_HOP_HEIGHT		(hopwire_hopheight.GetFloat())		// Maximum amount the grenade will "hop" upwards when detonated

class CGravityVortexController : public CBaseEntity
{
	DECLARE_CLASS( CGravityVortexController, CBaseEntity );
	DECLARE_DATADESC();

public:
	
			CGravityVortexController( void ) : m_flEndTime( 0.0f ), m_flRadius( 256 ), m_flStrength( 256 ), m_flMass( 0.0f ) {}	
	float	GetConsumedMass( void ) const;

	static CGravityVortexController *Create( const Vector &origin, float radius, float strength, float duration );

private:

	void	ConsumeEntity( CBaseEntity *pEnt );
	void	PullPlayersInRange( void );
	bool	KillNPCInRange( CBaseEntity *&pVictim, IPhysicsObject **pPhysObj );
	void	CreateDenseBall( void );
	void	PullThink( void );
	void	StartPull( const Vector &origin, float radius, float strength, float duration );

	float	m_flMass;		// Mass consumed by the vortex
	float	m_flEndTime;	// Time when the vortex will stop functioning
	float	m_flRadius;		// Area of effect for the vortex
	float	m_flStrength;	// Pulling strength of the vortex
};

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of mass consumed by the vortex
//-----------------------------------------------------------------------------
float CGravityVortexController::GetConsumedMass( void ) const 
{
	return m_flMass;
}

//-----------------------------------------------------------------------------
// Purpose: Adds the entity's mass to the aggregate mass consumed
//-----------------------------------------------------------------------------
void CGravityVortexController::ConsumeEntity( CBaseEntity *pEnt )
{
	// Get our base physics object
	IPhysicsObject *pPhysObject = pEnt->VPhysicsGetObject();
	if ( pPhysObject == NULL )
		return;

	// Ragdolls need to report the sum of all their parts
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pEnt );
	if ( pRagdoll != NULL )
	{		
		// Find the aggregate mass of the whole ragdoll
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			m_flMass += pRagdollPhys->list[j].pObject->GetMass();
		}
	}
	else
	{
		// Otherwise we just take the normal mass
		m_flMass += pPhysObject->GetMass();
	}

	// Destroy the entity
	UTIL_Remove( pEnt );
}

//-----------------------------------------------------------------------------
// Purpose: Causes players within the radius to be sucked in
//-----------------------------------------------------------------------------
void CGravityVortexController::PullPlayersInRange( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;
	
	Vector	vecForce = GetAbsOrigin() - pPlayer->WorldSpaceCenter();
	float	dist = VectorNormalize( vecForce );
	
	// FIXME: Need a more deterministic method here
	if ( dist < 128.0f )
	{
		// Kill the player (with falling death sound and effects)
		CTakeDamageInfo deathInfo( this, this, GetAbsOrigin(), GetAbsOrigin(), 200, DMG_FALL );
		pPlayer->TakeDamage( deathInfo );
		
		if ( pPlayer->IsAlive() == false )
		{
			color32 black = { 0, 0, 0, 255 };
			UTIL_ScreenFade( pPlayer, black, 0.1f, 0.0f, (FFADE_OUT|FFADE_STAYOUT) );
			return;
		}
	}

	// Must be within the radius
	if ( dist > m_flRadius )
		return;

	IPhysicsObject *pPhysics = pPlayer->VPhysicsGetObject();
	float mass = pPhysics ? pPhysics->GetMass() : 85.0f;
	float playerForce = m_flStrength * 0.05f;

	// Find the pull force
	// NOTE: We might want to make this non-linear to give more of a "grace distance"
	vecForce *= ( 1.0f - ( dist / m_flRadius ) ) * playerForce * mass;
	vecForce[2] *= 0.025f;
	
	pPlayer->SetBaseVelocity( vecForce );
	pPlayer->AddFlag( FL_BASEVELOCITY );
	
	// Make sure the player moves
	if ( vecForce.z > 0 && ( pPlayer->GetFlags() & FL_ONGROUND) )
	{
		pPlayer->SetGroundEntity( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to kill an NPC if it's within range and other criteria
// Input  : *pVictim - NPC to assess
//			**pPhysObj - pointer to the ragdoll created if the NPC is killed
// Output :	bool - whether or not the NPC was killed and the returned pointer is valid
//-----------------------------------------------------------------------------
bool CGravityVortexController::KillNPCInRange( CBaseEntity *&pVictim, IPhysicsObject **pPhysObj )
{
	CBaseCombatCharacter *pBCC = pVictim->MyCombatCharacterPointer();

	// See if we can ragdoll
	if ( pBCC != NULL && pBCC->IsNPC() && pBCC->CanBecomeRagdoll() )
	{
		// Don't bother with striders
		if ( FClassnameIs( pBCC, "npc_strider" ) )
			return false;

		// TODO: Make this an interaction between the NPC and the vortex

		// Become ragdoll
		CTakeDamageInfo info( this, this, 1.0f, DMG_GENERIC );
		CBaseEntity *pRagdoll = CreateServerRagdoll( pBCC, 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		if ( !pRagdoll )
			return false;
		pRagdoll->SetCollisionBounds( pVictim->CollisionProp()->OBBMins(), pVictim->CollisionProp()->OBBMaxs() );

		// Necessary to cause it to do the appropriate death cleanup
		CTakeDamageInfo ragdollInfo( this, this, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
		pVictim->TakeDamage( ragdollInfo );
		pVictim = pRagdoll;

		// Return the pointer to the ragdoll
		*pPhysObj = pRagdoll->VPhysicsGetObject();
		return true;
	}

	// Wasn't able to ragdoll this target
	*pPhysObj = NULL;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a dense ball with a mass equal to the aggregate mass consumed by the vortex
//-----------------------------------------------------------------------------
void CGravityVortexController::CreateDenseBall( void )
{
	CBaseEntity *pBall = CreateEntityByName( "prop_physics" );
	
	pBall->SetModel( DENSE_BALL_MODEL );
	pBall->SetAbsOrigin( GetAbsOrigin() );
	pBall->Spawn();

	IPhysicsObject *pObj = pBall->VPhysicsGetObject();
	if ( pObj != NULL )
	{
		pObj->SetMass( GetConsumedMass() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pulls physical objects towards the vortex center, killing them if they come too near
//-----------------------------------------------------------------------------
void CGravityVortexController::PullThink( void )
{
	// Pull any players close enough to us
	PullPlayersInRange();

	Vector mins, maxs;
	mins = GetAbsOrigin() - Vector( m_flRadius, m_flRadius, m_flRadius );
	maxs = GetAbsOrigin() + Vector( m_flRadius, m_flRadius, m_flRadius );

	// Draw debug information
	if ( g_debug_hopwire.GetBool() )
	{
		NDebugOverlay::Box( GetAbsOrigin(), mins - GetAbsOrigin(), maxs - GetAbsOrigin(), 0, 255, 0, 16, 4.0f );
	}

	CBaseEntity *pEnts[128];
	int numEnts = UTIL_EntitiesInBox( pEnts, 128, mins, maxs, 0 );

	for ( int i = 0; i < numEnts; i++ )
	{
		if ( pEnts[i] == this || pEnts[i]->IsPlayer() || pEnts[i]->IsWorld() || pEnts[i]->IsMarkedForDeletion() ||
			( pEnts[i]->WorldSpaceCenter() - GetAbsOrigin() ).LengthSqr() > m_flRadius * m_flRadius )
			continue;
		IPhysicsObject *pPhysObject = NULL;

		// Attempt to kill and ragdoll any victims in range
		if ( KillNPCInRange( pEnts[i], &pPhysObject ) == false )
		{	
			// If we didn't have a valid victim, see if we can just get the vphysics object
			pPhysObject = pEnts[i]->VPhysicsGetObject();
			if ( pPhysObject == NULL )
				continue;
		}
		if ( !pPhysObject || !pPhysObject->IsMoveable() )
			continue;

		float mass;

		CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pEnts[i] );
		if ( pRagdoll != NULL )
		{
			ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
			mass = 0.0f;
			
			// Find the aggregate mass of the whole ragdoll
			for ( int j = 0; j < pRagdollPhys->listCount; ++j )
			{
				mass += pRagdollPhys->list[j].pObject->GetMass();
			}
		}
		else
		{
			mass = pPhysObject->GetMass();
		}

		Vector	vecForce = GetAbsOrigin() - pEnts[i]->WorldSpaceCenter();
		Vector	vecForce2D = vecForce;
		vecForce2D[2] = 0.0f;
		float	dist2D = VectorNormalize( vecForce2D );
		float	dist = VectorNormalize( vecForce );
		
		// FIXME: Need a more deterministic method here
		if ( dist < 48.0f )
		{
			ConsumeEntity( pEnts[i] );
			continue;
		}

		// Must be within the radius
		if ( dist > m_flRadius )
			continue;

		// Find the pull force
		vecForce *= ( 1.0f - ( dist2D / m_flRadius ) ) * m_flStrength * mass;
		
		if ( pEnts[i]->VPhysicsGetObject() )
		{
			// Pull the object in
			pEnts[i]->VPhysicsTakeDamage( CTakeDamageInfo( this, this, vecForce, GetAbsOrigin(), m_flStrength, DMG_BLAST ) );
		}
	}

	// Keep going if need-be
	if ( m_flEndTime > gpGlobals->curtime )
	{
		SetThink( &CGravityVortexController::PullThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		DevMsg( "GAMMA_VORTEX_END consumed_mass=%.2f\n", m_flMass );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the vortex working
//-----------------------------------------------------------------------------
void CGravityVortexController::StartPull( const Vector &origin, float radius, float strength, float duration )
{
	SetAbsOrigin( origin );
	m_flEndTime	= gpGlobals->curtime + duration;
	m_flRadius	= radius;
	m_flStrength= strength;

	SetThink( &CGravityVortexController::PullThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Creation utility
//-----------------------------------------------------------------------------
CGravityVortexController *CGravityVortexController::Create( const Vector &origin, float radius, float strength, float duration )
{
	// Create an instance of the vortex
	CGravityVortexController *pVortex = (CGravityVortexController *) CreateEntityByName( "vortex_controller" );
	if ( pVortex == NULL )
		return NULL;

	// Start the vortex working
	pVortex->StartPull( origin, radius, strength, duration );
	DevMsg( "GAMMA_VORTEX_START origin=%.2f %.2f %.2f radius=%.0f strength=%.0f duration=%.1f\n",
		origin.x, origin.y, origin.z, radius, strength, duration );

	return pVortex;
}

BEGIN_DATADESC( CGravityVortexController )
	DEFINE_FIELD( m_flMass, FIELD_FLOAT ),
	DEFINE_FIELD( m_flEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStrength, FIELD_FLOAT ),

	DEFINE_THINKFUNC( PullThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( vortex_controller, CGravityVortexController );

CON_COMMAND_F( gamma_vortexinfo, "Print active black holes and a gamma_vortex_probe physics prop", FCVAR_CHEAT )
{
	int controllers = 0;
	CBaseEntity *entity = NULL;
	while ( ( entity = gEntList.FindEntityByClassname( entity, "vortex_controller" ) ) != NULL )
	{
		if ( entity->IsMarkedForDeletion() )
			continue;
		++controllers;
		Vector origin = entity->GetAbsOrigin();
		CBaseEntity *probe = gEntList.FindEntityByName( NULL, "gamma_vortex_probe" );
		Vector velocity = vec3_origin;
		if ( probe && probe->VPhysicsGetObject() )
			probe->VPhysicsGetObject()->GetVelocity( &velocity, NULL );
		Msg( "GAMMA_VORTEX_INFO entity=%d origin=%.2f %.2f %.2f mass=%.2f probe=%d distance=%.2f speed=%.2f\n",
			entity->entindex(), origin.x, origin.y, origin.z,
			static_cast<CGravityVortexController *>( entity )->GetConsumedMass(), probe ? 1 : 0,
			probe ? ( probe->WorldSpaceCenter() - origin ).Length() : -1.0f, velocity.Length() );
	}
	Msg( "GAMMA_VORTEX_STATUS enabled=%d trap=%d controllers=%d\n", hopwire_vortex.GetBool() ? 1 : 0,
		hopwire_trap.GetBool() ? 1 : 0, controllers );
}

CON_COMMAND_F( gamma_vortex_probe, "Place a weightless test prop near the first active black hole", FCVAR_CHEAT )
{
	CBaseEntity *vortex = gEntList.FindEntityByClassname( NULL, "vortex_controller" );
	if ( !vortex || vortex->IsMarkedForDeletion() )
	{
		Warning( "GAMMA_VORTEX_PROBE no active controller\n" );
		return;
	}
	const Vector offsets[] = { Vector( 160, 0, 32 ), Vector( -160, 0, 32 ), Vector( 0, 160, 32 ), Vector( 0, -160, 32 ), Vector( 0, 0, 160 ) };
	Vector origin;
	bool clear = false;
	for ( int i = 0; i < ARRAYSIZE( offsets ); ++i )
	{
		origin = vortex->GetAbsOrigin() + offsets[i];
		trace_t trace;
		UTIL_TraceHull( vortex->GetAbsOrigin(), origin, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ),
			MASK_SOLID, vortex, COLLISION_GROUP_NONE, &trace );
		if ( !trace.startsolid && trace.fraction == 1.0f )
		{
			clear = true;
			break;
		}
	}
	if ( !clear )
	{
		Warning( "GAMMA_VORTEX_PROBE no clear test position\n" );
		return;
	}
	CBaseEntity *probe = CreateEntityByName( "prop_physics" );
	if ( !probe )
		return;
	probe->KeyValue( "targetname", "gamma_vortex_probe" );
	probe->SetModel( DENSE_BALL_MODEL );
	probe->SetAbsOrigin( origin );
	DispatchSpawn( probe );
	IPhysicsObject *physics = probe->VPhysicsGetObject();
	if ( physics )
		physics->EnableGravity( false );
	else
		UTIL_Remove( probe );
	Msg( "GAMMA_VORTEX_PROBE created=%d position=%.2f %.2f %.2f\n", physics ? 1 : 0, origin.x, origin.y, origin.z );
}

CON_COMMAND_F( gamma_vortex_view, "Aim the local player's view at the first active black hole", FCVAR_CHEAT )
{
	CBaseEntity *vortex = gEntList.FindEntityByClassname( NULL, "vortex_controller" );
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if ( !vortex || !player )
		return;
	QAngle angles;
	VectorAngles( vortex->GetAbsOrigin() - player->EyePosition(), angles );
	player->SnapEyeAngles( angles );
}

#define GRENADE_MODEL_CLOSED	"models/Weapons/w_hopwire.mdl"
#define GRENADE_MODEL_OPEN		"models/Weapons/w_hopwire.mdl"

BEGIN_DATADESC( CGrenadeVortexHopwire )
	DEFINE_FIELD( m_hVortexController, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bVortexActive, FIELD_BOOLEAN ),

	DEFINE_THINKFUNC( EndThink ),
	DEFINE_THINKFUNC( CombatThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_grenade_vortexhopwire, CGrenadeVortexHopwire );

IMPLEMENT_SERVERCLASS_ST( CGrenadeVortexHopwire, DT_GrenadeVortexHopwire )
	SendPropBool( SENDINFO( m_bVortexActive ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::Spawn( void )
{
	Precache();

	SetModel( GRENADE_MODEL_CLOSED );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	
	if ( !CreateVPhysics() )
		UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrenadeVortexHopwire::CreateVPhysics()
{
	// Create the object in the physics system
	return VPhysicsInitNormal( SOLID_BBOX, 0, false ) != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::Precache( void )
{
	// FIXME: Replace
	//PrecacheSound("NPC_Strider.Shoot");
	//PrecacheSound("d3_citadel.weapon_zapper_beam_loop2");

	PrecacheModel( GRENADE_MODEL_OPEN );
	PrecacheModel( GRENADE_MODEL_CLOSED );
	
	PrecacheModel( DENSE_BALL_MODEL );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timer - 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::SetTimer( float timer )
{
	SetThink( &CBaseGrenade::PreDetonate );
	SetNextThink( gpGlobals->curtime + timer );
}

#define	MAX_STRIDER_KILL_DISTANCE_HORZ	(hopwire_strider_kill_dist_h.GetFloat())		// Distance a Strider will be killed if within
#define	MAX_STRIDER_KILL_DISTANCE_VERT	(hopwire_strider_kill_dist_v.GetFloat())		// Distance a Strider will be killed if within

#define MAX_STRIDER_STUN_DISTANCE_HORZ	(MAX_STRIDER_KILL_DISTANCE_HORZ*2)	// Distance a Strider will be stunned if within
#define MAX_STRIDER_STUN_DISTANCE_VERT	(MAX_STRIDER_KILL_DISTANCE_VERT*2)	// Distance a Strider will be stunned if within

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::KillStriders( void )
{
	CBaseEntity *pEnts[128];
	Vector	mins, maxs;

	ClearBounds( mins, maxs );
	AddPointToBounds( -Vector( MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ ), mins, maxs );
	AddPointToBounds(  Vector( MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ ), mins, maxs );
	AddPointToBounds( -Vector( MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT ), mins, maxs );
	AddPointToBounds(  Vector( MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT ), mins, maxs );

	// FIXME: It's probably much faster to simply iterate over the striders in the map, rather than any entity in the radius - jdw

	// Find any striders in range of us
	int numTargets = UTIL_EntitiesInBox( pEnts, ARRAYSIZE( pEnts ), GetAbsOrigin()+mins, GetAbsOrigin()+maxs, FL_NPC );
	float targetDistHorz, targetDistVert;

	for ( int i = 0; i < numTargets; i++ )
	{
		// Only affect striders
		if ( FClassnameIs( pEnts[i], "npc_strider" ) == false )
			continue;

		// We categorize our spatial relation to the strider in horizontal and vertical terms, so that we can specify both parameters separately
		targetDistHorz = UTIL_DistApprox2D( pEnts[i]->GetAbsOrigin(), GetAbsOrigin() );
		targetDistVert = fabs( pEnts[i]->GetAbsOrigin()[2] - GetAbsOrigin()[2] );

		if ( targetDistHorz < MAX_STRIDER_KILL_DISTANCE_HORZ && targetDistHorz < MAX_STRIDER_KILL_DISTANCE_VERT )
		{
			// Kill the strider
			float fracDamage = ( pEnts[i]->GetMaxHealth() / hopwire_strider_hits.GetFloat() ) + 1.0f;
			CTakeDamageInfo killInfo( this, this, fracDamage, DMG_GENERIC );
			Vector	killDir = pEnts[i]->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize( killDir );

			killInfo.SetDamageForce( killDir * -1000.0f );
			killInfo.SetDamagePosition( GetAbsOrigin() );

			pEnts[i]->TakeDamage( killInfo );
		}
		else if ( targetDistHorz < MAX_STRIDER_STUN_DISTANCE_HORZ && targetDistHorz < MAX_STRIDER_STUN_DISTANCE_VERT )
		{
			// Stun the strider
			CTakeDamageInfo killInfo( this, this, 200.0f, DMG_GENERIC );
			pEnts[i]->TakeDamage( killInfo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::EndThink( void )
{
	m_bVortexActive = false;
	if ( hopwire_vortex.GetBool() )
	{
		EntityMessageBegin( this, true );
			WRITE_BYTE( 1 );
		MessageEnd();
	}

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::CombatThink( void )
{
	// Stop the grenade from moving
	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
	VPhysicsDestroyObject();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );

	// Do special behaviors if there are any striders in the area
	KillStriders();

	// FIXME: Replace
	//EmitSound("NPC_Strider.Shoot");
	//EmitSound("d3_citadel.weapon_zapper_beam_loop2");

	// Quick screen flash
	CBasePlayer *pPlayer = ToBasePlayer( GetThrower() );
	color32 white = { 255,255,255,255 };
	if ( pPlayer )
		UTIL_ScreenFade( pPlayer, white, 0.2f, 0.0f, FFADE_IN );

	// Create the vortex controller to pull entities towards us
	if ( hopwire_vortex.GetBool() )
	{
		m_hVortexController = CGravityVortexController::Create( GetAbsOrigin(), 512, 150, 3.0f );
		m_bVortexActive = m_hVortexController != NULL;

		// Start our client-side effect
		EntityMessageBegin( this, true );
			WRITE_BYTE( 0 );
		MessageEnd();
		
		// Begin to stop in two seconds
		SetThink( &CGrenadeVortexHopwire::EndThink );
		SetNextThink( gpGlobals->curtime + 2.0f );
	}
	else
	{
		// Remove us immediately
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	
	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hop off the ground to start deployment
//-----------------------------------------------------------------------------
void CGrenadeVortexHopwire::Detonate( void )
{
	SetModel( GRENADE_MODEL_OPEN );

	AngularImpulse	hopAngle = RandomAngularImpulse( -300, 300 );

	//Find out how tall the ceiling is and always try to hop halfway
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, MAX_HOP_HEIGHT*2 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	// Jump half the height to the found ceiling
	float hopHeight = MIN( MAX_HOP_HEIGHT, (MAX_HOP_HEIGHT*tr.fraction) );

	//Add upwards velocity for the "hop"
	Vector hopVel( 0.0f, 0.0f, hopHeight );
	SetVelocity( hopVel, hopAngle );

	// Get the time until the apex of the hop
	float apexTime = sqrt( hopHeight / GetCurrentGravity() );

	// Explode at the apex
	SetThink( &CGrenadeVortexHopwire::CombatThink );
	SetNextThink( gpGlobals->curtime + apexTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseGrenade *VortexHopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer )
{
	CGrenadeVortexHopwire *pGrenade = (CGrenadeVortexHopwire *) CBaseEntity::Create( "npc_grenade_vortexhopwire", position, angles, pOwner );
	if ( !pGrenade || pGrenade->IsMarkedForDeletion() )
		return NULL;
	
	// Only set ourselves to detonate on a timer if we're not a trap hopwire
	if ( hopwire_trap.GetBool() == false )
	{
		pGrenade->SetTimer( timer );
	}

	pGrenade->SetVelocity( velocity, angVelocity );
	pGrenade->SetThrower( ToBaseCombatCharacter( pOwner ) );

	return pGrenade;
}
