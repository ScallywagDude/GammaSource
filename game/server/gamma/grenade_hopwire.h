//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/grenade_hopwire.h) ============//
//
// Purpose: 
//
//=============================================================================

#ifndef GRENADE_HOPWIRE_H
#define GRENADE_HOPWIRE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "sprite.h"

class CTetherHook;

class CGrenadeHopWire : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeHopWire, CBaseGrenade );
	DECLARE_DATADESC();

public:
	void	Spawn( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	void	SetTimer( float timer );
	void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	void	Detonate( void );
	void	UpdateOnRemove( void );
	void	RemoveTethers( void );
	
	void	CombatThink( void );
	void	TetherThink( void );

protected:

	int			m_nHooksShot;
	// HL2MODERN_HOPWIRE_COMBAT: saved finite stages and owned child entities.
	float m_flTetherDeadline;
	float m_flCombatDeadline;
	Vector m_vecHookVelocity;
	CHandle<CTetherHook> m_hHooks[8];
	CHandle<CSprite> m_pGlow;
};

extern CBaseGrenade *HopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer );

#endif // GRENADE_HOPWIRE_H
