// GammaSource: generated from server/episodic/grenade_hopwire.h by modernize/port2018/make_vortexhopwire.py
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef GRENADE_HOPWIRE_H
#define GRENADE_HOPWIRE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "Sprite.h"

extern ConVar hopwire_trap;

class CGravityVortexController;

class CGrenadeVortexHopwire : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeVortexHopwire, CBaseGrenade );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CGrenadeVortexHopwire() { m_bVortexActive = false; }
	void	Spawn( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	void	SetTimer( float timer );
	void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	void	Detonate( void );
	
	void	EndThink( void );		// Last think before going away
	void	CombatThink( void );	// Makes the main explosion go off

protected:

	void	KillStriders( void );

	CHandle<CGravityVortexController>	m_hVortexController;
	CNetworkVar( bool, m_bVortexActive );
};

extern CBaseGrenade *VortexHopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer );

#endif // GRENADE_HOPWIRE_H
