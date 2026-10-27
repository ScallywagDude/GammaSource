// GammaSource: generated from server/episodic/weapon_hopwire.cpp by modernize/port2018/make_vortexhopwire.py
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "grenade_frag.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "grenade_vortexhopwire.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GRENADE_TIMER	2.0f	//Seconds

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f	// inches

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponVortexHopwire: public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponVortexHopwire, CBaseHLCombatWeapon );
public:
	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	DecrementAmmo( CBaseCombatCharacter *pOwner );
	void	ItemPostFrame( void );

	void	HandleFireOnEmpty( void );
	bool	HasAnyAmmo( void );
	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	bool	Reload( void );

private:
	bool	ThrowGrenade( CBasePlayer *pPlayer );
	bool	RollGrenade( CBasePlayer *pPlayer );
	bool	LobGrenade( CBasePlayer *pPlayer );
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void	CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc );

	bool	m_bRedraw;	//Draw the weapon again after throwing a grenade
	
	int		m_AttackPaused;
	bool	m_fDrawbackFinished;

	CHandle<CGrenadeVortexHopwire>	m_hActiveHopWire;

	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};


BEGIN_DATADESC( CWeaponVortexHopwire )
	DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_AttackPaused, FIELD_INTEGER ),
	DEFINE_FIELD( m_fDrawbackFinished, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hActiveHopWire, FIELD_EHANDLE ),
END_DATADESC()

acttable_t	CWeaponVortexHopwire::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
};

IMPLEMENT_ACTTABLE(CWeaponVortexHopwire);

IMPLEMENT_SERVERCLASS_ST(CWeaponVortexHopwire, DT_WeaponVortexHopwire)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_vortexhopwire, CWeaponVortexHopwire );
PRECACHE_WEAPON_REGISTER(weapon_vortexhopwire);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::Precache( void )
{
	BaseClass::Precache();

	UTIL_PrecacheOther( "npc_grenade_vortexhopwire" );

	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );

	m_bRedraw = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::Deploy( void )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// A thrown hopwire no longer locks the player to this weapon. The handle
	// survives the holster, so trap mode can still be sprung after switching
	// back, and a second throw stays blocked until the first one is gone.
	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	bool fThrewGrenade = false;
	bool fThrowEvent = false;

	switch( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_fDrawbackFinished = true;
			break;

		case EVENT_WEAPON_THROW:
			fThrowEvent = true;
			if ( pOwner && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
				fThrewGrenade = ThrowGrenade( pOwner );
			break;

		case EVENT_WEAPON_THROW2:
			fThrowEvent = true;
			if ( pOwner && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
				fThrewGrenade = RollGrenade( pOwner );
			break;

		case EVENT_WEAPON_THROW3:
			fThrowEvent = true;
			if ( pOwner && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
				fThrewGrenade = LobGrenade( pOwner );
			break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}

#define RETHROW_DELAY	0.5
	if ( fThrowEvent && !fThrewGrenade )
	{
		// Creation failure must leave ammo intact and release the pullback lock.
		m_bRedraw = false;
		m_fDrawbackFinished = false;
		m_AttackPaused = GRENADE_PAUSED_NO;
		SendWeaponAnim( ACT_VM_IDLE );
		m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.2f;
	}
	if( fThrewGrenade )
	{
		DecrementAmmo( pOwner );
		m_flNextPrimaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flNextSecondaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

		// Make a sound designed to scare snipers back into their holes!
		CBaseCombatCharacter *pOwner = GetOwner();

		if( pOwner )
		{
			Vector vecSrc = pOwner->Weapon_ShootPosition();
			Vector	vecDir;

			AngleVectors( pOwner->EyeAngles(), &vecDir );

			trace_t tr;

			UTIL_TraceLine( vecSrc, vecSrc + vecDir * 1024, MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &tr );

			CSoundEnt::InsertSound( SOUND_DANGER_SNIPERONLY, tr.endpos, 384, 0.2, pOwner );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override the ammo behavior so we never disallow pulling the weapon out
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::HasAnyAmmo( void )
{
	// Only a trap still waiting to be sprung keeps an empty hopwire selectable.
	if ( hopwire_trap.GetBool() && m_hActiveHopWire != NULL )
		return true;

	return BaseClass::HasAnyAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::Reload( void )
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::SecondaryAttack( void )
{
	if ( m_bRedraw )
		return;

	// One live hopwire at a time.
	if ( !HasPrimaryAmmo() || m_hActiveHopWire != NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// The beta model labels standing lob as HAULBACK and crouched roll as
	// SECONDARYATTACK. Their throw events create the appropriate projectile.
	m_AttackPaused = GRENADE_PAUSED_NO;
	m_fDrawbackFinished = false;
	SendWeaponAnim( ( pPlayer->GetFlags() & FL_DUCKING ) ? ACT_VM_SECONDARYATTACK : ACT_VM_HAULBACK );
	m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack =
		gpGlobals->curtime + MAX( 0.2f, SequenceDuration() );
}

//-----------------------------------------------------------------------------
// Purpose: Allow activation even if this is our last piece of ammo
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::HandleFireOnEmpty( void )
{
	if ( m_hActiveHopWire!= NULL )
	{
		// FIXME: This toggle is hokey
		m_bRedraw = false;
		PrimaryAttack();
		m_bRedraw = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::PrimaryAttack( void )
{
	if ( m_bRedraw )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();
	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );;
	if ( !pPlayer )
		return;

	// See if we're in trap mode
	if ( hopwire_trap.GetBool() && ( m_hActiveHopWire != NULL ) )
	{
		// Spring the trap
		m_hActiveHopWire->Detonate();
		m_hActiveHopWire = NULL;

		// Don't allow another throw for awhile
		m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 2.0f;

		return;
	}

	// One live hopwire at a time.
	if ( !HasPrimaryAmmo() || m_hActiveHopWire != NULL )
		return;

	// The beta hopwire model has a direct throw and its projectile event,
	// but no retail pullback activity or sequence-finished event. Waiting for
	// that missing event leaves the weapon locked at FLT_MAX forever.
	m_AttackPaused = GRENADE_PAUSED_NO;
	m_fDrawbackFinished = false;
	SendWeaponAnim( ACT_VM_THROW );
	m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack =
		gpGlobals->curtime + MAX( 0.2f, SequenceDuration() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponVortexHopwire::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}
}

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponVortexHopwire::CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc )
{
	trace_t tr;

	UTIL_TraceHull( vecEye, vecSrc, -Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), 
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr );
	
	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::ThrowGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	vForward[2] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 1200;
	m_hActiveHopWire = static_cast<CGrenadeVortexHopwire *> (VortexHopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GRENADE_TIMER ));
	if ( !m_hActiveHopWire )
		return false;

	m_bRedraw = true;

	WeaponSound( SINGLE );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::LobGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector( 0, 0, -8 );
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	
	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 350 + Vector( 0, 0, 50 );
	m_hActiveHopWire = static_cast<CGrenadeVortexHopwire *> (VortexHopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(200,random->RandomInt(-600,600),0), pPlayer, GRENADE_TIMER ));
	if ( !m_hActiveHopWire )
		return false;

	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
bool CWeaponVortexHopwire::RollGrenade( CBasePlayer *pPlayer )
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecSrc );
	vecSrc.z += GRENADE_RADIUS;

	Vector vecFacing = pPlayer->BodyDirection2D( );
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc - Vector(0,0,16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct( vecFacing, tr.plane.normal, tangent );
		CrossProduct( tr.plane.normal, tangent, vecFacing );
	}
	vecSrc += (vecFacing * 18.0);
	CheckThrowPosition( pPlayer, pPlayer->WorldSpaceCenter(), vecSrc );

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vecFacing * 700;
	// put it on its side
	QAngle orientation(0,pPlayer->GetLocalAngles().y,-90);
	// roll it
	AngularImpulse rotSpeed(0,0,720);
	m_hActiveHopWire = static_cast<CGrenadeVortexHopwire *> (VortexHopWire_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, GRENADE_TIMER ));
	if ( !m_hActiveHopWire )
		return false;

	WeaponSound( SPECIAL1 );

	m_bRedraw = true;
	return true;
}
