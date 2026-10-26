// GammaSource: reconstructed AI for the supplied 2003 Cremator assets.
// No Cremator AI implementation survives in the provided 2003 source. Model,
// activities, muzzle/head attachments, sounds, hull and CLASS_CREMATOR relations
// are recovered data; attack timing, damage, patrol and cleanup policy below are
// explicitly new, independently tunable reconstruction rather than recovered AI.
#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "beam_shared.h"
#include "entitylist.h"
#include "npcevent.h"
#include "eventlist.h"
#include "soundent.h"
#include "vphysics_interface.h"
#include "basecombatweapon.h"
#include "tier0/memdbgon.h"

static const char *CREMATOR_MODEL = "models/cremator.mdl";
static const char *CREMATOR_BODY = "models/cremator_body.mdl";
static const char *CREMATOR_HEAD = "models/cremator_head.mdl";
static const char *CREMATOR_BEAM = "sprites/bluelaser1.vmt";

static ConVar sk_cremator_health("sk_cremator_health", "300", FCVAR_NONE,
    "Reconstructed Cremator health; no original AI balance is available.");
static ConVar sk_cremator_dps("sk_cremator_dps", "40", FCVAR_NONE,
    "Reconstructed Cremator immolator damage per second.");
static ConVar sk_cremator_range("sk_cremator_range", "600", FCVAR_NONE,
    "Reconstructed Cremator immolator range in world units.");
static ConVar sk_cremator_cleanup_time("sk_cremator_cleanup_time", "2", FCVAR_NONE,
    "Continuous clear beam contact needed to dissolve a corpse.");
static ConVar gamma_cremator_debug("gamma_cremator_debug", "0", FCVAR_CHEAT,
    "Trace reconstructed Cremator fire and corpse cleanup.");

enum
{
    SCHED_CREMATOR_ATTACK = LAST_SHARED_SCHEDULE,
    SCHED_CREMATOR_CLEANUP,
};
enum
{
    TASK_CREMATOR_GET_CLEANUP_PATH = LAST_SHARED_TASK,
    TASK_CREMATOR_FACE_CLEANUP,
    TASK_CREMATOR_IMMOLATE,
};

class CNPC_Cremator : public CAI_BaseNPC
{
public:
    DECLARE_CLASS(CNPC_Cremator, CAI_BaseNPC);
    DECLARE_DATADESC();
    DEFINE_CUSTOM_AI;
    CNPC_Cremator();
    void Precache();
    void Spawn();
    void Activate();
    void UpdateOnRemove();
    void PrescheduleThink();
    void OnScheduleChange();
    void StartTask(const Task_t *task);
    void RunTask(const Task_t *task);
    int SelectSchedule();
    int SelectFailSchedule(int schedule, int task, AI_TaskFailureCode_t failure);
    int RangeAttack1Conditions(float dot, float distance);
    Activity NPC_TranslateActivity(Activity activity);
    float MaxYawSpeed() { return IsMoving() ? 35.0f : 50.0f; }
    Class_T Classify() { return CLASS_CREMATOR; }
    void HandleAnimEvent(animevent_t *event);
    void AlertSound() { EmitSound("NPC_Cremator.SeeEnemy"); }
    void DeathSound(const CTakeDamageInfo &info) { EmitSound("NPC_Cremator.Die"); }
    void Event_Killed(const CTakeDamageInfo &info);
    bool BecomeRagdoll(const CTakeDamageInfo &info, const Vector &force);
    void StopLoopingSounds();
    void InputEnable(inputdata_t &data);
    void InputDisable(inputdata_t &data);
    void InputSetCleanupTarget(inputdata_t &data);

private:
    bool IsCorpse(CBaseEntity *entity) const;
    void FindCleanupTarget();
    void BeginImmolating(bool cleanup);
    void StopImmolating();
    bool UpdateImmolator();
    void StartBreathing();
    Vector MuzzlePosition();
    CBaseEntity *CreateDeathPart(const char *classname, const char *model, const Vector &origin,
        const Vector &force);

    CHandle<CBeam> m_hBeam;
    EHANDLE m_hCleanupTarget;
    bool m_bEnabled;
    bool m_bPatrol;
    bool m_bCleanupCorpses;
    bool m_bDropImmolator;
    bool m_bImmolating;
    bool m_bBurningCorpse;
    bool m_bBreathing;
    float m_flNextAttack;
    float m_flAttackEnd;
    float m_flNextPulse;
    float m_flLastPulse;
    float m_flNextCorpseSearch;
    float m_flCleanupProgress;
    float m_flLastFootstepCycle;
    int m_nLastFootstepSequence;
    int m_nMuzzleAttachment;
    int m_nHeadAttachment;
    COutputEvent m_OnImmolatorStarted;
    COutputEvent m_OnImmolatorStopped;
    COutputEvent m_OnCorpseCleaned;
};

LINK_ENTITY_TO_CLASS(npc_cremator, CNPC_Cremator);

BEGIN_DATADESC(CNPC_Cremator)
    DEFINE_FIELD(m_hBeam, FIELD_EHANDLE),
    DEFINE_FIELD(m_hCleanupTarget, FIELD_EHANDLE),
    DEFINE_KEYFIELD(m_bEnabled, FIELD_BOOLEAN, "StartEnabled"),
    DEFINE_KEYFIELD(m_bPatrol, FIELD_BOOLEAN, "patrol"),
    DEFINE_KEYFIELD(m_bCleanupCorpses, FIELD_BOOLEAN, "cleanupcorpses"),
    DEFINE_KEYFIELD(m_bDropImmolator, FIELD_BOOLEAN, "dropimmolator"),
    DEFINE_FIELD(m_bImmolating, FIELD_BOOLEAN),
    DEFINE_FIELD(m_bBurningCorpse, FIELD_BOOLEAN),
    DEFINE_FIELD(m_bBreathing, FIELD_BOOLEAN),
    DEFINE_FIELD(m_flNextAttack, FIELD_TIME),
    DEFINE_FIELD(m_flAttackEnd, FIELD_TIME),
    DEFINE_FIELD(m_flNextPulse, FIELD_TIME),
    DEFINE_FIELD(m_flLastPulse, FIELD_TIME),
    DEFINE_FIELD(m_flNextCorpseSearch, FIELD_TIME),
    DEFINE_FIELD(m_flCleanupProgress, FIELD_FLOAT),
    DEFINE_FIELD(m_flLastFootstepCycle, FIELD_FLOAT),
    DEFINE_FIELD(m_nLastFootstepSequence, FIELD_INTEGER),
    DEFINE_FIELD(m_nMuzzleAttachment, FIELD_INTEGER),
    DEFINE_FIELD(m_nHeadAttachment, FIELD_INTEGER),
    DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
    DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
    DEFINE_INPUTFUNC(FIELD_STRING, "SetCleanupTarget", InputSetCleanupTarget),
    DEFINE_OUTPUT(m_OnImmolatorStarted, "OnImmolatorStarted"),
    DEFINE_OUTPUT(m_OnImmolatorStopped, "OnImmolatorStopped"),
    DEFINE_OUTPUT(m_OnCorpseCleaned, "OnCorpseCleaned"),
END_DATADESC()

CNPC_Cremator::CNPC_Cremator()
{
    m_bEnabled = m_bPatrol = m_bCleanupCorpses = true;
    m_bDropImmolator = true;
    m_bImmolating = m_bBurningCorpse = m_bBreathing = false;
    m_flNextAttack = m_flAttackEnd = m_flNextPulse = m_flLastPulse = 0;
    m_flNextCorpseSearch = m_flCleanupProgress = m_flLastFootstepCycle = 0;
    m_nLastFootstepSequence = -1;
    m_nMuzzleAttachment = m_nHeadAttachment = 0;
}

void CNPC_Cremator::Precache()
{
    if (!GetModelName()) SetModelName(AllocPooledString(CREMATOR_MODEL));
    PrecacheModel(STRING(GetModelName()));
    PrecacheModel(CREMATOR_BODY);
    PrecacheModel(CREMATOR_HEAD);
    PrecacheModel(CREMATOR_BEAM);
    if (m_bDropImmolator) UTIL_PrecacheOther("weapon_immolator");
    PrecacheScriptSound("NPC_Cremator.Die");
    PrecacheScriptSound("NPC_Cremator.SeeEnemy");
    PrecacheScriptSound("NPC_Cremator.LeftFootstep");
    PrecacheScriptSound("NPC_Cremator.RightFootstep");
    PrecacheScriptSound("NPC_Cremator.Breathe");
    PrecacheScriptSound("NPC_Cremator.CloakSwish");
    BaseClass::Precache();
}

void CNPC_Cremator::Spawn()
{
    Precache();
    SetModel(STRING(GetModelName()));
    SetHullType(HULL_MEDIUM); // Explicitly identified as the Cremator hull in ai_hull.h.
    SetHullSizeNormal();
    SetSolid(SOLID_BBOX);
    AddSolidFlags(FSOLID_NOT_STANDABLE);
    SetMoveType(MOVETYPE_STEP);
    SetBloodColor(BLOOD_COLOR_RED);
    SetDefaultEyeOffset();
    if (GetHealth() <= 0) SetHealth(MAX(1, sk_cremator_health.GetInt()));
    SetMaxHealth(GetHealth());
    m_flFieldOfView = 0.5f;
    m_NPCState = NPC_STATE_NONE;
    CapabilitiesClear();
    CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_SQUAD | bits_CAP_DOORS_GROUP | bits_CAP_INNATE_RANGE_ATTACK1);
    m_nMuzzleAttachment = LookupAttachment("0"); // Original attachment 0 is bound to bone "muzzle".
    m_nHeadAttachment = LookupAttachment("1"); // Original attachment 1 is bound to Bip01 Head.
    NPCInit();
}

void CNPC_Cremator::StartBreathing()
{
    if (IsAlive() && m_bEnabled && !m_bBreathing)
    {
        EmitSound("NPC_Cremator.Breathe");
        m_bBreathing = true;
    }
}

void CNPC_Cremator::Activate()
{
    BaseClass::Activate();
    // Sound channels are not entity save fields. Restart this one loop after load.
    StopSound("NPC_Cremator.Breathe");
    m_bBreathing = false;
    StartBreathing();
}

void CNPC_Cremator::StopLoopingSounds()
{
    StopSound("NPC_Cremator.Breathe");
    m_bBreathing = false;
    BaseClass::StopLoopingSounds();
}

void CNPC_Cremator::UpdateOnRemove()
{
    StopImmolating();
    StopLoopingSounds();
    BaseClass::UpdateOnRemove();
}

bool CNPC_Cremator::IsCorpse(CBaseEntity *entity) const
{
    if (!entity || entity == this || entity->IsMarkedForDeletion() || entity->IsPlayer()) return false;
    CBaseAnimating *anim = entity->GetBaseAnimating();
    if (!anim || anim->IsDissolving() || entity->GetMoveParent()) return false;
    return (entity->IsNPC() && !entity->IsAlive()) || FClassnameIs(entity, "prop_ragdoll");
}

void CNPC_Cremator::FindCleanupTarget()
{
    m_flNextCorpseSearch = gpGlobals->curtime + 2.0f;
    if (IsCorpse(m_hCleanupTarget)) return;
    m_hCleanupTarget = NULL;
    float nearest = 1024.0f * 1024.0f;
    for (CBaseEntity *entity = gEntList.FindEntityInSphere(NULL, GetAbsOrigin(), 1024);
        entity; entity = gEntList.FindEntityInSphere(entity, GetAbsOrigin(), 1024))
    {
        if (!IsCorpse(entity)) continue;
        const float distance = (entity->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
        if (distance >= nearest) continue;
        trace_t trace;
        UTIL_TraceLine(EyePosition(), entity->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, this,
            COLLISION_GROUP_NONE, &trace);
        if (trace.fraction < 1.0f) continue;
        nearest = distance;
        m_hCleanupTarget = entity;
    }
}

int CNPC_Cremator::RangeAttack1Conditions(float dot, float distance)
{
    if (!m_bEnabled || gpGlobals->curtime < m_flNextAttack) return COND_NONE;
    if (distance > MAX(64.0f, sk_cremator_range.GetFloat())) return COND_TOO_FAR_TO_ATTACK;
    if (dot < 0.5f) return COND_NOT_FACING_ATTACK;
    return COND_CAN_RANGE_ATTACK1;
}

Activity CNPC_Cremator::NPC_TranslateActivity(Activity activity)
{
    activity = BaseClass::NPC_TranslateActivity(activity);
    if (activity == ACT_RUN && GetHealth() < GetMaxHealth() / 3 &&
        SelectWeightedSequence(ACT_RUN_HURT) >= 0) return ACT_RUN_HURT;
    if ((activity == ACT_TURN_LEFT || activity == ACT_TURN_RIGHT) &&
        SelectWeightedSequence(activity) < 0) return ACT_IDLE;
    return activity;
}

int CNPC_Cremator::SelectSchedule()
{
    if (!m_bEnabled) return SCHED_IDLE_STAND;
    if (GetState() == NPC_STATE_COMBAT && GetEnemy())
    {
        m_hCleanupTarget = NULL;
        if (HasCondition(COND_ENEMY_DEAD)) return BaseClass::SelectSchedule();
        if (HasCondition(COND_CAN_RANGE_ATTACK1)) return SCHED_CREMATOR_ATTACK;
        return SCHED_CHASE_ENEMY;
    }
    if (GetState() == NPC_STATE_IDLE || GetState() == NPC_STATE_ALERT)
    {
        if (m_bCleanupCorpses && gpGlobals->curtime >= m_flNextCorpseSearch) FindCleanupTarget();
        if (IsCorpse(m_hCleanupTarget)) return SCHED_CREMATOR_CLEANUP;
        if (m_bPatrol && HasCondition(COND_IN_PVS)) return SCHED_PATROL_WALK;
    }
    return BaseClass::SelectSchedule();
}

int CNPC_Cremator::SelectFailSchedule(int schedule, int task, AI_TaskFailureCode_t failure)
{
    if (schedule == SCHED_CREMATOR_CLEANUP)
    {
        m_hCleanupTarget = NULL;
        m_flNextCorpseSearch = gpGlobals->curtime + 3;
        StopImmolating();
        return SCHED_IDLE_STAND;
    }
    return BaseClass::SelectFailSchedule(schedule, task, failure);
}

void CNPC_Cremator::OnScheduleChange()
{
    StopImmolating();
    BaseClass::OnScheduleChange();
}

Vector CNPC_Cremator::MuzzlePosition()
{
    Vector position;
    QAngle angles;
    if (m_nMuzzleAttachment > 0 && GetAttachment(m_nMuzzleAttachment, position, angles)) return position;
    return WorldSpaceCenter();
}

void CNPC_Cremator::BeginImmolating(bool cleanup)
{
    StopImmolating();
    m_bImmolating = true;
    m_bBurningCorpse = cleanup;
    m_flCleanupProgress = 0;
    m_flLastPulse = gpGlobals->curtime;
    m_flNextPulse = gpGlobals->curtime + 0.1f;
    m_flAttackEnd = gpGlobals->curtime + (cleanup ? MAX(0.1f, sk_cremator_cleanup_time.GetFloat()) + 1 : 2.0f);
    m_hBeam = CBeam::BeamCreate(CREMATOR_BEAM, 8.0f);
    if (m_hBeam)
    {
        m_hBeam->PointsInit(MuzzlePosition(), MuzzlePosition());
        m_hBeam->SetColor(75, 255, 95);
        m_hBeam->SetBrightness(220);
        m_hBeam->SetNoise(3.0f);
        m_hBeam->SetOwnerEntity(this);
    }
    EmitSound("NPC_Cremator.CloakSwish");
    m_OnImmolatorStarted.FireOutput(this, this);
    if (gamma_cremator_debug.GetBool()) Msg("GAMMA_CREMATOR fire_start id=%d cleanup=%d\n", entindex(), cleanup);
}

void CNPC_Cremator::StopImmolating()
{
    if (m_hBeam) UTIL_Remove(m_hBeam);
    m_hBeam = NULL;
    if (m_bImmolating)
    {
        m_bImmolating = false;
        m_flNextAttack = gpGlobals->curtime + 1;
        m_OnImmolatorStopped.FireOutput(this, this);
    }
    m_bBurningCorpse = false;
}

bool CNPC_Cremator::UpdateImmolator()
{
    CBaseEntity *target = m_bBurningCorpse ? m_hCleanupTarget.Get() : GetEnemy();
    if (!m_bImmolating || !target || (!m_bBurningCorpse && !target->IsAlive()) ||
        (m_bBurningCorpse && !IsCorpse(target))) return false;
    const Vector start = MuzzlePosition();
    Vector destination = target->BodyTarget(start);
    GetMotor()->SetIdealYawToTargetAndUpdate(destination);
    Vector direction = destination - start;
    const float distance = VectorNormalize(direction);
    const float range = MAX(64.0f, sk_cremator_range.GetFloat());
    if (distance > range + 32) return false;
    trace_t trace;
    UTIL_TraceLine(start, start + direction * MIN(range, distance + 16), MASK_SHOT, this,
        COLLISION_GROUP_NONE, &trace);
    if (m_hBeam)
    {
        m_hBeam->SetStartPos(start);
        m_hBeam->SetEndPos(trace.endpos);
        m_hBeam->RelinkBeam();
    }
    if (gpGlobals->curtime < m_flNextPulse) return true;
    const float elapsed = clamp(gpGlobals->curtime - m_flLastPulse, 0.0f, 0.25f);
    m_flLastPulse = gpGlobals->curtime;
    m_flNextPulse = gpGlobals->curtime + 0.1f;
    // Damage and cleanup require actual clear beam contact, never distance alone.
    if (trace.m_pEnt != target) return true;
    if (m_bBurningCorpse)
    {
        m_flCleanupProgress += elapsed;
        if (m_flCleanupProgress >= MAX(0.1f, sk_cremator_cleanup_time.GetFloat()))
        {
            m_OnCorpseCleaned.FireOutput(target, this);
            CBaseAnimating *corpse = target->GetBaseAnimating();
            if (corpse && !corpse->Dissolve(NULL, gpGlobals->curtime, false)) UTIL_Remove(target);
            if (gamma_cremator_debug.GetBool()) Msg("GAMMA_CREMATOR corpse_cleaned id=%d target=%d\n", entindex(), target->entindex());
            m_hCleanupTarget = NULL;
            return false;
        }
    }
    else if (target->m_takedamage != DAMAGE_NO && IRelationType(target) == D_HT)
    {
        CTakeDamageInfo damage(this, this, MAX(0.0f, sk_cremator_dps.GetFloat()) * elapsed, DMG_BURN | DMG_DISSOLVE);
        damage.SetDamagePosition(trace.endpos);
        damage.SetDamageForce(direction * damage.GetDamage() * 5.0f);
        target->TakeDamage(damage);
        if (gamma_cremator_debug.GetBool()) Msg("GAMMA_CREMATOR burn id=%d target=%d damage=%.3f health=%d\n", entindex(), target->entindex(), damage.GetDamage(), target->GetHealth());
    }
    return true;
}

void CNPC_Cremator::StartTask(const Task_t *task)
{
    switch (task->iTask)
    {
    case TASK_CREMATOR_GET_CLEANUP_PATH:
        if (!IsCorpse(m_hCleanupTarget)) { TaskFail(FAIL_NO_TARGET); return; }
        if ((m_hCleanupTarget->GetAbsOrigin() - GetAbsOrigin()).Length2D() > 160)
        {
            AI_NavGoal_t goal(m_hCleanupTarget->GetAbsOrigin(), ACT_WALK, 128);
            if (!GetNavigator()->SetGoal(goal)) { TaskFail(FAIL_NO_ROUTE); return; }
        }
        TaskComplete();
        return;
    case TASK_CREMATOR_FACE_CLEANUP:
        if (!IsCorpse(m_hCleanupTarget)) { TaskFail(FAIL_NO_TARGET); return; }
        GetMotor()->SetIdealYawToTarget(m_hCleanupTarget->WorldSpaceCenter());
        return;
    case TASK_CREMATOR_IMMOLATE:
        if (!GetEnemy() && !IsCorpse(m_hCleanupTarget)) { TaskFail(FAIL_NO_TARGET); return; }
        SetActivity(ACT_RANGE_ATTACK1);
        BeginImmolating(GetEnemy() == NULL);
        return;
    default:
        BaseClass::StartTask(task);
    }
}

void CNPC_Cremator::RunTask(const Task_t *task)
{
    switch (task->iTask)
    {
    case TASK_CREMATOR_FACE_CLEANUP:
        if (!IsCorpse(m_hCleanupTarget)) { TaskFail(FAIL_NO_TARGET); return; }
        GetMotor()->SetIdealYawToTargetAndUpdate(m_hCleanupTarget->WorldSpaceCenter());
        if (FacingIdeal()) TaskComplete();
        return;
    case TASK_CREMATOR_IMMOLATE:
        if (!UpdateImmolator() || gpGlobals->curtime >= m_flAttackEnd)
        {
            StopImmolating();
            TaskComplete();
        }
        return;
    default:
        BaseClass::RunTask(task);
    }
}

void CNPC_Cremator::PrescheduleThink()
{
    BaseClass::PrescheduleThink();
    StartBreathing();
    // The recovered run clip has footsteps; the recovered walk clip has none.
    // Walk footsteps use phase crossings, avoiding duplicated authored run events.
    if (GetActivity() == ACT_WALK && IsMoving())
    {
        const float cycle = GetCycle();
        if (GetSequence() == m_nLastFootstepSequence)
        {
            if (cycle < m_flLastFootstepCycle) EmitSound("NPC_Cremator.LeftFootstep");
            if (m_flLastFootstepCycle < 0.5f && cycle >= 0.5f) EmitSound("NPC_Cremator.RightFootstep");
        }
        m_flLastFootstepCycle = cycle;
        m_nLastFootstepSequence = GetSequence();
    }
    else m_nLastFootstepSequence = -1;
    if (m_bImmolating)
    {
        const Task_t *task = GetTask();
        if (!task || task->iTask != TASK_CREMATOR_IMMOLATE || !TaskIsRunning()) StopImmolating();
    }
}

void CNPC_Cremator::HandleAnimEvent(animevent_t *event)
{
    // Recovered cremator.mdl run1 uses the shared left/right footstep events.
    if (event->event == AE_NPC_LEFTFOOT || event->event == 2050) { EmitSound("NPC_Cremator.LeftFootstep"); return; }
    if (event->event == AE_NPC_RIGHTFOOT || event->event == 2051) { EmitSound("NPC_Cremator.RightFootstep"); return; }
    BaseClass::HandleAnimEvent(event);
}

void CNPC_Cremator::Event_Killed(const CTakeDamageInfo &info)
{
    StopImmolating();
    StopLoopingSounds();
    if (m_bDropImmolator && !HasSpawnFlags(SF_NPC_NO_WEAPON_DROP))
    {
        CBaseCombatWeapon *weapon = dynamic_cast<CBaseCombatWeapon *>(CreateEntityByName("weapon_immolator"));
        if (weapon)
        {
            weapon->SetAbsOrigin(MuzzlePosition());
            weapon->SetAbsAngles(GetAbsAngles());
            if (DispatchSpawn(weapon) >= 0) weapon->Drop(vec3_origin);
        }
    }
    BaseClass::Event_Killed(info);
}

CBaseEntity *CNPC_Cremator::CreateDeathPart(const char *classname, const char *model,
    const Vector &origin, const Vector &force)
{
    CBaseEntity *part = CreateEntityByName(classname);
    if (!part) return NULL;
    EHANDLE handle = part;
    part->KeyValue("model", model);
    part->SetAbsOrigin(origin);
    part->SetAbsAngles(GetAbsAngles());
    if (DispatchSpawn(part) < 0 || !handle || handle->IsMarkedForDeletion()) return NULL;
    part->Activate();
    if (part->VPhysicsGetObject()) part->VPhysicsGetObject()->ApplyForceCenter(force);
    return part;
}

bool CNPC_Cremator::BecomeRagdoll(const CTakeDamageInfo &info, const Vector &force)
{
    // The supplied complete Cremator lacks a .phy; its separate body/head have
    // original physics data. Use those assets rather than a broken client ragdoll.
    CBaseEntity *body = CreateDeathPart("prop_ragdoll", CREMATOR_BODY, GetAbsOrigin(), force);
    Vector headOrigin = EyePosition();
    QAngle headAngles;
    if (m_nHeadAttachment > 0) GetAttachment(m_nHeadAttachment, headOrigin, headAngles);
    CBaseEntity *head = CreateDeathPart("prop_physics_override", CREMATOR_HEAD, headOrigin, force * 0.25f);
    if (!body && !head) return BaseClass::BecomeRagdoll(info, force);
    AddEffects(EF_NODRAW);
    AddSolidFlags(FSOLID_NOT_SOLID);
    RemoveDeferred();
    return true;
}

void CNPC_Cremator::InputEnable(inputdata_t &data)
{
    m_bEnabled = true;
    ClearSchedule("Cremator enabled");
    StartBreathing();
}

void CNPC_Cremator::InputDisable(inputdata_t &data)
{
    m_bEnabled = false;
    StopImmolating();
    StopLoopingSounds();
    GetNavigator()->ClearGoal();
    ClearSchedule("Cremator disabled");
}

void CNPC_Cremator::InputSetCleanupTarget(inputdata_t &data)
{
    CBaseEntity *target = gEntList.FindEntityByName(NULL, data.value.String(), this, data.pActivator, data.pCaller);
    if (!IsCorpse(target)) return;
    m_hCleanupTarget = target;
    if (m_bEnabled && !GetEnemy()) SetSchedule(SCHED_CREMATOR_CLEANUP);
}

AI_BEGIN_CUSTOM_NPC(npc_cremator, CNPC_Cremator)
    DECLARE_TASK(TASK_CREMATOR_GET_CLEANUP_PATH)
    DECLARE_TASK(TASK_CREMATOR_FACE_CLEANUP)
    DECLARE_TASK(TASK_CREMATOR_IMMOLATE)
    DEFINE_SCHEDULE(SCHED_CREMATOR_ATTACK,
        " Tasks"
        "  TASK_STOP_MOVING 0"
        "  TASK_FACE_ENEMY 0"
        "  TASK_CREMATOR_IMMOLATE 0"
        " Interrupts"
        "  COND_ENEMY_DEAD"
        "  COND_NEW_ENEMY"
        "  COND_HEAVY_DAMAGE"
        "  COND_HEAR_DANGER"
    )
    DEFINE_SCHEDULE(SCHED_CREMATOR_CLEANUP,
        " Tasks"
        "  TASK_CREMATOR_GET_CLEANUP_PATH 0"
        "  TASK_WALK_PATH 0"
        "  TASK_WAIT_FOR_MOVEMENT 0"
        "  TASK_STOP_MOVING 0"
        "  TASK_CREMATOR_FACE_CLEANUP 0"
        "  TASK_CREMATOR_IMMOLATE 0"
        " Interrupts"
        "  COND_NEW_ENEMY"
        "  COND_LIGHT_DAMAGE"
        "  COND_HEAVY_DAMAGE"
        "  COND_HEAR_DANGER"
    )
AI_END_CUSTOM_NPC()
