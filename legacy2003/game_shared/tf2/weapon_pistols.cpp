//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:	Recon's dual semi-auto pistols
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "in_buttons.h"
#include "gamerules.h"
#include "ammodef.h"
#include "basegrenade_shared.h"

ConVar	weapon_pistols_damage( "weapon_pistols_damage","10", FCVAR_NONE, "Recon pistols maximum damage" );
ConVar	weapon_pistols_range( "weapon_pistols_range","1000", FCVAR_NONE, "Recon pistols maximum range" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponPistols : public CTFMachineGun
{
	DECLARE_CLASS( CWeaponPistols, CTFMachineGun );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CWeaponPistols():m_flSoonestPrimaryAttack(0),m_flLastPrimaryAttack(0) {}
	virtual void Precache(void);
	virtual float			GetFireRate( void );
	virtual const Vector&	GetBulletSpread( void ); 
	virtual bool			Deploy( void );
	virtual void			PrimaryAttack( void );
	virtual void			SecondaryAttack( void );
	virtual void			ItemPostFrame( void );

private:
	float		m_flSoonestPrimaryAttack;
	float		m_flLastPrimaryAttack;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponPistols,DT_WeaponPistols)
END_SEND_TABLE()
BEGIN_DATADESC(CWeaponPistols)
    DEFINE_FIELD(CWeaponPistols,m_flSoonestPrimaryAttack,FIELD_TIME),
    DEFINE_FIELD(CWeaponPistols,m_flLastPrimaryAttack,FIELD_TIME),
END_DATADESC()
LINK_ENTITY_TO_CLASS( weapon_pistols, CWeaponPistols );
PRECACHE_WEAPON_REGISTER(weapon_pistols);

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponPistols::GetBulletSpread( void )
{
	static Vector cone;
	cone = VECTOR_CONE_10DEGREES;

	// Modify accuracy based upon firing rate
	// Maximum accuracy's used if you're firing at the standard rate of the gun
	float flModifier = min( GetFireRate(), gpGlobals->curtime - m_flLastPrimaryAttack );
	flModifier = 1.0 - RemapVal( flModifier, 0, GetFireRate(), 0, 1.0 );
	cone *= flModifier;

	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponPistols::GetFireRate( void )
{	
	return 0.5; 
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
bool CWeaponPistols::Deploy( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flLastPrimaryAttack = gpGlobals->curtime;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistols::PrimaryAttack( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime + 0.1;
	BaseClass::PrimaryAttack();
	m_flLastPrimaryAttack = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Throw out a sticky bomb
//-----------------------------------------------------------------------------
void CWeaponPistols::SecondaryAttack( void )
{
    // HL2MODERN_PISTOLS_STICKY: activate the supplied recon grenade.
    CBasePlayer *player=ToBasePlayer(GetOwner());
    if (!player || !player->IsAlive() || IsOwnerEMPed() ||
        m_flNextSecondaryAttack>gpGlobals->curtime || m_iSecondaryAmmoType<0 ||
        player->GetAmmoCount(m_iSecondaryAmmoType)<=0) return;
    Vector forward;
    player->EyeVectors(&forward);
    Vector origin=player->Weapon_ShootPosition();
    trace_t trace;
    UTIL_TraceHull(origin,origin+forward*16,Vector(-4,-4,-4),Vector(4,4,4),
        MASK_SOLID,player,COLLISION_GROUP_NONE,&trace);
    if (trace.startsolid || trace.allsolid) { m_flNextSecondaryAttack=gpGlobals->curtime+0.2f; return; }
    QAngle angles;
    VectorAngles(forward,angles);
    CBaseGrenade *grenade=dynamic_cast<CBaseGrenade *>(CBaseEntity::Create("grenade_stickybomb",trace.endpos,angles,player));
    if (!grenade || grenade->IsMarkedForDeletion()) { m_flNextSecondaryAttack=gpGlobals->curtime+0.2f; return; }
    grenade->SetOwner(player);
    grenade->SetOwnerEntity(player);
    grenade->SetAbsVelocity(forward*1200);
    grenade->ChangeTeam(player->GetTeamNumber());
    player->RemoveAmmo(1,m_iSecondaryAmmoType);
    PlayAttackAnimation(ACT_VM_SECONDARYATTACK);
    m_flNextSecondaryAttack=gpGlobals->curtime+0.5f;
    m_flNextPrimaryAttack=max((float)m_flNextPrimaryAttack,(float)m_flNextSecondaryAttack);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistols::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// Allow a refire as fast as the player can click
	if ( ( ( pPlayer->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime )
		&& ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}

	BaseClass::ItemPostFrame();
}


void CWeaponPistols::Precache(void)
{
    BaseClass::Precache();
    UTIL_PrecacheOther("grenade_stickybomb");
    // Older scripts omitted the secondary slot; use the branch's real 3-grenade pool.
    if (m_iSecondaryAmmoType<0) m_iSecondaryAmmoType=GetAmmoDef()->Index("Grenades");
}
