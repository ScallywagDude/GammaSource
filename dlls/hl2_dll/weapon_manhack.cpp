/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/


#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "player_manhack.h"

// HL2MODERN_REMOTE_MANHACK: primary starts original remote blade control;
// secondary returns to the stationary, vulnerable player body.
class CWeapon_Manhack : public CBaseHLCombatWeapon
{
    DECLARE_CLASS(CWeapon_Manhack,CBaseHLCombatWeapon);
public:
    DECLARE_SERVERCLASS();
    DECLARE_DATADESC();
    void Precache(void);
    void PrimaryAttack(void);
    void SecondaryAttack(void);
    void ItemPostFrame(void);
    bool Holster(CBaseCombatWeapon *switchingTo=NULL);
    void Drop(const Vector &velocity);
    void UpdateOnRemove(void);
    void StopRemote(void);
private:
    CHandle<CPlayer_Manhack> m_hRemote;
};

IMPLEMENT_SERVERCLASS_ST(CWeapon_Manhack,DT_Weapon_Manhack)
END_SEND_TABLE()
LINK_ENTITY_TO_CLASS(weapon_manhack,CWeapon_Manhack);
PRECACHE_WEAPON_REGISTER(weapon_manhack);
BEGIN_DATADESC(CWeapon_Manhack)
    DEFINE_FIELD(CWeapon_Manhack,m_hRemote,FIELD_EHANDLE),
END_DATADESC()

void CWeapon_Manhack::Precache(void)
{
    BaseClass::Precache();
    UTIL_PrecacheOther("player_manhack");
}

void CWeapon_Manhack::PrimaryAttack(void)
{
    m_flNextPrimaryAttack=gpGlobals->curtime+0.5f;
    CBasePlayer *player=ToBasePlayer(GetOwner());
    if (!player || !player->IsAlive() || m_hRemote || CPlayer_Manhack::FindForPlayer(player)) return;
    Vector forward;
    player->EyeVectors(&forward);
    const Vector start=player->EyePosition();
    trace_t trace;
    UTIL_TraceHull(start,start+forward*64,Vector(-22,-22,-22),Vector(22,22,22),
                  MASK_PLAYERSOLID,player,COLLISION_GROUP_NONE,&trace);
    if (trace.startsolid || trace.fraction<1.0f) return;
    CPlayer_Manhack *remote=dynamic_cast<CPlayer_Manhack *>(CBaseEntity::Create(
        "player_manhack",trace.endpos,player->EyeAngles(),player));
    if (!remote || remote->IsMarkedForDeletion()) return;
    if (!remote->StartControl(player,this)) { UTIL_Remove(remote); return; }
    m_hRemote=remote;
}

void CWeapon_Manhack::StopRemote(void)
{
    CPlayer_Manhack *remote=m_hRemote.Get();
    m_hRemote=NULL;
    if (remote) { remote->StopControl(); UTIL_Remove(remote); }
    CBasePlayer *player=ToBasePlayer(GetOwner());
    remote=CPlayer_Manhack::FindForPlayer(player);
    if (remote && remote->GetControlWeapon()==this)
    { remote->StopControl(); UTIL_Remove(remote); }
}

void CWeapon_Manhack::SecondaryAttack(void)
{
    StopRemote();
    m_flNextSecondaryAttack=gpGlobals->curtime+0.5f;
}

void CWeapon_Manhack::ItemPostFrame(void)
{
    if (m_hRemote && !m_hRemote->IsControlledBy(ToBasePlayer(GetOwner()))) m_hRemote=NULL;
    BaseClass::ItemPostFrame();
}

bool CWeapon_Manhack::Holster(CBaseCombatWeapon *switchingTo)
{
    if (!BaseClass::Holster(switchingTo)) return false;
    StopRemote();
    return true;
}

void CWeapon_Manhack::Drop(const Vector &velocity)
{
    StopRemote();
    BaseClass::Drop(velocity);
}

void CWeapon_Manhack::UpdateOnRemove(void)
{
    StopRemote();
    BaseClass::UpdateOnRemove();
}
