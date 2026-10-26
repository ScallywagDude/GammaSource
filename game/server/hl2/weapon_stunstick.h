//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_STUNSTICK_H
#define WEAPON_STUNSTICK_H
#ifdef _WIN32
#pragma once
#endif

#include "basebludgeonweapon.h"

#define	STUNSTICK_RANGE		48.0f	// GammaSource: 2003 reach
#define	STUNSTICK_REFIRE	0.7f	// v_stunbaton attack sequences last 0.67s
#define	STUNSTICK_HIT_DELAY	0.25f	// the baton connects about a third into the swing

class CWeaponStunStick : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS( CWeaponStunStick, CBaseHLBludgeonWeapon );
	DECLARE_DATADESC();

public:

	CWeaponStunStick();

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	virtual void Precache();

	void		Spawn();

	float		GetRange( void )		{ return STUNSTICK_RANGE; }
	float		GetFireRate( void )		{ return STUNSTICK_REFIRE; }

	int			WeaponMeleeAttack1Condition( float flDot, float flDist );

	bool		Deploy( void );
	bool		Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	
	void		Drop( const Vector &vecVelocity );
	void		ImpactEffect( trace_t &traceHit );
	void		SecondaryAttack( void )	{}
	void		PrimaryAttack( void );
	void		ItemPostFrame( void );
	void		SetStunState( bool state );
	bool		GetStunState( void );
	void		Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	
	float		GetDamageForActivity( Activity hitActivity );

	bool		CanBePickedUpByNPCs( void ) { return false;	}		

private:

	CNetworkVar( bool, m_bActive );

private:
	bool		TraceSwing( CBasePlayer *pOwner, trace_t &traceHit, Activity &hitActivity );
	float		m_flDelayedHitTime;	// 0 when no swing is waiting to connect
public:
};

#endif // WEAPON_STUNSTICK_H
