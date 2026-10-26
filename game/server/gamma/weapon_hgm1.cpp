//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/weapon_hgm1.cpp) ============//
//
// Purpose: Heavy machine gun (currently an AK-47).
//
//=============================================================================

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "npcevent.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CWeaponHMG1 : public CHLSelectFireMachineGun
{
public:
	DECLARE_CLASS( CWeaponHMG1, CHLSelectFireMachineGun );

	DECLARE_SERVERCLASS();

	CWeaponHMG1();
	
	void	Precache( void );
	bool	Deploy( void );

	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_15DEGREES;
		return cone;
	}

	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
	{
		switch( pEvent->event )
		{
			case EVENT_WEAPON_HMG1:
			{
				Vector vecShootOrigin, vecShootDir;
				vecShootOrigin = pOperator->Weapon_ShootPosition( );

				CAI_BaseNPC *npc = pOperator->MyNPCPointer();
				Vector vecSpread;
				if (npc)
				{
					vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
					vecSpread = VECTOR_CONE_PRECALCULATED;
				}
				else
				{
					AngleVectors( pOperator->GetLocalAngles(), &vecShootDir );
					vecSpread = GetBulletSpread();
				}
				WeaponSound(SINGLE_NPC);
				pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, vecSpread, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
				pOperator->DoMuzzleFlash();
			}
			break;
			default:
				CBaseCombatWeapon::Operator_HandleAnimEvent( pEvent, pOperator );
				break;
		}
	}
	DECLARE_ACTTABLE();
};

IMPLEMENT_SERVERCLASS_ST(CWeaponHMG1, DT_WeaponHMG1)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_hmg1, CWeaponHMG1 );
PRECACHE_WEAPON_REGISTER( weapon_hmg1 );

acttable_t	CWeaponHMG1::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_HMG1, true },
};

IMPLEMENT_ACTTABLE(CWeaponHMG1);

//=========================================================
CWeaponHMG1::CWeaponHMG1( )
{
}

void CWeaponHMG1::Precache( void )
{
	BaseClass::Precache();
}

bool CWeaponHMG1::Deploy( void )
{
	//CBaseCombatCharacter *pOwner  = m_hOwner;
	return BaseClass::Deploy();
}
