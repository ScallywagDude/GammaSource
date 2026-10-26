//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Stun Stick- beating stick with a zappy end
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib.h"
#include "in_buttons.h"
#include "soundent.h"
#include "BaseBludgeonWeapon.h"
#include "decals.h"
#include "AI_BaseNPC.h"
#include "npcevent.h"

#define	STUNSTICK_RANGE		48
#define	STUNSTICK_REFIRE	0.7f	// v_stunbaton attack sequences last 0.67s
#define	STUNSTICK_HIT_DELAY	0.25f	// the baton connects about a third into the swing

ConVar    sk_plr_dmg_stunstick	( "sk_plr_dmg_stunstick","0");
ConVar    sk_npc_dmg_stunstick	( "sk_npc_dmg_stunstick","0");

//-----------------------------------------------------------------------------
// CWeaponStunStick
//-----------------------------------------------------------------------------

class CWeaponStunStick : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CWeaponStunStick, CBaseHLBludgeonWeapon );
public:

	CWeaponStunStick();

	DECLARE_SERVERCLASS();

	float		GetRange( void )		{ return STUNSTICK_RANGE; }
	float		GetFireRate( void )		{ return STUNSTICK_REFIRE; }

	void		ImpactEffect( trace_t &traceHit );

	float		GetDamageForActivity( Activity hitActivity );

	void		SecondaryAttack( void )	{}
	void		PrimaryAttack( void );
	void		ItemPostFrame( void );
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	bool		TraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity );

	float		m_flDelayedHitTime;	// 0 when no swing is waiting to connect
};

IMPLEMENT_SERVERCLASS_ST(CWeaponStunStick, DT_WeaponStunStick)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_stunstick, CWeaponStunStick );
PRECACHE_WEAPON_REGISTER( weapon_stunstick );

BEGIN_DATADESC( CWeaponStunStick )
	DEFINE_FIELD( CWeaponStunStick, m_flDelayedHitTime, FIELD_TIME ),
END_DATADESC()

acttable_t CWeaponStunStick::m_acttable[] = 
{
	{ ACT_MELEE_ATTACK1, ACT_MELEE_ATTACK_SWING, true },
};

IMPLEMENT_ACTTABLE(CWeaponStunStick);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponStunStick::CWeaponStunStick( void )
{
	m_flDelayedHitTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Same reach test as the base swing; true when something is in range
//-----------------------------------------------------------------------------
bool CWeaponStunStick::TraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity )
{
	static const Vector mins( -16, -16, -16 ), maxs( 16, 16, 16 );
	Vector swingStart = pOwner->Weapon_ShootPosition();
	Vector forward;
	pOwner->EyeVectors( &forward, NULL, NULL );

	hitActivity = ACT_VM_HITCENTER;
	UTIL_TraceLine( swingStart, swingStart + forward * GetRange(), MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	if ( traceHit.fraction == 1.0f )
	{
		UTIL_TraceHull( swingStart, swingStart + forward * GetRange(), mins, maxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
		if ( traceHit.fraction < 1.0f )
		{
			hitActivity = ChooseIntersectionPointAndActivity( traceHit, mins, maxs, pOwner );
		}
	}
	return traceHit.fraction < 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Start the swing now; the impact is applied when the baton connects
//-----------------------------------------------------------------------------
void CWeaponStunStick::PrimaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	trace_t traceHit;
	Activity hitActivity;
	SendWeaponAnim( TraceSwing( pOwner, traceHit, hitActivity ) ? hitActivity : ACT_VM_MISSCENTER );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
	m_flDelayedHitTime = gpGlobals->curtime + STUNSTICK_HIT_DELAY;

	WeaponSound( SINGLE );
}

//-----------------------------------------------------------------------------
// Purpose: Apply a pending swing's impact at the right moment
//-----------------------------------------------------------------------------
void CWeaponStunStick::ItemPostFrame( void )
{
	if ( m_flDelayedHitTime > 0.0f && gpGlobals->curtime >= m_flDelayedHitTime )
	{
		m_flDelayedHitTime = 0.0f;

		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		trace_t traceHit;
		Activity hitActivity;
		if ( pOwner && TraceSwing( pOwner, traceHit, hitActivity ) )
		{
			Hit( traceHit, hitActivity );
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponStunStick::GetDamageForActivity( Activity hitActivity )
{
	if ( ( GetOwner() != NULL ) && ( GetOwner()->IsPlayer() ) )
		return sk_plr_dmg_stunstick.GetFloat();
	
	return sk_npc_dmg_stunstick.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponStunStick::ImpactEffect( trace_t &traceHit )
{
	//Glowing spark effect for hit
	//UTIL_DecalTrace( &m_trLineHit, "PlasmaGlowFade" );
	
	//FIXME: need new decals
	UTIL_ImpactTrace( &traceHit, DMG_CLUB );
}

void CWeaponStunStick::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_MELEE_HIT:
		{
			Vector vecShootOrigin = pOperator->Weapon_ShootPosition();
			CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), sk_npc_dmg_stunstick.GetFloat(), DMG_SHOCK | DMG_CLUB );
			
			// did I hit someone?
			if ( pHurt )
			{
				// play sound
				WeaponSound( MELEE_HIT );
				// punch angles
				// do effect?
			}
			else
			{
				WeaponSound( MELEE_MISS );
			}
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}
