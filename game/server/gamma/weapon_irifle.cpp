//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/weapon_irifle.cpp) ============//
//
// Purpose: This is the incendiary rifle.
//
//=============================================================================

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "soundent.h"
#include "player.h"
#include "ieffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "weapon_flaregun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//###########################################################################
//	>> CWeaponIRifle
//###########################################################################

class CWeaponIRifle : public CBaseHLCombatWeapon
{
public:

	CWeaponIRifle();

	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CWeaponIRifle, CBaseHLCombatWeapon );

	void	Precache( void );
	bool	Deploy( void );
	bool	Reload( void );

	void	PrimaryAttack( void );
	virtual float GetFireRate( void ) { return 1; };

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	DECLARE_ACTTABLE();
};

IMPLEMENT_SERVERCLASS_ST(CWeaponIRifle, DT_WeaponIRifle)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_irifle, CWeaponIRifle );
PRECACHE_WEAPON_REGISTER(weapon_irifle);

//---------------------------------------------------------
// Activity table
//---------------------------------------------------------
acttable_t	CWeaponIRifle::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_ML, true },
};
IMPLEMENT_ACTTABLE(CWeaponIRifle);

//---------------------------------------------------------
// Constructor
//---------------------------------------------------------
CWeaponIRifle::CWeaponIRifle()
{
	m_bReloadsSingly = true;

	m_fMinRange1		= 65;
	m_fMinRange2		= 65;
	m_fMaxRange1		= 200;
	m_fMaxRange2		= 200;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CWeaponIRifle::Precache( void )
{
	UTIL_PrecacheOther("env_flare");
	BaseClass::Precache();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CWeaponIRifle::PrimaryAttack( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// HL2MODERN_IRIFLE_AMMO: consume only after a flare is created.
	if ( UsesClipsForAmmo1() ? m_iClip1<=0 : pOwner->GetAmmoCount(m_iPrimaryAmmoType)<=0 ) return;

	CFlare *pFlare = CFlare::Create( pOwner->Weapon_ShootPosition(), pOwner->EyeAngles(), pOwner, FLARE_DURATION );

	if ( pFlare == NULL )
		return;
	if ( UsesClipsForAmmo1() ) --m_iClip1;
	else pOwner->RemoveAmmo(1,m_iPrimaryAmmoType);
	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	m_flNextPrimaryAttack=gpGlobals->curtime+GetFireRate();
	pOwner->m_flNextAttack=m_flNextPrimaryAttack;

	Vector forward;
	pOwner->EyeVectors( &forward );

	pFlare->SetAbsVelocity( forward * 1500 );

	WeaponSound( SINGLE );
}

//---------------------------------------------------------
// BUGBUG - don't give ammo here.
//---------------------------------------------------------
bool CWeaponIRifle::Deploy( void )
{
	// Switching weapons must not manufacture ammunition.
	return BaseClass::Deploy();
}

// The beta model has no ACT_VM_RELOAD. Reusing the previous idle sequence
// accidentally turns a reload into its full five-second idle duration.
bool CWeaponIRifle::Reload( void )
{
    Activity activity=ACT_VM_RELOAD;
    if ( SelectWeightedSequence(activity)==ACTIVITY_NOT_AVAILABLE )
        activity=ACT_VM_DRAW;
    if ( SelectWeightedSequence(activity)==ACTIVITY_NOT_AVAILABLE )
        return false;
    if ( !DefaultReload(GetMaxClip1(),GetMaxClip2(),activity) )
        return false;
    WeaponSound(RELOAD);
    return true;
}
