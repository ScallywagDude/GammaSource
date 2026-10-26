#ifndef HL2_PLAYER_MANHACK_H
#define HL2_PLAYER_MANHACK_H
#ifdef _WIN32
#pragma once
#endif
#include "basecombatcharacter.h"
class CBasePlayer;
class CBaseCombatWeapon;
class CUserCmd;

// HL2MODERN_REMOTE_MANHACK: original remote blade/camera entity, bound to its
// controlling player. The player's vulnerable body remains at its own origin.
class CPlayer_Manhack : public CBaseCombatCharacter
{
    DECLARE_CLASS(CPlayer_Manhack,CBaseCombatCharacter);
public:
    DECLARE_SERVERCLASS();
    DECLARE_DATADESC();
    CPlayer_Manhack();
    void Precache(void);
    void Spawn(void);
    bool StartControl(CBasePlayer *player,CBaseCombatWeapon *weapon);
    void StopControl(void);
    void PlayerCommand(CUserCmd *command);
    void FlyThink(void);
    void CheckBladeTrace(void);
    void UpdateOnRemove(void);
    void OnRestore(void);
    void Event_Killed(const CTakeDamageInfo &info);
    void InputActivate(inputdata_t &inputdata);
    void InputDeactivate(inputdata_t &inputdata);
    static CPlayer_Manhack *FindForPlayer(CBasePlayer *player);
    bool IsControlledBy(CBasePlayer *player) const;
    CBaseCombatWeapon *GetControlWeapon(void) const;
private:
    CHandle<CBasePlayer> m_hController;
    CHandle<CBaseCombatWeapon> m_hControlWeapon;
    bool m_bActive;
    bool m_bRestoreView;
    bool m_bSavedViewModelVisible;
    int m_iSavedFOV;
    int m_iSavedMoveType;
    int m_iSavedMoveCollide;
    float m_flExpireTime;
    float m_flLastCommand;
    float m_flNextBladeDamage;
    Vector m_vecDesiredVelocity;
    QAngle m_angCommandView;
    QAngle m_angSavedPlayerView;
};

void HL2ModernManhackPlayerCommand(CBasePlayer *player,CUserCmd *command);
#endif
