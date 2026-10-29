//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/weapon_immolator.cpp) ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "player.h"
#include "soundent.h"
#include "te_particlesystem.h"
#include "ndebugoverlay.h"
#include "in_buttons.h"
#include "ai_basenpc.h"
#include "ai_memory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MAX_BURN_RADIUS		256
#define RADIUS_GROW_RATE	50.0	// units/sec 
#define IMMOLATOR_TICK_INTERVAL 0.05f
#define IMMOLATOR_FUEL_INTERVAL 0.2f

#define IMMOLATOR_TARGET_INVALID Vector( FLT_MAX, FLT_MAX, FLT_MAX )

static ConVar gamma_immolator_debug( "gamma_immolator_debug", "0", FCVAR_CHEAT,
	"Report immolator attack, damage, fuel and shutdown state." );

class CWeaponImmolator : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponImmolator, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	CWeaponImmolator( void );
	
	void Precache( void );
	void PrimaryAttack( void );
	void ItemPostFrame( void );
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void Drop( const Vector &vecVelocity );
	void UpdateOnRemove();
	void OnRestore();

	int CapabilitiesGet( void ) {	return bits_CAP_WEAPON_RANGE_ATTACK1;	}

	void ImmolationDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );
	virtual bool WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );	
	virtual int	WeaponRangeAttack1Condition( float flDot, float flDist );

	void Update();
	void UpdateThink();

	void StartImmolating();
	void StopImmolating();
	bool IsImmolating() { return m_flBurnRadius != 0.0; }

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

	int	m_beamIndex;

	float m_flBurnRadius;
	float m_flTimeLastUpdatedRadius;
	float m_flNextFuelUse;
	float m_flNextPlayerGesture;
	float m_flNextChargeAllowed;

	Vector  m_vecImmolatorTarget;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponImmolator, DT_WeaponImmolator)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( info_target_immolator, CPointEntity );
LINK_ENTITY_TO_CLASS( weapon_immolator, CWeaponImmolator );
PRECACHE_WEAPON_REGISTER( weapon_immolator );

BEGIN_DATADESC( CWeaponImmolator )

	DEFINE_FIELD( m_beamIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_flBurnRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeLastUpdatedRadius, FIELD_TIME ),
	DEFINE_FIELD( m_flNextFuelUse, FIELD_TIME ),
	DEFINE_FIELD( m_flNextPlayerGesture, FIELD_TIME ),
	DEFINE_FIELD( m_flNextChargeAllowed, FIELD_TIME ),
	DEFINE_FIELD( m_vecImmolatorTarget, FIELD_VECTOR ),

	DEFINE_ENTITYFUNC( UpdateThink ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t CWeaponImmolator::m_acttable[] = 
{
	{	ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SNIPER_RIFLE, true }
};

IMPLEMENT_ACTTABLE( CWeaponImmolator );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponImmolator::CWeaponImmolator( void )
{
	m_fMaxRange1 = 4096;
	m_vecImmolatorTarget = IMMOLATOR_TARGET_INVALID;
	m_flBurnRadius = 0.0f;
	m_flTimeLastUpdatedRadius = 0.0f;
	m_flNextFuelUse = m_flNextPlayerGesture = 0.0f;
	m_flNextChargeAllowed = 0.0f;
}

void CWeaponImmolator::StartImmolating()
{
	if ( IsImmolating() || gpGlobals->curtime < m_flNextChargeAllowed ) return;
	CBaseCombatCharacter *owner = GetOwner();
	if ( !owner || !owner->IsAlive() ) return;
	if ( owner->IsPlayer() && ( m_iPrimaryAmmoType < 0 || owner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ) ) return;
	// Start the radius really tiny because we use radius == 0.0 to 
	// determine whether the immolator is operating or not.
	m_flBurnRadius = 0.1;
	m_flTimeLastUpdatedRadius = gpGlobals->curtime;
	m_flNextFuelUse = gpGlobals->curtime;
	m_flNextPlayerGesture = gpGlobals->curtime;
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	SetWeaponIdleTime( gpGlobals->curtime + 1.0f );
	EmitSound( "Weapon_Immolator.Start" );
	EmitSound( "Weapon_Immolator.Run" );
	m_flNextPrimaryAttack = gpGlobals->curtime + IMMOLATOR_TICK_INTERVAL;
	SetThink( &CWeaponImmolator::UpdateThink );
	SetNextThink( gpGlobals->curtime );
	if ( gamma_immolator_debug.GetBool() ) Msg( "IMMOLATOR start owner=%d\n", owner->entindex() );
}

void CWeaponImmolator::StopImmolating()
{
	const bool wasImmolating = IsImmolating();
	m_flBurnRadius = 0.0;
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
	StopSound( "Weapon_Immolator.Run" );
	StopSound( "Weapon_Immolator.Start" );
	if ( wasImmolating )
	{
		SendWeaponAnim( ACT_VM_IDLE );
		EmitSound( "Weapon_Immolator.Off" );
		m_flNextPrimaryAttack = gpGlobals->curtime + 2.0f;
		m_flNextChargeAllowed = m_flNextPrimaryAttack;
		if ( gamma_immolator_debug.GetBool() ) Msg( "IMMOLATOR stop owner=%d\n", GetOwner() ? GetOwner()->entindex() : 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponImmolator::Precache( void )
{
	m_beamIndex = engine->PrecacheModel( "sprites/bluelaser1.vmt" );
	PrecacheScriptSound( "Weapon_Immolator.Start" );
	PrecacheScriptSound( "Weapon_Immolator.Run" );
	PrecacheScriptSound( "Weapon_Immolator.Off" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponImmolator::PrimaryAttack( void )
{
	if( !IsImmolating() )
	{
		StartImmolating();
	} 
}

//-----------------------------------------------------------------------------
// This weapon is said to have Line of Sight when it CAN'T see the target, but
// can see a place near the target than can.
//-----------------------------------------------------------------------------
bool CWeaponImmolator::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	CBaseCombatCharacter *owner=GetOwner();
	CAI_BaseNPC* npcOwner=owner ? owner->MyNPCPointer() : NULL;
	if ( !npcOwner ) return false;

	if( IsImmolating() )
	{
		// Don't update while Immolating. This is a committed attack.
		return false;
	}

	// Assume we won't find a target.
	m_vecImmolatorTarget = IMMOLATOR_TARGET_INVALID;

	if( npcOwner->FInViewCone( targetPos ) && npcOwner->FVisible( targetPos ) )
	{
		// We can see the target (usually the enemy). Don't fire the immolator directly at them.
		return false;
	}

	// Find a nearby immolator target that CAN see targetPOS
	float flNearest = FLT_MAX;
	CBaseEntity *pNearest = NULL;

	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "info_target_immolator" );

	while( pEnt )
	{
		float flDist = ( pEnt->GetAbsOrigin() - targetPos ).Length();

		if( flDist < flNearest )
		{
			trace_t tr;

			UTIL_TraceLine( targetPos, pEnt->GetAbsOrigin(), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

			if( tr.fraction == 1.0 )
			{
				pNearest = pEnt;
				flNearest = flDist;
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "info_target_immolator" );
	}

	if( pNearest )
	{
		m_vecImmolatorTarget = pNearest->GetAbsOrigin();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Weapon firing conditions
//-----------------------------------------------------------------------------
int CWeaponImmolator::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	if( IsImmolating() )
	{
		// Once is enough!
		return COND_NONE;
	}

	if(	m_vecImmolatorTarget == IMMOLATOR_TARGET_INVALID )
	{
		// No target!
		return COND_NONE;
	}

	if ( flDist > m_fMaxRange1 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDot < 0.5f )	// UNDONE: Why check this here? Isn't the AI checking this already?
	{
		return COND_NOT_FACING_ATTACK;
	}

	// !!!HACKHACK this makes the strider work! 
	// It's the Immolator's RangeAttack1, but it's the Strider's Range Attack 2.
	return COND_CAN_RANGE_ATTACK2;
}

void CWeaponImmolator::UpdateThink( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if( !IsImmolating() || !pOwner || !pOwner->IsAlive() ||
		( pOwner->IsPlayer() && ( pOwner->GetActiveWeapon() != this || pOwner->GetWaterLevel() == 3 ) ) )
	{
		StopImmolating();
		return;
	}

	Update();
	// Update can stop the attack. Do not revive its think after that shutdown.
	if ( IsImmolating() ) SetNextThink( gpGlobals->curtime + IMMOLATOR_TICK_INTERVAL );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponImmolator::Update()
{
	CBaseCombatCharacter *owner = GetOwner();
	if ( !owner ) { StopImmolating(); return; }
	if ( owner->IsPlayer() && gpGlobals->curtime >= m_flNextFuelUse )
	{
		if ( m_iPrimaryAmmoType < 0 || owner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		{
			StopImmolating(); return;
		}
		owner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		m_flNextFuelUse = gpGlobals->curtime + IMMOLATOR_FUEL_INTERVAL;
	}
	if ( owner->IsPlayer() && gpGlobals->curtime >= m_flNextPlayerGesture )
	{
		ToBasePlayer( owner )->SetAnimation( PLAYER_ATTACK1 );
		m_flNextPlayerGesture = gpGlobals->curtime + 0.35f;
	}
	if ( GetActivity() != ACT_VM_PRIMARYATTACK ) SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	SetWeaponIdleTime( gpGlobals->curtime + 1.0f );
	m_flNextPrimaryAttack = gpGlobals->curtime + IMMOLATOR_TICK_INTERVAL;
	float flDuration = gpGlobals->curtime - m_flTimeLastUpdatedRadius;
	if( flDuration != 0.0 )
	{
		m_flBurnRadius += RADIUS_GROW_RATE * flDuration;
	}

	// Clamp
	m_flBurnRadius = MIN( m_flBurnRadius, MAX_BURN_RADIUS );

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	Vector vecSrc;
	Vector vecAiming;

	if( pOwner )
	{
		vecSrc	 = pOwner->Weapon_ShootPosition( );
		vecAiming = pOwner->GetAutoaimVector(AUTOAIM_2DEGREES);
	}
	else
	{
		CBaseCombatCharacter *pOwner = GetOwner();

		vecSrc = pOwner->Weapon_ShootPosition( );
		vecAiming = m_vecImmolatorTarget - vecSrc;
		VectorNormalize( vecAiming );
	}

	trace_t	tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * m_fMaxRange1, MASK_SHOT, owner, COLLISION_GROUP_NONE, &tr );

	int brightness;
	brightness = 255 * (m_flBurnRadius/MAX_BURN_RADIUS);
	UTIL_Beam(  vecSrc,
				tr.endpos,
				m_beamIndex,
				0,		//halo index
				0,		//frame start
				2.0f,	//framerate
				0.1f,	//life
				20,		// width
				1,		// endwidth
				0,		// fadelength,
				1,		// noise

				0,		// red
				255,	// green
				0,		// blue,

				brightness, // bright
				100  // speed
				);


	if( tr.DidHitWorld() )
	{
		int beams;

		for( beams = 0 ; beams < 5 ; beams++ )
		{
			Vector vecDest;

			// Random unit vector
			vecDest.x = random->RandomFloat( -1, 1 );
			vecDest.y = random->RandomFloat( -1, 1 );
			vecDest.z = random->RandomFloat( 0, 1 );

			// Push out to radius dist.
			vecDest = tr.endpos + vecDest * m_flBurnRadius;

			UTIL_Beam(  tr.endpos,
						vecDest,
						m_beamIndex,
						0,		//halo index
						0,		//frame start
						2.0f,	//framerate
						0.15f,	//life
						20,		// width
						1.75,	// endwidth
						0.75,	// fadelength,
						15,		// noise

						0,		// red
						255,	// green
						0,		// blue,

						128, // bright
						100  // speed
						);
		}

		ImmolationDamage( CTakeDamageInfo( this, GetOwner(), 1, DMG_BURN ), tr.endpos + tr.plane.normal * 2.0f, m_flBurnRadius, CLASS_NONE );
	}
	else
	{
		// HL2MODERN_IMMOLATOR_DIRECT_HIT: use the existing burn tick on direct impacts.
		if ( tr.m_pEnt && tr.m_pEnt->m_takedamage!=DAMAGE_NO )
		{
			CTakeDamageInfo damage( this, owner, 1, DMG_BURN );
			damage.SetDamagePosition( tr.endpos );
			tr.m_pEnt->TakeDamage( damage );
			if ( gamma_immolator_debug.GetBool() ) Msg( "IMMOLATOR hit target=%d health=%d radius=%.1f ammo=%d\n", tr.m_pEnt->entindex(), tr.m_pEnt->GetHealth(), m_flBurnRadius, m_iPrimaryAmmoType >= 0 ? owner->GetAmmoCount( m_iPrimaryAmmoType ) : -1 );
		}
	}

	m_flTimeLastUpdatedRadius = gpGlobals->curtime;

	if( m_flBurnRadius >= MAX_BURN_RADIUS )
	{
		StopImmolating();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponImmolator::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	StopImmolating();
	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponImmolator::Drop( const Vector &vecVelocity )
{
	StopImmolating();
	BaseClass::Drop( vecVelocity );
}

void CWeaponImmolator::UpdateOnRemove()
{
	StopImmolating();
	BaseClass::UpdateOnRemove();
}

void CWeaponImmolator::OnRestore()
{
	BaseClass::OnRestore();
	if ( IsImmolating() && GetOwner() && GetOwner()->IsAlive() )
	{
		EmitSound( "Weapon_Immolator.Run" );
		m_flTimeLastUpdatedRadius = gpGlobals->curtime;
		m_flNextFuelUse = gpGlobals->curtime;
		SetThink( &CWeaponImmolator::UpdateThink );
		SetNextThink( gpGlobals->curtime + IMMOLATOR_TICK_INTERVAL );
	}
}

void CWeaponImmolator::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
}



void CWeaponImmolator::ImmolationDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
{
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	Vector		vecSpot;

	Vector vecSrc = vecSrcIn;

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity() )
	{
		if ( pEntity->m_takedamage != DAMAGE_NO )
		{
			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{
				continue;
			}
			if ( pEntity == GetOwner() || pEntity == this ) continue;
			// The wall impact can grow a burn field, but solids still shield actors.
			vecSpot = pEntity->BodyTarget( vecSrc );
			UTIL_TraceLine( vecSrc, vecSpot, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction != 1.0f ) continue;
			pEntity->TakeDamage( info );
		}
	}
}

CON_COMMAND_F( gamma_immolator_status, "Report the owned immolator charge, fuel and think state, including while holstered.", FCVAR_CHEAT )
{
	CBasePlayer *player = UTIL_GetLocalPlayer();
	CWeaponImmolator *weapon = player ? dynamic_cast<CWeaponImmolator *>( player->Weapon_OwnsThisType( "weapon_immolator" ) ) : NULL;
	if ( !weapon ) { Msg( "IMMOLATOR status no_owned_immolator\n" ); return; }
	Msg( "IMMOLATOR status active=%d radius=%.3f fuel=%d nextthink=%.3f activity=%d cooldown=%.3f equipped=%d\n",
		weapon->IsImmolating(), weapon->m_flBurnRadius, weapon->GetPrimaryAmmoType() >= 0 ? player->GetAmmoCount( weapon->GetPrimaryAmmoType() ) : -1,
		weapon->GetNextThink(), (int)weapon->GetActivity(), MAX(0.0f, weapon->m_flNextChargeAllowed - gpGlobals->curtime), player->GetActiveWeapon() == weapon );
}

CON_COMMAND_F( gamma_immolator_fuel, "gamma_immolator_fuel <0-1000>: set fuel for an isolated immolator test.", FCVAR_CHEAT )
{
	CBasePlayer *player = UTIL_GetLocalPlayer();
	CWeaponImmolator *weapon = player ? dynamic_cast<CWeaponImmolator *>( player->GetActiveWeapon() ) : NULL;
	if ( !weapon || args.ArgC() != 2 || weapon->GetPrimaryAmmoType() < 0 ) return;
	player->SetAmmoCount( clamp( atoi(args[1]), 0, 1000 ), weapon->GetPrimaryAmmoType() );
	Msg( "IMMOLATOR test_fuel=%d\n", player->GetAmmoCount( weapon->GetPrimaryAmmoType() ) );
}
