// GammaSource: explicitly reconstructed Alien Assassin AI.
// Recovered: Assassin.mdl, its knife/stab/smoke activities and events 2/3/5,
// and npc/assassin/{idle2,tattle1}.wav. No matching Alien Assassin AI or knife,
// pest or shock-tripwire projectile art survives in the supplied source/assets.
// This distinct NPC does not replace the recovered Combine/Female Assassin.
#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "activitylist.h"
#include "npcevent.h"
#include "eventlist.h"
#include "smoke_trail.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "tier0/memdbgon.h"

static ConVar sk_alienassassin_health("sk_alienassassin_health", "120", FCVAR_NONE,
    "Reconstructed Alien Assassin health; original AI/balance is unavailable.");
static ConVar sk_alienassassin_claw("sk_alienassassin_claw", "20", FCVAR_NONE,
    "Reconstructed Alien Assassin claw damage.");
static ConVar sk_alienassassin_knife("sk_alienassassin_knife", "15", FCVAR_NONE,
    "Reconstructed Alien Assassin thrown blade damage.");
static ConVar gamma_alienassassin_debug("gamma_alienassassin_debug", "0", FCVAR_CHEAT,
    "Trace reconstructed Alien Assassin attacks and smoke.");
static int ACT_ASSASSIN_SMOKE;
static const char *ALIEN_ASSASSIN_MODEL = "models/Assassin.mdl";

// A traced moving projectile, with an existing beta sprite as its visible trail.
// No unrelated weapon model is presented as authentic Alien Assassin knife art.
class CAlienAssassinKnife : public CBaseEntity
{
public:
    DECLARE_CLASS(CAlienAssassinKnife, CBaseEntity);
    DECLARE_DATADESC();
    void Spawn();
    void TouchKnife(CBaseEntity *other);
    void ExpireKnife() { UTIL_Remove(this); }
    void UpdateOnRemove()
    {
        if (m_hTrail) UTIL_Remove(m_hTrail);
        BaseClass::UpdateOnRemove();
    }
    CHandle<CSpriteTrail> m_hTrail;
};
LINK_ENTITY_TO_CLASS(gamma_alienassassin_knife, CAlienAssassinKnife);
BEGIN_DATADESC(CAlienAssassinKnife)
    DEFINE_FIELD(m_hTrail, FIELD_EHANDLE),
    DEFINE_ENTITYFUNC(TouchKnife),
    DEFINE_THINKFUNC(ExpireKnife),
END_DATADESC()

void CAlienAssassinKnife::Spawn()
{
    PrecacheModel("sprites/bluelaser1.vmt");
    SetMoveType(MOVETYPE_FLYGRAVITY);
    SetGravity(0.1f);
    SetSolid(SOLID_BBOX);
    SetCollisionGroup(COLLISION_GROUP_PROJECTILE);
    AddEffects(EF_NODRAW);
    UTIL_SetSize(this, Vector(-1,-1,-1), Vector(1,1,1));
    SetTouch(&CAlienAssassinKnife::TouchKnife);
    SetThink(&CAlienAssassinKnife::ExpireKnife);
    SetNextThink(gpGlobals->curtime + 5);
    m_hTrail = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetAbsOrigin(), false);
    if (m_hTrail)
    {
        m_hTrail->SetParent(this);
        m_hTrail->SetLocalOrigin(vec3_origin);
        m_hTrail->SetTransparency(kRenderTransAdd, 175, 190, 200, 180, kRenderFxNone);
        m_hTrail->SetStartWidth(1.5f);
        m_hTrail->SetEndWidth(0.1f);
        m_hTrail->SetLifeTime(0.1f);
    }
}

void CAlienAssassinKnife::TouchKnife(CBaseEntity *other)
{
    if (!other || other == GetOwnerEntity() || !other->IsSolid() ||
        other->IsSolidFlagSet(FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS)) return;
    if (other->m_takedamage != DAMAGE_NO)
    {
        Vector direction = GetAbsVelocity();
        VectorNormalize(direction);
        trace_t trace;
        trace = GetTouchTrace();
        CTakeDamageInfo damage(this, GetOwnerEntity() ? GetOwnerEntity() : this,
            MAX(0.0f, sk_alienassassin_knife.GetFloat()), DMG_SLASH);
        CalculateMeleeDamageForce(&damage, direction, trace.endpos);
        ClearMultiDamage();
        other->DispatchTraceAttack(damage, direction, &trace);
        ApplyMultiDamage();
        if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN knife_hit target=%d damage=%.2f\n", other->entindex(), damage.GetDamage());
    }
    SetTouch(NULL);
    SetSolid(SOLID_NONE);
    SetAbsVelocity(vec3_origin);
    SetThink(&CAlienAssassinKnife::ExpireKnife);
    SetNextThink(gpGlobals->curtime + 0.1f);
}

enum { SCHED_ALIENASSASSIN_SMOKE = LAST_SHARED_SCHEDULE };
enum { TASK_ALIENASSASSIN_SMOKE = LAST_SHARED_TASK };

class CNPC_AlienAssassin : public CAI_BaseNPC
{
public:
    DECLARE_CLASS(CNPC_AlienAssassin, CAI_BaseNPC);
    DECLARE_DATADESC();
    DEFINE_CUSTOM_AI;
    CNPC_AlienAssassin();
    void Precache();
    void Spawn();
    Class_T Classify() { return CLASS_COMBINE; }
    float MaxYawSpeed() { return IsMoving() ? 90.0f : 120.0f; }
    int MeleeAttack1Conditions(float dot, float distance);
    int RangeAttack1Conditions(float dot, float distance);
    int SelectSchedule();
    void StartTask(const Task_t *task);
    void RunTask(const Task_t *task);
    void HandleAnimEvent(animevent_t *event);
    void PrescheduleThink();
    void IdleSound() { EmitSound("npc/assassin/idle2.wav"); }
    void AlertSound() { EmitSound("npc/assassin/tattle1.wav"); }
    void UpdateOnRemove();
    void Event_Killed(const CTakeDamageInfo &info);
    void InputDeploySmoke(inputdata_t &data);

private:
    void ThrowKnife();
    void Claw();
    void ReleaseSmoke();
    Vector HandPosition();
    float m_flNextKnife;
    float m_flNextSmoke;
    float m_flSmokeTaskEnd;
    float m_flFootstepCycle;
    int m_nFootstepSequence;
    bool m_bReleasedSmoke;
    CHandle<SmokeTrail> m_hSmoke;
};

LINK_ENTITY_TO_CLASS(npc_alienassassin, CNPC_AlienAssassin);
LINK_ENTITY_TO_CLASS(npc_assassin_alien, CNPC_AlienAssassin);
BEGIN_DATADESC(CNPC_AlienAssassin)
    DEFINE_FIELD(m_flNextKnife, FIELD_TIME),
    DEFINE_FIELD(m_flNextSmoke, FIELD_TIME),
    DEFINE_FIELD(m_flSmokeTaskEnd, FIELD_TIME),
    DEFINE_FIELD(m_flFootstepCycle, FIELD_FLOAT),
    DEFINE_FIELD(m_nFootstepSequence, FIELD_INTEGER),
    DEFINE_FIELD(m_bReleasedSmoke, FIELD_BOOLEAN),
    DEFINE_FIELD(m_hSmoke, FIELD_EHANDLE),
    DEFINE_INPUTFUNC(FIELD_VOID, "DeploySmoke", InputDeploySmoke),
END_DATADESC()

CNPC_AlienAssassin::CNPC_AlienAssassin()
{
    m_flNextKnife = m_flNextSmoke = m_flSmokeTaskEnd = m_flFootstepCycle = 0;
    m_nFootstepSequence = -1;
    m_bReleasedSmoke = false;
}

void CNPC_AlienAssassin::Precache()
{
    // Register before the model's activity names are indexed.
    ACT_ASSASSIN_SMOKE = ActivityList_RegisterPrivateActivity("ACT_ASSASSIN_SMOKE");
    if (!GetModelName()) SetModelName(AllocPooledString(ALIEN_ASSASSIN_MODEL));
    PrecacheModel(STRING(GetModelName()));
    PrecacheModel("sprites/bluelaser1.vmt");
    PrecacheScriptSound("npc/assassin/idle2.wav");
    PrecacheScriptSound("npc/assassin/tattle1.wav");
    PrecacheScriptSound("NPC_Assassin.FootstepLeft");
    PrecacheScriptSound("NPC_Assassin.FootstepRight");
    UTIL_PrecacheOther("gamma_alienassassin_knife");
    BaseClass::Precache();
}

void CNPC_AlienAssassin::Spawn()
{
    Precache();
    SetModel(STRING(GetModelName()));
    SetHullType(HULL_HUMAN);
    SetHullSizeNormal();
    SetSolid(SOLID_BBOX);
    AddSolidFlags(FSOLID_NOT_STANDABLE);
    SetMoveType(MOVETYPE_STEP);
    SetBloodColor(BLOOD_COLOR_RED);
    SetDefaultEyeOffset();
    if (GetHealth() <= 0) SetHealth(MAX(1, sk_alienassassin_health.GetInt()));
    SetMaxHealth(GetHealth());
    m_flFieldOfView = 0.5f;
    m_NPCState = NPC_STATE_NONE;
    CapabilitiesClear();
    CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_SQUAD | bits_CAP_DOORS_GROUP |
        bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1);
    NPCInit();
}

int CNPC_AlienAssassin::MeleeAttack1Conditions(float dot, float distance)
{
    if (distance > 88) return COND_TOO_FAR_TO_ATTACK;
    return dot >= 0.6f ? COND_CAN_MELEE_ATTACK1 : COND_NOT_FACING_ATTACK;
}

int CNPC_AlienAssassin::RangeAttack1Conditions(float dot, float distance)
{
    if (gpGlobals->curtime < m_flNextKnife) return COND_NONE;
    if (distance < 88) return COND_TOO_CLOSE_TO_ATTACK;
    if (distance > 1024) return COND_TOO_FAR_TO_ATTACK;
    return dot >= 0.5f ? COND_CAN_RANGE_ATTACK1 : COND_NOT_FACING_ATTACK;
}

int CNPC_AlienAssassin::SelectSchedule()
{
    if (GetState() == NPC_STATE_COMBAT && GetEnemy())
    {
        if (HasCondition(COND_ENEMY_DEAD)) return BaseClass::SelectSchedule();
        if (HasCondition(COND_HEAVY_DAMAGE) && gpGlobals->curtime >= m_flNextSmoke)
            return SCHED_ALIENASSASSIN_SMOKE;
        if (HasCondition(COND_CAN_MELEE_ATTACK1)) return SCHED_MELEE_ATTACK1;
        if (HasCondition(COND_CAN_RANGE_ATTACK1)) return SCHED_RANGE_ATTACK1;
        return SCHED_CHASE_ENEMY;
    }
    return BaseClass::SelectSchedule();
}

Vector CNPC_AlienAssassin::HandPosition()
{
    const int hand = LookupBone("Bip01 R Hand");
    Vector position = WorldSpaceCenter();
    QAngle angles;
    if (hand >= 0) GetBonePosition(hand, position, angles);
    return position;
}

void CNPC_AlienAssassin::ThrowKnife()
{
    if (!GetEnemy() || gpGlobals->curtime < m_flNextKnife) return;
    const Vector start = HandPosition();
    Vector direction = GetEnemy()->BodyTarget(start) - start;
    if (VectorNormalize(direction) < 1.0f) return;
    CAlienAssassinKnife *knife = dynamic_cast<CAlienAssassinKnife *>(CreateEntityByName("gamma_alienassassin_knife"));
    if (!knife) return;
    knife->SetOwnerEntity(this);
    knife->SetAbsOrigin(start);
    if (DispatchSpawn(knife) < 0) return;
    knife->SetAbsVelocity(direction * 1400.0f);
    m_flNextKnife = gpGlobals->curtime + 1.2f;
    CSoundEnt::InsertSound(SOUND_COMBAT, start, 256, 0.25f, this);
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN knife_throw id=%d projectile=%d\n", entindex(), knife->entindex());
}

void CNPC_AlienAssassin::Claw()
{
    CBaseEntity *victim = CheckTraceHullAttack(96, Vector(-12,-12,-16), Vector(12,12,16),
        MAX(0, sk_alienassassin_claw.GetInt()), DMG_SLASH, 1.0f, true);
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN claw id=%d hit=%d\n", entindex(), victim ? victim->entindex() : -1);
}

void CNPC_AlienAssassin::ReleaseSmoke()
{
    if (m_bReleasedSmoke) return;
    m_bReleasedSmoke = true;
    if (m_hSmoke) UTIL_Remove(m_hSmoke);
    m_hSmoke = SmokeTrail::CreateSmokeTrail();
    if (m_hSmoke)
    {
        m_hSmoke->SetAbsOrigin(GetAbsOrigin() + Vector(0,0,32));
        m_hSmoke->m_StartColor = Vector(0.3f,0.3f,0.3f);
        m_hSmoke->m_EndColor = Vector(0.15f,0.15f,0.15f);
        m_hSmoke->m_Opacity = 0.8f;
        m_hSmoke->m_SpawnRate = 30;
        m_hSmoke->m_ParticleLifetime = 2.5f;
        m_hSmoke->m_StartSize = 24;
        m_hSmoke->m_EndSize = 90;
        m_hSmoke->m_SpawnRadius = 40;
        m_hSmoke->m_MinSpeed = 12;
        m_hSmoke->m_MaxSpeed = 24;
        m_hSmoke->m_StopEmitTime = gpGlobals->curtime + 1.2f;
        m_hSmoke->SetLifetime(4);
    }
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN smoke id=%d\n", entindex());
}

void CNPC_AlienAssassin::StartTask(const Task_t *task)
{
    if (task->iTask == TASK_ALIENASSASSIN_SMOKE)
    {
        m_bReleasedSmoke = false;
        m_flNextSmoke = gpGlobals->curtime + 8;
        m_flSmokeTaskEnd = gpGlobals->curtime + 2;
        if (SelectWeightedSequence((Activity)ACT_ASSASSIN_SMOKE) >= 0) SetActivity((Activity)ACT_ASSASSIN_SMOKE);
        else { ReleaseSmoke(); TaskComplete(); }
        return;
    }
    BaseClass::StartTask(task);
}

void CNPC_AlienAssassin::RunTask(const Task_t *task)
{
    if (task->iTask == TASK_ALIENASSASSIN_SMOKE)
    {
        if (IsSequenceFinished() || gpGlobals->curtime >= m_flSmokeTaskEnd)
        {
            ReleaseSmoke();
            TaskComplete();
        }
        return;
    }
    BaseClass::RunTask(task);
}

void CNPC_AlienAssassin::HandleAnimEvent(animevent_t *event)
{
    // Numeric event meanings are recovered from this model, not the Female Assassin.
    if (event->event == 2 && GetActivity() == ACT_RANGE_ATTACK1) { ThrowKnife(); return; }
    if (event->event == 3 && GetActivity() == ACT_MELEE_ATTACK1) { Claw(); return; }
    if (event->event == 5 && GetActivity() == ACT_ASSASSIN_SMOKE) { ReleaseSmoke(); return; }
    if (!(event->type & AE_TYPE_NEWEVENTSYSTEM) && event->event >= 1 && event->event <= 7)
        return; // Unreconstructed pest/tripwire events are not misread as shared events.
    BaseClass::HandleAnimEvent(event);
}

void CNPC_AlienAssassin::PrescheduleThink()
{
    BaseClass::PrescheduleThink();
    if (IsMoving() && (GetActivity() == ACT_RUN || GetActivity() == ACT_WALK))
    {
        const float cycle = GetCycle();
        if (m_nFootstepSequence == GetSequence())
        {
            if (cycle < m_flFootstepCycle) EmitSound("NPC_Assassin.FootstepLeft");
            if (m_flFootstepCycle < 0.5f && cycle >= 0.5f) EmitSound("NPC_Assassin.FootstepRight");
        }
        m_flFootstepCycle = cycle;
        m_nFootstepSequence = GetSequence();
    }
    else m_nFootstepSequence = -1;
}

void CNPC_AlienAssassin::InputDeploySmoke(inputdata_t &data)
{
    if (IsAlive() && GetEnemy() && gpGlobals->curtime >= m_flNextSmoke)
        SetSchedule(SCHED_ALIENASSASSIN_SMOKE);
}

void CNPC_AlienAssassin::Event_Killed(const CTakeDamageInfo &info)
{
    if (m_hSmoke) UTIL_Remove(m_hSmoke);
    m_hSmoke = NULL;
    BaseClass::Event_Killed(info);
}

void CNPC_AlienAssassin::UpdateOnRemove()
{
    if (m_hSmoke) UTIL_Remove(m_hSmoke);
    m_hSmoke = NULL;
    BaseClass::UpdateOnRemove();
}

AI_BEGIN_CUSTOM_NPC(npc_alienassassin, CNPC_AlienAssassin)
    DECLARE_TASK(TASK_ALIENASSASSIN_SMOKE)
    DEFINE_SCHEDULE(SCHED_ALIENASSASSIN_SMOKE,
        " Tasks"
        "  TASK_STOP_MOVING 0"
        "  TASK_FACE_ENEMY 0"
        "  TASK_ALIENASSASSIN_SMOKE 0"
        "  TASK_FIND_COVER_FROM_ENEMY 0"
        "  TASK_RUN_PATH 0"
        "  TASK_WAIT_FOR_MOVEMENT 0"
        " Interrupts"
        "  COND_NEW_ENEMY"
        "  COND_ENEMY_DEAD"
    )
AI_END_CUSTOM_NPC()
