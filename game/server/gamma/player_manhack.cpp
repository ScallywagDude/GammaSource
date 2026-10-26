//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/player_manhack.cpp) ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "player_manhack.h"
#include "player.h"
#include "basecombatweapon.h"
#include "baseviewmodel.h"
#include "entitylist.h"
#include "physics.h"
#include "usercmd.h"
#include "in_buttons.h"
#include "soundent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// HL2MODERN_REMOTE_MANHACK: reactivate the original player_manhack concept
// using its own body/camera, not the NPC implementation or player teleportation.
IMPLEMENT_SERVERCLASS_ST(CPlayer_Manhack,DT_Player_Manhack)
END_SEND_TABLE()
LINK_ENTITY_TO_CLASS(player_manhack,CPlayer_Manhack);

BEGIN_DATADESC(CPlayer_Manhack)
    DEFINE_FIELD( m_hController,FIELD_EHANDLE),
    DEFINE_FIELD( m_hControlWeapon,FIELD_EHANDLE),
    DEFINE_FIELD( m_bActive,FIELD_BOOLEAN),
    DEFINE_FIELD( m_bSavedViewModelVisible,FIELD_BOOLEAN),
    DEFINE_FIELD( m_iSavedFOV,FIELD_INTEGER),
    DEFINE_FIELD( m_iSavedMoveType,FIELD_INTEGER),
    DEFINE_FIELD( m_iSavedMoveCollide,FIELD_INTEGER),
    DEFINE_FIELD( m_flExpireTime,FIELD_TIME),
    DEFINE_FIELD( m_flLastCommand,FIELD_TIME),
    DEFINE_FIELD( m_flNextBladeDamage,FIELD_TIME),
    DEFINE_FIELD( m_vecDesiredVelocity,FIELD_VECTOR),
    DEFINE_FIELD( m_angCommandView,FIELD_VECTOR),
    DEFINE_FIELD( m_angSavedPlayerView,FIELD_VECTOR),
    DEFINE_THINKFUNC( FlyThink ),
    DEFINE_INPUTFUNC( FIELD_VOID,"Activate",InputActivate),
    DEFINE_INPUTFUNC( FIELD_VOID,"Deactivate",InputDeactivate),
END_DATADESC()

CPlayer_Manhack::CPlayer_Manhack()
{
    m_bActive=false;
    m_bRestoreView=false;
    m_bSavedViewModelVisible=false;
    m_iSavedFOV=0;
    m_iSavedMoveType=MOVETYPE_WALK;
    m_iSavedMoveCollide=MOVECOLLIDE_DEFAULT;
    m_flExpireTime=0;
    m_flLastCommand=0;
    m_flNextBladeDamage=0;
    m_vecDesiredVelocity=vec3_origin;
    m_angCommandView=vec3_angle;
    m_angSavedPlayerView=vec3_angle;
}

void CPlayer_Manhack::Precache(void)
{
    engine->PrecacheModel("models/manhack.mdl");
    BaseClass::Precache();
}

void CPlayer_Manhack::Spawn(void)
{
    Precache();
    SetModel("models/manhack.mdl");
    SetMoveType(MOVETYPE_VPHYSICS);
    SetSolid(SOLID_BBOX);
    UTIL_SetSize(this,Vector(-22,-22,-22),Vector(22,22,22));
    m_iHealth=25;
    m_iMaxHealth=25;
    m_lifeState=LIFE_ALIVE;
    m_takedamage=DAMAGE_YES;
    IPhysicsObject *physics=VPhysicsInitNormal(SOLID_BBOX,0,false);
    if (!physics) { UTIL_Remove(this); return; }
    physics->EnableGravity(false);
}

CPlayer_Manhack *CPlayer_Manhack::FindForPlayer(CBasePlayer *player)
{
    if (!player) return NULL;
    CBaseEntity *entity=NULL;
    while ((entity=gEntList.FindEntityByClassname(entity,"player_manhack"))!=NULL)
    {
        CPlayer_Manhack *control=dynamic_cast<CPlayer_Manhack *>(entity);
        if (control && !control->IsMarkedForDeletion() && control->IsControlledBy(player)) return control;
    }
    return NULL;
}

bool CPlayer_Manhack::IsControlledBy(CBasePlayer *player) const
{
    return m_bActive && m_hController.Get()==player;
}

CBaseCombatWeapon *CPlayer_Manhack::GetControlWeapon(void) const
{
    return m_hControlWeapon.Get();
}

bool CPlayer_Manhack::StartControl(CBasePlayer *player,CBaseCombatWeapon *weapon)
{
    if (m_bActive || !player || !player->IsAlive() || !weapon ||
        weapon->GetOwner()!=player || player->GetActiveWeapon()!=weapon ||
        !VPhysicsGetObject() || FindForPlayer(player)) return false;
    m_hController=player;
    m_hControlWeapon=weapon;
    m_iSavedFOV=player->GetFOV();
    m_iSavedMoveType=player->GetMoveType();
    m_iSavedMoveCollide=player->GetMoveCollide();
    CBaseViewModel *viewmodel=player->GetViewModel();
    m_bSavedViewModelVisible=viewmodel && !(viewmodel->IsEffectActive( EF_NODRAW ));
    m_bActive=true;
    m_flExpireTime=gpGlobals->curtime+60.0f;
    m_flLastCommand=gpGlobals->curtime;
    m_flNextBladeDamage=gpGlobals->curtime;
    m_angCommandView=player->EyeAngles();
    m_angSavedPlayerView=m_angCommandView;
    player->SetAbsVelocity(vec3_origin);
    player->SetMoveType(MOVETYPE_NONE);
    player->SetFOV( this, 132 );
    player->ShowViewModel(true);
    engine->SetView(player->edict(),edict());
    EmitSound("Player_Manhack.ThrustLow");
    SetThink(&CPlayer_Manhack::FlyThink);
    SetNextThink(gpGlobals->curtime+0.02f);
    return true;
}

void CPlayer_Manhack::StopControl(void)
{
    if (!m_bActive) return;
    m_bActive=false;
    CBasePlayer *player=m_hController.Get();
    m_hController=NULL;
    m_hControlWeapon=NULL;
    m_vecDesiredVelocity=vec3_origin;
    StopSound("Player_Manhack.ThrustLow");
    StopSound("Player_Manhack.ThrustHigh");
    StopSound("Player_Manhack.Grind");
    StopSound("Player_Manhack.GrindFlesh");
    if (player)
    {
        engine->SetView(player->edict(),player->edict());
        if (player->IsAlive())
        {
            player->SetMoveType((MoveType_t)m_iSavedMoveType,(MoveCollide_t)m_iSavedMoveCollide);
            player->SetAbsVelocity(vec3_origin);
            player->SetFOV( this, m_iSavedFOV );
            player->SnapEyeAngles(m_angSavedPlayerView);
            player->ShowViewModel(m_bSavedViewModelVisible);
        }
        else player->SetFOV( this, 0 );
    }
}

void CPlayer_Manhack::PlayerCommand(CUserCmd *command)
{
    if (!command || !m_bActive) return;
    // The 2018 input defaults to 450, while the remote flies at most 400.
    // Clamp ordinary player input instead of rejecting every full WASD press.
    float forward=command->forwardmove;
    float side=command->sidemove;
    float up=command->upmove;
    forward=IsFinite(forward)?clamp(forward,-400.0f,400.0f):0.0f;
    side=IsFinite(side)?clamp(side,-400.0f,400.0f):0.0f;
    up=IsFinite(up)?clamp(up,-400.0f,400.0f):0.0f;
    if (command->buttons & IN_JUMP) up=400;
    if (command->buttons & IN_DUCK) up=-400;
    command->forwardmove=command->sidemove=command->upmove=0;
    command->buttons &= ~(IN_JUMP|IN_DUCK);
    for (int i=0;i<3;++i)
        if (!(command->viewangles[i]>=-36000.0f && command->viewangles[i]<=36000.0f)) return;
    m_angCommandView=command->viewangles;
    m_angCommandView.x=MAX(-89.0f,MIN(89.0f,m_angCommandView.x));
    m_angCommandView.z=0;
    Vector direction,right;
    AngleVectors(m_angCommandView,&direction,&right,NULL);
    m_vecDesiredVelocity=direction*forward+right*side+Vector(0,0,up);
    if (m_vecDesiredVelocity.Length()>400.0f)
    { VectorNormalize(m_vecDesiredVelocity); m_vecDesiredVelocity*=400.0f; }
    m_flLastCommand=gpGlobals->curtime;
}

void HL2ModernManhackPlayerCommand(CBasePlayer *player,CUserCmd *command)
{
    CPlayer_Manhack *control=CPlayer_Manhack::FindForPlayer(player);
    if (control) control->PlayerCommand(command);
}

CON_COMMAND_F(gamma_manhackinfo,"Print remote manhack and stationary player positions",FCVAR_CHEAT)
{
    CBasePlayer *player=UTIL_GetLocalPlayer();
    if (!player) return;
    CPlayer_Manhack *remote=CPlayer_Manhack::FindForPlayer(player);
    const Vector body=player->GetAbsOrigin();
    const Vector position=remote?remote->GetAbsOrigin():vec3_origin;
    Vector velocity=vec3_origin;
    if (remote && remote->VPhysicsGetObject()) remote->VPhysicsGetObject()->GetVelocity(&velocity,NULL);
    Msg("GAMMA_MANHACK active=%d body=%.2f %.2f %.2f movetype=%d fov=%d remote=%.2f %.2f %.2f velocity=%.2f %.2f %.2f\n",
        remote?1:0,body.x,body.y,body.z,player->GetMoveType(),player->GetFOV(),
        position.x,position.y,position.z,velocity.x,velocity.y,velocity.z);
    if (!remote)
    {
        Vector forward;
        player->EyeVectors(&forward);
        const Vector start=player->EyePosition();
        trace_t trace;
        UTIL_TraceHull(start,start+forward*64,Vector(-22,-22,-22),Vector(22,22,22),
            MASK_PLAYERSOLID,player,COLLISION_GROUP_NONE,&trace);
        Msg("GAMMA_MANHACK_SPAWN fraction=%.3f startsolid=%d entity=%s surface=%s\n",
            trace.fraction,trace.startsolid?1:0,trace.m_pEnt?trace.m_pEnt->GetClassname():"none",
            trace.surface.name?trace.surface.name:"none");
    }
}

void CPlayer_Manhack::FlyThink(void)
{
    CBasePlayer *player=m_hController.Get();
    CBaseCombatWeapon *weapon=m_hControlWeapon.Get();
    if (!m_bActive || !player || !player->IsAlive() || !weapon ||
        weapon->GetOwner()!=player || player->GetActiveWeapon()!=weapon ||
        gpGlobals->curtime>=m_flExpireTime ||
        (GetAbsOrigin()-player->GetAbsOrigin()).Length()>1600.0f)
    { StopControl(); UTIL_Remove(this); return; }
    IPhysicsObject *physics=VPhysicsGetObject();
    if (!physics) { StopControl(); UTIL_Remove(this); return; }
    if (m_bRestoreView)
    {
        engine->SetView(player->edict(),edict());
        player->SetMoveType(MOVETYPE_NONE);
        player->SetFOV( this, 132 );
        m_bRestoreView=false;
    }
    Vector velocity;
    physics->GetVelocity(&velocity,NULL);
    if (gpGlobals->curtime-m_flLastCommand>0.25f) m_vecDesiredVelocity=vec3_origin;
    Vector nextVelocity=velocity+(m_vecDesiredVelocity-velocity)*0.12f;
    if (nextVelocity.Length()>400.0f)
    { VectorNormalize(nextVelocity); nextVelocity*=400.0f; }
    Vector correction=nextVelocity-velocity;
    physics->AddVelocity(&correction,NULL);
    Vector physicsPosition;
    physics->GetPosition(&physicsPosition,NULL);
    physics->SetPosition(physicsPosition,m_angCommandView,true);
    if (gpGlobals->curtime>=m_flNextBladeDamage)
    {
        CheckBladeTrace();
        m_flNextBladeDamage=gpGlobals->curtime+0.1f;
    }
    SetNextThink(gpGlobals->curtime+0.02f);
}

void CPlayer_Manhack::CheckBladeTrace(void)
{
    CBasePlayer *player=m_hController.Get();
    if (!player) return;
    Vector facing;
    AngleVectors(m_angCommandView,&facing);
    const Vector start=GetAbsOrigin()+Vector(0,0,-18);
    trace_t trace;
    UTIL_TraceHull(start,start+facing*14,Vector(-22,-22,-1),Vector(22,22,1),
                   MASK_SHOT,this,COLLISION_GROUP_NONE,&trace);
    if (trace.fraction==1.0f && !trace.startsolid) return;
    CBaseEntity *hit=trace.m_pEnt;
    if (hit && hit!=player && hit->m_takedamage!=DAMAGE_NO)
    {
        CTakeDamageInfo info(this,player,1.0f,DMG_SLASH);
        CalculateMeleeDamageForce(&info,facing,trace.endpos);
        hit->TakeDamage(info);
        EmitSound(ToBaseCombatCharacter(hit)?"Player_Manhack.GrindFlesh":"Player_Manhack.Grind");
    }
    else if (trace.DidHitWorld())
    {
        EmitSound("Player_Manhack.Grind");
        UTIL_DecalTrace(&trace,"ManhackCut");
    }
}

void CPlayer_Manhack::OnRestore(void)
{
    BaseClass::OnRestore();
    m_bRestoreView=m_bActive;
    if (m_bActive)
    {
        SetThink(&CPlayer_Manhack::FlyThink);
        SetNextThink(gpGlobals->curtime+0.1f);
    }
}

void CPlayer_Manhack::UpdateOnRemove(void)
{
    StopControl();
    SetThink(NULL);
    BaseClass::UpdateOnRemove();
}

void CPlayer_Manhack::Event_Killed(const CTakeDamageInfo &info)
{
    (void)info;
    m_lifeState=LIFE_DEAD;
    m_takedamage=DAMAGE_NO;
    EmitSound("NPC_Manhack.Die");
    UTIL_Remove(this);
}

void CPlayer_Manhack::InputActivate(inputdata_t &inputdata)
{
    CBasePlayer *player=ToBasePlayer(inputdata.pActivator);
    if (!player) return;
    CBaseCombatWeapon *weapon=player->Weapon_OwnsThisType("weapon_manhack");
    if (!weapon)
    {
        player->GiveNamedItem("weapon_manhack");
        weapon=player->Weapon_OwnsThisType("weapon_manhack");
    }
    if (weapon && player->Weapon_Switch(weapon)) StartControl(player,weapon);
}

void CPlayer_Manhack::InputDeactivate(inputdata_t &inputdata)
{
    (void)inputdata;
    StopControl();
    UTIL_Remove(this);
}
