//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Sticky bombs thrown by the recon
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "player.h"
#include "basegrenade_shared.h"
#include "tf_shareddefs.h"
#include "Sprite.h"


// Damage CVars
ConVar	grenade_stickybomb_damage( "grenade_stickybomb_damage","100", 0, "Recon's stickybomb maximum damage" );
ConVar	grenade_stickybomb_radius( "grenade_stickybomb_radius","100", 0, "Recon's stickybomb splash radius" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CGrenadeStickyBomb : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeStickyBomb, CBaseGrenade );
public:
	CGrenadeStickyBomb();

	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );
	void SetTimer( float timer );
	void UpdateOnRemove(void);
	void StickyTouch( CBaseEntity *pOther );
	virtual void	Explode( trace_t *pTrace, int bitsDamageType );
	virtual int		GetDamageType() const { return DMG_BLAST; }

private:
	CHandle<CSprite> m_pLiveSprite;
};

// Global Savedata for friction modifier
BEGIN_DATADESC( CGrenadeStickyBomb )
	DEFINE_FIELD(CGrenadeStickyBomb,m_pLiveSprite,FIELD_EHANDLE),

	// Function Pointers
	DEFINE_FUNCTION( CGrenadeStickyBomb, StickyTouch ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( grenade_stickybomb, CGrenadeStickyBomb );
PRECACHE_WEAPON_REGISTER(grenade_stickybomb);

CGrenadeStickyBomb::CGrenadeStickyBomb()
{
	m_pLiveSprite=NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeStickyBomb::Precache( void )
{
	BaseClass::Precache();
	engine->PrecacheModel( "models/weapons/w_grenade.mdl" );
	engine->PrecacheModel( "sprites/redglow1.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeStickyBomb::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.6 );
	SetFriction( 1.0 );
	SetModel( "models/weapons/w_grenade.mdl");
	UTIL_SetSize(this, Vector( -4, -4, -4), Vector(4, 4, 4));
	SetTouch( &ThisClass::StickyTouch );
	SetCollisionGroup( TFCOLLISION_GROUP_WEAPON );

	// Create a red light
	m_pLiveSprite = CSprite::SpriteCreate( "sprites/redglow1.vmt", GetLocalOrigin(), false );
	if (m_pLiveSprite)
	{
	m_pLiveSprite->SetTransparency( kRenderGlow, 255, 200, 200, 255, kRenderFxNoDissipation );
	m_pLiveSprite->SetBrightness( 255 );
	m_pLiveSprite->SetScale( 0.3 );
	m_pLiveSprite->SetAttachment( this, 0 );
	}

	// Set my damages to the cvar values
	SetDamage( grenade_stickybomb_damage.GetFloat() );
	SetDamageRadius( grenade_stickybomb_radius.GetFloat() );

	SetTimer( 2.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeStickyBomb::SetTimer( float timer )
{
	SetThink( &ThisClass::Detonate );
	SetNextThink( gpGlobals->curtime + timer );
}

//-----------------------------------------------------------------------------
// Purpose: Make the grenade stick to whatever it touches
//-----------------------------------------------------------------------------
void CGrenadeStickyBomb::StickyTouch( CBaseEntity *pOther )
{
	if (!pOther || pOther==GetOwnerEntity() || pOther->IsBSPModel() == false)
		return;

	BounceSound();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: Remove my glow when I'm removed
//-----------------------------------------------------------------------------
void CGrenadeStickyBomb::Explode( trace_t *pTrace, int bitsDamageType )
{
	if ( m_pLiveSprite )
	{
		UTIL_Remove( m_pLiveSprite );
		m_pLiveSprite = NULL;
	}

	BaseClass::Explode( pTrace, bitsDamageType );
}

// HL2MODERN_RECON_STICKY_LIFECYCLE: glow must also leave on non-explosive removal.
void CGrenadeStickyBomb::UpdateOnRemove(void)
{
    if (m_pLiveSprite) { UTIL_Remove(m_pLiveSprite); m_pLiveSprite=NULL; }
    BaseClass::UpdateOnRemove();
}
