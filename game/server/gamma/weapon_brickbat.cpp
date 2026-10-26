//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: This is the brickbat weapon
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "AI_Memory.h"
#include "player.h"
#include "gamerules.h"		// For g_pGameRules
#include "weapon_brickbat.h"
#include "grenade_brickbat.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "game.h"			
#include "IEffects.h"
#include "vstdlib/random.h"
#include "baseviewmodel.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sk_npc_dmg_brickbat;
extern ConVar sk_plr_dmg_brickbat;

struct BrickbatAmmo_s
{
	const char	*m_sClassName;
	int			m_nAmmoType;
	int			m_nMaxCarry;
	const char	*m_sViewModel;
	const char	*m_sWorldModel;
};

BrickbatAmmo_s	BrickBatAmmoArray[NUM_BRICKBAT_AMMO_TYPES] =
{
	{ "grenade_rockbb",			BRICKBAT_ROCK,			5,	"models/weapons/v_bb_bottle.mdl",		"models/props_junk/Rock001a.mdl" },
	{ "grenade_beerbottle",		BRICKBAT_BOTTLE,		3,	"models/weapons/v_bb_bottle.mdl",		"models/weapons/w_bb_bottle.mdl" },
	{ "grenade_crematorhead",	BRICKBAT_CREMATORHEAD,	1,	"models/weapons/v_bb_crematorhead.mdl",	"models/cremator_head.mdl" },
};

IMPLEMENT_SERVERCLASS_ST(CWeaponBrickbat, DT_WeaponBrickbat)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_brickbat, CWeaponBrickbat );
PRECACHE_WEAPON_REGISTER(weapon_brickbat);

acttable_t	CWeaponBrickbat::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_THROW, true },
};
IMPLEMENT_ACTTABLE(CWeaponBrickbat);



BEGIN_DATADESC( CWeaponBrickbat )

	DEFINE_FIELD( m_bNeedDraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bNeedThrow, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iThrowBits, FIELD_INTEGER ),
	DEFINE_FIELD( m_fNextThrowCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_ARRAY( m_nAmmoCount, FIELD_INTEGER, NUM_BRICKBAT_AMMO_TYPES ),
	DEFINE_KEYFIELD( m_iCurrentAmmoType, FIELD_INTEGER, "BrickbatType" ),

	// Function Pointers
	DEFINE_ENTITYFUNC( BrickbatTouch ),

END_DATADESC()

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWeaponBrickbat::Precache( void )
{
	for (int i=0;i<ARRAYSIZE(BrickBatAmmoArray);i++)
	{
		engine->PrecacheModel(BrickBatAmmoArray[i].m_sWorldModel);
		engine->PrecacheModel(BrickBatAmmoArray[i].m_sViewModel);
	}

	for (int type=0;type<NUM_BRICKBAT_AMMO_TYPES;++type)
		UTIL_PrecacheOther(BrickBatAmmoArray[type].m_sClassName);

	BaseClass::Precache();
}

void CWeaponBrickbat::Spawn( void )
{
    m_iCurrentAmmoType=ValidAmmoType();
    m_bNeedDraw=true; m_bNeedThrow=false;
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) m_nAmmoCount[i]=0;
    m_nAmmoCount[m_iCurrentAmmoType]=1;
    BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponBrickbat::GetViewModel( int viewmodelindex /*=0*/ )
{
	return BrickBatAmmoArray[ValidAmmoType()].m_sViewModel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponBrickbat::GetWorldModel( void )
{
	return BrickBatAmmoArray[ValidAmmoType()].m_sWorldModel;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool CWeaponBrickbat::Deploy( void )
{
	SetModel( GetViewModel() );
	m_bNeedDraw		= false;
	m_bNeedThrow	= false;
	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_DRAW, (char*)GetAnimPrefix() );
}

//------------------------------------------------------------------------------
// Purpose : Override to use brickbats pickup touch function
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWeaponBrickbat::SetPickupTouch( void )
{
	SetTouch( &ThisClass::BrickbatTouch );
}

//------------------------------------------------------------------------------
// Purpose : Override base class so can be picked up with physics gun
// Input   :
// Output  :
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Override so give correct ammo
// Input  : pOther - the entity that touched me
// Output :
//-----------------------------------------------------------------------------
void CWeaponBrickbat::BrickbatTouch( CBaseEntity *pOther )
{
    CBasePlayer *player=ToBasePlayer(pOther);
    if (!player || GetOwner() || IsMarkedForDeletion() ||
        !player->Weapon_CanUse(this) || !g_pGameRules->CanHavePlayerItem(player,this)) return;
    CWeaponBrickbat *owned=dynamic_cast<CWeaponBrickbat *>(player->Weapon_OwnsThisType(GetClassname()));
    if (!owned || owned==this) { BaseClass::DefaultTouch(pOther); return; }
    bool transferred=false,remaining=false;
    for (int type=0;type<NUM_BRICKBAT_AMMO_TYPES;++type)
    {
        int available=MAX(0,BrickBatAmmoArray[type].m_nMaxCarry-owned->m_nAmmoCount[type]);
        int count=MIN(available,m_nAmmoCount[type]);
        if (count>0) { owned->m_nAmmoCount[type]+=count; m_nAmmoCount[type]-=count; transferred=true; }
        if (m_nAmmoCount[type]>0) remaining=true;
    }
    if (transferred) owned->SynchronizeAmmo();
    if (!remaining) UTIL_Remove(this);
}


//-----------------------------------------------------------------------------
// Purpose: Gets event from anim stream and throws the object
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponBrickbat::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_THROW:
		{
			CAI_BaseNPC *pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;

			if (!pNPC)
			{
				return;
			}

			Vector vec_target = pNPC->GetEnemyLKP();

			// -----------------------------------------------------
			//  Get position of throw
			// -----------------------------------------------------
			// If owner has a hand, set position to the hand bone position
			Vector launchPos;
			int iBIndex = pNPC->LookupBone("Bip01 R Hand");
			if (iBIndex != -1) {
				Vector origin;
				QAngle angles;
				pNPC->GetBonePosition( iBIndex, launchPos, angles);
			}
			// Otherwise just set to in front of the owner
			else {
				Vector vFacingDir = pNPC->BodyDirection2D( );
				vFacingDir = vFacingDir * 60.0; 
				launchPos = pNPC->GetLocalOrigin()+vFacingDir;
			}

			if (!ThrowBrickbat( launchPos, m_vecTossVelocity, sk_npc_dmg_brickbat.GetFloat())) return;

			// Drop the weapon and remove as no more ammo
			pNPC->Weapon_Drop( this );
			UTIL_Remove( this );
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponBrickbat::ObjectInWay( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	Vector vecSrc	 = pOwner->Weapon_ShootPosition( );
	Vector vecAiming = pOwner->BodyDirection2D( );

	trace_t tr;

	Vector	vecEnd = vecSrc + (vecAiming * 32);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr );
	
	if (tr.fraction < 1.0)
	{
		// Don't block on a living creature
		if (tr.m_pEnt)
		{
			CBaseEntity *pEntity = tr.m_pEnt;
			CBaseCombatCharacter *pBCC		= ToBaseCombatCharacter( pEntity );
			if (pBCC)
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override to allow throw w/o LOS
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponBrickbat::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos,bool bSetConditions)
{
	// <<TODO>> should test if can throw from present location here...
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Override to check throw
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CWeaponBrickbat::WeaponRangeAttack1Condition( float flDot, float flDist )
{
	// If things haven't changed too much since last time
	// just return that previously calculated value
	if (gpGlobals->curtime < m_fNextThrowCheck )
	{
		return m_iThrowBits;
	}
	
	if ( flDist < m_fMinRange1) 
	{
		m_iThrowBits = COND_TOO_CLOSE_TO_ATTACK;
	}
	else if (flDist > m_fMaxRange1) 
	{
		m_iThrowBits = COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.5) 
	{
		m_iThrowBits = COND_NOT_FACING_ATTACK;
	}

	// If moving, can't throw.
	else if ( m_flGroundSpeed != 0 )
	{
		m_iThrowBits = COND_NONE;
	}
	else 
	{
		// Ok we should check again as some time has passed
		// This function is only used by NPC's so we can cast to a Base Monster
		CAI_BaseNPC *pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;
		CBaseEntity *pEnemy = pNPC ? pNPC->GetEnemy() : NULL;

		if (!pEnemy)
		{
			return COND_NONE;
		}

		// Get Enemy Position 
		Vector vecTarget = Vector( pEnemy->GetAbsOrigin().x, 
									pEnemy->GetAbsOrigin().y, 
									( pEnemy->GetAbsOrigin() + pEnemy->CollisionProp()->OBBMins() ).z );

		// Get Toss Vector
		Vector			throwStart  = pNPC->Weapon_ShootPosition();
		Vector			vecToss;
		CBaseEntity*	pBlocker	= NULL;
		float			throwDist	= (throwStart - vecTarget).Length();
		float			fGravity	= sv_gravity.GetFloat(); 
		float			throwLimit	= pNPC->ThrowLimit(throwStart, vecTarget, fGravity, 35, WorldAlignMins(), WorldAlignMaxs(), pEnemy, &vecToss, &pBlocker);

		// If I can make the throw (or most of the throw)
		if (!throwLimit || (throwLimit != throwDist && throwLimit > 0.8*throwDist))
		{
			m_vecTossVelocity = vecToss;
			m_iThrowBits = COND_CAN_RANGE_ATTACK1;

		}
		else
		{
			m_iThrowBits = COND_NONE;
		}

	}
	// don't check again for a while.
	m_fNextThrowCheck = gpGlobals->curtime + 0.33; // 1/3 second.

	return m_iThrowBits;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
bool CWeaponBrickbat::ThrowBrickbat( Vector vecSrc, Vector vecVelocity, float damage)
{
	m_iCurrentAmmoType=ValidAmmoType();
	if (!GetOwner() || m_nAmmoCount[m_iCurrentAmmoType]<=0) return false;
	CGrenade_Brickbat *pBrickbat = (CGrenade_Brickbat*)Create( BrickBatAmmoArray[m_iCurrentAmmoType].m_sClassName, vecSrc, vec3_angle, GetOwner() );

	if (!pBrickbat || pBrickbat->IsMarkedForDeletion())
	{
		Msg("Brickbat type (%s) not defined!\n",BrickBatAmmoArray[m_iCurrentAmmoType].m_sClassName);
		return false;
	}

	AngularImpulse vecAngVel;
	// Tumble through the air
	vecAngVel.x = random->RandomFloat ( -500, -100 );
	vecAngVel.z = random->RandomFloat ( -500, -100 );
	vecAngVel.y = random->RandomFloat ( -500, -100 );

	// If physically simulated
	IPhysicsObject *pPhysicsObject = pBrickbat->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->AddVelocity( &vecVelocity, &vecAngVel );
	}
	// Otherwise
	else
	{
		pBrickbat->SetAbsVelocity( vecVelocity );

		QAngle angVel;
		AngularImpulseToQAngle( vecAngVel, angVel );
		pBrickbat->SetLocalAngularVelocity( angVel );
	}

	pBrickbat->SetThrower( GetOwner() );
	pBrickbat->SetOwnerEntity( ((CBaseEntity*)GetOwner()) );
	pBrickbat->SetDamage(damage);

	m_nAmmoCount[m_iCurrentAmmoType]--;
	SynchronizeAmmo();
	m_bNeedThrow = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponBrickbat::PrimaryAttack( void )
{

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer || !pPlayer->IsAlive() || m_bNeedThrow ||
	    m_nAmmoCount[ValidAmmoType()]<=0 || m_flNextPrimaryAttack>gpGlobals->curtime) return;

	SendWeaponAnim(ACT_VM_PULLBACK);
	
	// Don't fire again until fire animation has completed
	float flSequenceEndTime = gpGlobals->curtime + SequenceDuration();
	pPlayer->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = flSequenceEndTime;

	m_bNeedThrow = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponBrickbat::Throw( void )
{

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	Vector vecSrc		= pPlayer->WorldSpaceCenter();
	Vector vecFacing	= pPlayer->BodyDirection3D( );
	vecSrc				= vecSrc + vecFacing * 18.0;
	vecSrc.z		   += 24.0f;

	// Player may have turned to face a wall during the throw anim in which case
	// we don't want to throw the SLAM into the wall
	if (ObjectInWay())
	{
		vecSrc = pPlayer->WorldSpaceCenter() + vecFacing * 5.0;
	}

	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	vecAiming.z += 0.20; // Raise up so passes through reticle

	if (!ThrowBrickbat(vecSrc, vecAiming*800, sk_plr_dmg_brickbat.GetFloat()))
	{
		m_bNeedThrow=false;
		m_bNeedDraw=true;
		m_flNextPrimaryAttack=gpGlobals->curtime+0.2f;
		return;
	}

	SendWeaponAnim(ACT_VM_THROW);
	
	// Don't fire again until fire animation has completed
	float flSequenceEndTime = gpGlobals->curtime + SequenceDuration();
	pPlayer->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = flSequenceEndTime;

	m_bNeedThrow = false;
	m_bNeedDraw	 = true;
}
//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponBrickbat::SecondaryAttack( void )
{
	m_iCurrentAmmoType=ValidAmmoType();
	int counter = 0;
	while (counter < NUM_BRICKBAT_AMMO_TYPES)
	{
		m_iCurrentAmmoType = ((++m_iCurrentAmmoType)%NUM_BRICKBAT_AMMO_TYPES);

		// If I've found a category with ammo stop looking
		if (m_nAmmoCount[m_iCurrentAmmoType] > 0)
		{
			DrawAmmo();
			return;
		}
		counter++;
	}
	// I'm out of all ammo types
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponBrickbat::DrawAmmo( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner) return;
	m_iCurrentAmmoType=ValidAmmoType();

	// -------------------------------------------
	// Make sure I have ammo of the current type
	// -------------------------------------------
	int counter = 0;
	while (m_nAmmoCount[m_iCurrentAmmoType] <=0)
	{
		m_iCurrentAmmoType = ((++m_iCurrentAmmoType)%NUM_BRICKBAT_AMMO_TYPES);
		counter++;

		// ----------------------------------------------------
		// No ammo of any types so drop the weapon and destroy
		// ----------------------------------------------------
		if (counter >= NUM_BRICKBAT_AMMO_TYPES)
		{
			pOwner->Weapon_Drop( this, NULL, NULL );
			UTIL_Remove(this);
			return;
		}
	}
	SetModel( BrickBatAmmoArray[ValidAmmoType()].m_sViewModel);
	CBaseViewModel *vm = pOwner->GetViewModel();
	if ( vm )
	{
		vm->SetModel( BrickBatAmmoArray[ValidAmmoType()].m_sViewModel );
	}

	//Msg("Drawing %s...\n",BrickBatAmmoArray[m_iCurrentAmmoType].m_sClassName);
	m_bNeedDraw = false;

	SendWeaponAnim(ACT_VM_DRAW);

	// Don't fire again until fire animation has completed
	float flSequenceEndTime = gpGlobals->curtime + SequenceDuration();
	pOwner->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = flSequenceEndTime;
}

//-----------------------------------------------------------------------------
// Purpose: Override so shotgun can do mulitple reloads in a row
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponBrickbat::ItemPostFrame( void )
{
    CBasePlayer *owner=ToBasePlayer(GetOwner());
    if (!owner || !owner->IsAlive()) { m_bNeedThrow=false; return; }
    SynchronizeAmmo();
    if (m_bNeedThrow)
    {
        if (m_flNextPrimaryAttack<=gpGlobals->curtime) Throw();
        return;
    }
    if (m_bNeedDraw && m_flNextPrimaryAttack<=gpGlobals->curtime) { DrawAmmo(); return; }
    if ((owner->m_nButtons&IN_ATTACK2) && m_flNextSecondaryAttack<=gpGlobals->curtime) SecondaryAttack();
    else if ((owner->m_nButtons&IN_ATTACK) && m_flNextPrimaryAttack<=gpGlobals->curtime)
    {
        if (m_nAmmoCount[ValidAmmoType()]>0) PrimaryAttack();
        else DrawAmmo();
    }
    else WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponBrickbat::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( info.GetDamageType() & DMG_BULLET)
	{
		if (BrickBatAmmoArray[m_iCurrentAmmoType].m_nAmmoType == BRICKBAT_CREMATORHEAD ||
			BrickBatAmmoArray[m_iCurrentAmmoType].m_nAmmoType == BRICKBAT_ROCK			)
		{
			g_pEffects->Ricochet(ptr->endpos,ptr->plane.normal);
		}	
	}
	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponBrickbat::CWeaponBrickbat( void )
{
	m_iCurrentAmmoType=BRICKBAT_ROCK;
	m_bNeedDraw=true; m_bNeedThrow=false; m_iThrowBits=COND_NONE; m_fNextThrowCheck=0;
	m_vecTossVelocity.Init();
	for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) m_nAmmoCount[i]=0;
#ifdef _DEBUG
	m_vecTossVelocity.Init();
#endif

	m_fMinRange1	= 200;
	m_fMaxRange1	= 1000;
}

// HL2MODERN_BRICKBAT_INVENTORY: preserve the original per-type carry limits.
bool CWeaponBrickbat::HasPrimaryAmmo(void)
{
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) if (m_nAmmoCount[i]>0) return true;
    return false;
}

void CWeaponBrickbat::SynchronizeAmmo(void)
{
    CBaseCombatCharacter *owner=GetOwner();
    if (!owner || m_iPrimaryAmmoType<0) return;
    int total=0;
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) total+=m_nAmmoCount[i];
    int reserve=owner->GetAmmoCount(m_iPrimaryAmmoType);
    if (reserve>total) owner->RemoveAmmo(reserve-total,m_iPrimaryAmmoType);
    else if (reserve<total) owner->GiveAmmo(total-reserve,m_iPrimaryAmmoType);
}

void CWeaponBrickbat::Equip(CBaseCombatCharacter *owner)
{
    BaseClass::Equip(owner);
    SynchronizeAmmo();
}

bool CWeaponBrickbat::Holster(CBaseCombatWeapon *switchingTo)
{
    if (!BaseClass::Holster(switchingTo)) return false;
    m_bNeedThrow=false; m_bNeedDraw=true;
    return true;
}

void CWeaponBrickbat::Drop(const Vector &velocity)
{
    m_bNeedThrow=false; m_bNeedDraw=true;
    BaseClass::Drop(velocity);
}

// Fill the real per-variant inventory, not just its displayed reserve mirror.
void CWeaponBrickbat::GiveMaxAmmo(void)
{
    if (!GetOwner() || m_iPrimaryAmmoType<0) return;
    int remaining=MAX(0,GetAmmoDef()->MaxCarry(m_iPrimaryAmmoType));
    const int first=ValidAmmoType();
    for (int offset=0;offset<NUM_BRICKBAT_AMMO_TYPES;++offset)
    {
        const int type=(first+offset)%NUM_BRICKBAT_AMMO_TYPES;
        m_nAmmoCount[type]=MIN(BrickBatAmmoArray[type].m_nMaxCarry,remaining);
        remaining-=m_nAmmoCount[type];
    }
    SynchronizeAmmo();
}

void GammaGiveMaxBrickbatAmmo(CBasePlayer *player)
{
    CWeaponBrickbat *weapon=player ? dynamic_cast<CWeaponBrickbat *>(player->Weapon_OwnsThisType("weapon_brickbat")) : NULL;
    if (weapon) weapon->GiveMaxAmmo();
}

void CWeaponBrickbat::PrintAmmoState(void)
{
    CBaseCombatCharacter *owner=GetOwner();
    Msg("GAMMA_BRICKBAT_VARIANTS active=%d rock=%d/5 bottle=%d/3 head=%d/1 reserve=%d\n",
        ValidAmmoType(),m_nAmmoCount[BRICKBAT_ROCK],m_nAmmoCount[BRICKBAT_BOTTLE],
        m_nAmmoCount[BRICKBAT_CREMATORHEAD],owner && m_iPrimaryAmmoType>=0 ? owner->GetAmmoCount(m_iPrimaryAmmoType) : 0);
}

CON_COMMAND_F(gamma_brickbat_info,"Print the owned brickbat's per-variant ammunition",FCVAR_CHEAT)
{
    CBasePlayer *player=UTIL_GetLocalPlayer();
    CWeaponBrickbat *weapon=player ? dynamic_cast<CWeaponBrickbat *>(player->Weapon_OwnsThisType("weapon_brickbat")) : NULL;
    if (weapon) weapon->PrintAmmoState();
}
