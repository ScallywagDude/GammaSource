// GammaSource: explicitly reconstructed Alien Assassin AI.
// Recovered: Assassin.mdl, its knife/stab/smoke activities and events 2/3/5,
// its tripwire/pest clips, and npc/assassin/{idle2,tattle1}.wav. No matching
// Alien Assassin AI or knife, pest or shock-tripwire projectile art survives in
// the supplied source/assets. The shock tripwire, pests and rope running below
// are new reconstruction built from the design notes, with sprite/beam visuals
// standing in for the missing art. This distinct NPC does not replace the
// recovered Combine/Female Assassin.
#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "activitylist.h"
#include "npcevent.h"
#include "eventlist.h"
#include "smoke_trail.h"
#include "SpriteTrail.h"
#include "Sprite.h"
#include "beam_shared.h"
#include "IEffects.h"
#include "rope.h"
#include "soundent.h"
#include "tier0/memdbgon.h"

static ConVar sk_alienassassin_health("sk_alienassassin_health", "120", FCVAR_NONE,
    "Reconstructed Alien Assassin health; original AI/balance is unavailable.");
static ConVar sk_alienassassin_claw("sk_alienassassin_claw", "20", FCVAR_NONE,
    "Reconstructed Alien Assassin claw damage.");
static ConVar sk_alienassassin_knife("sk_alienassassin_knife", "15", FCVAR_NONE,
    "Reconstructed Alien Assassin thrown blade damage.");
static ConVar sk_alienassassin_tripwire("sk_alienassassin_tripwire", "25", FCVAR_NONE,
    "Reconstructed Alien Assassin shock tripwire damage.");
static ConVar sk_alienassassin_pest("sk_alienassassin_pest", "4", FCVAR_NONE,
    "Reconstructed Alien Assassin pest bite damage.");
static ConVar gamma_alienassassin_tripwires("gamma_alienassassin_tripwires", "1", FCVAR_NONE,
    "Let the reconstructed Alien Assassin lay shock tripwires.");
static ConVar gamma_alienassassin_pests("gamma_alienassassin_pests", "1", FCVAR_NONE,
    "Let the reconstructed Alien Assassin release pests.");
static ConVar gamma_alienassassin_roperun("gamma_alienassassin_roperun", "1", FCVAR_NONE,
    "Let the reconstructed Alien Assassin run along map ropes.");
static ConVar gamma_alienassassin_debug("gamma_alienassassin_debug", "0", FCVAR_CHEAT,
    "Trace reconstructed Alien Assassin attacks and smoke.");

static const char *ALIEN_ASSASSIN_BEAM = "sprites/bluelaser1.vmt";
static const char *ALIEN_ASSASSIN_PEST_SPRITE = "sprites/glow01.vmt";
static const int ALIEN_ASSASSIN_MAX_TRIPWIRES = 2;
static const int ALIEN_ASSASSIN_MAX_PESTS = 6;

// Counts live helper entities of one class that belong to an owner.
static int CountOwnedEntities(const char *classname, CBaseEntity *owner)
{
    int count = 0;
    for (CBaseEntity *entity = gEntList.FindEntityByClassname(NULL, classname); entity;
        entity = gEntList.FindEntityByClassname(entity, classname))
    {
        if (entity->GetOwnerEntity() == owner && !entity->IsMarkedForDeletion()) ++count;
    }
    return count;
}

// True if owner treats this entity as something to attack.
static bool IsHostileTo(CBaseEntity *owner, CBaseEntity *entity)
{
    if (!owner || !entity || entity == owner || !entity->IsAlive()) return false;
    CBaseCombatCharacter *character = owner->MyCombatCharacterPointer();
    if (!character || !entity->MyCombatCharacterPointer()) return false;
    const Disposition_t relation = character->IRelationType(entity);
    return relation == D_HT || relation == D_FR;
}
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

// Reconstructed shock tripwire: a charged beam strung between two walls at
// knee height. The first enemy to cross it is shocked and the wire breaks.
// It goes dark when its Assassin dies.
class CAlienAssassinTripwire : public CBaseEntity
{
public:
    DECLARE_CLASS(CAlienAssassinTripwire, CBaseEntity);
    DECLARE_DATADESC();
    static CAlienAssassinTripwire *Create(CBaseEntity *owner, const Vector &start, const Vector &end);
    void Spawn();
    void Precache();
    void WireThink();
    void BreakThink() { UTIL_Remove(this); }
    void UpdateOnRemove()
    {
        if (m_hBeam) UTIL_Remove(m_hBeam);
        BaseClass::UpdateOnRemove();
    }

    Vector m_vecStart;
    Vector m_vecEnd;
    float m_flArmTime;
    float m_flDieTime;
    CHandle<CBeam> m_hBeam;
};
LINK_ENTITY_TO_CLASS(gamma_alienassassin_tripwire, CAlienAssassinTripwire);
BEGIN_DATADESC(CAlienAssassinTripwire)
    DEFINE_FIELD(m_vecStart, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_vecEnd, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_flArmTime, FIELD_TIME),
    DEFINE_FIELD(m_flDieTime, FIELD_TIME),
    DEFINE_FIELD(m_hBeam, FIELD_EHANDLE),
    DEFINE_THINKFUNC(WireThink),
    DEFINE_THINKFUNC(BreakThink),
END_DATADESC()

void CAlienAssassinTripwire::Precache()
{
    PrecacheModel(ALIEN_ASSASSIN_BEAM);
    PrecacheSound("ambient/energy/spark1.wav");
    PrecacheSound("ambient/energy/zap1.wav");
}

CAlienAssassinTripwire *CAlienAssassinTripwire::Create(CBaseEntity *owner, const Vector &start, const Vector &end)
{
    CAlienAssassinTripwire *wire = static_cast<CAlienAssassinTripwire *>(CreateEntityByName("gamma_alienassassin_tripwire"));
    if (!wire) return NULL;
    wire->m_vecStart = start;
    wire->m_vecEnd = end;
    wire->SetOwnerEntity(owner);
    wire->SetAbsOrigin((start + end) * 0.5f);
    if (DispatchSpawn(wire) < 0) return NULL;
    return wire;
}

void CAlienAssassinTripwire::Spawn()
{
    Precache();
    SetSolid(SOLID_NONE);
    SetMoveType(MOVETYPE_NONE);
    AddEffects(EF_NODRAW);
    m_flArmTime = gpGlobals->curtime + 0.75f;
    m_flDieTime = gpGlobals->curtime + 30.0f;
    m_hBeam = CBeam::BeamCreate(ALIEN_ASSASSIN_BEAM, 1.5f);
    if (m_hBeam)
    {
        m_hBeam->PointsInit(m_vecStart, m_vecEnd);
        m_hBeam->SetColor(120, 190, 255);
        m_hBeam->SetBrightness(110);
        m_hBeam->SetNoise(0.6f);
        m_hBeam->SetScrollRate(25);
        m_hBeam->RelinkBeam();
    }
    g_pEffects->Sparks(m_vecStart);
    g_pEffects->Sparks(m_vecEnd);
    EmitSound("ambient/energy/spark1.wav");
    SetThink(&CAlienAssassinTripwire::WireThink);
    SetNextThink(gpGlobals->curtime + 0.1f);
}

void CAlienAssassinTripwire::WireThink()
{
    CBaseEntity *owner = GetOwnerEntity();
    if (!owner || !owner->IsAlive() || gpGlobals->curtime >= m_flDieTime)
    {
        UTIL_Remove(this);
        return;
    }
    SetNextThink(gpGlobals->curtime + 0.1f);
    if (gpGlobals->curtime < m_flArmTime) return;

    trace_t trace;
    UTIL_TraceLine(m_vecStart, m_vecEnd, MASK_SHOT, owner, COLLISION_GROUP_NONE, &trace);
    CBaseEntity *victim = trace.m_pEnt;
    if (!victim || victim->IsWorld() || !IsHostileTo(owner, victim)) return;

    Vector direction = m_vecEnd - m_vecStart;
    VectorNormalize(direction);
    CTakeDamageInfo damage(this, owner, MAX(0.0f, sk_alienassassin_tripwire.GetFloat()), DMG_SHOCK);
    damage.SetDamagePosition(trace.endpos);
    damage.SetDamageForce(Vector(0, 0, 1) * 2000.0f);
    victim->TakeDamage(damage);
    g_pEffects->Sparks(trace.endpos, 2, 2);
    EmitSound("ambient/energy/zap1.wav");
    CSoundEnt::InsertSound(SOUND_COMBAT, trace.endpos, 384, 0.3f, owner);
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN tripwire_hit target=%d damage=%.2f\n", victim->entindex(), damage.GetDamage());

    // Flare, then break.
    if (m_hBeam)
    {
        m_hBeam->SetBrightness(255);
        m_hBeam->SetNoise(6.0f);
    }
    SetThink(&CAlienAssassinTripwire::BreakThink);
    SetNextThink(gpGlobals->curtime + 0.2f);
}

// Reconstructed pest: a small glowing alien creature that homes in on the
// Assassin's enemy and nips it. A sprite stands in for the missing pest art.
// Pests can be shot down and expire after a short time.
class CAlienAssassinPest : public CBaseEntity
{
public:
    DECLARE_CLASS(CAlienAssassinPest, CBaseEntity);
    DECLARE_DATADESC();
    void Spawn();
    void Precache();
    void PestThink();
    int OnTakeDamage(const CTakeDamageInfo &info);
    void Pop();
    void UpdateOnRemove()
    {
        if (m_hSprite) UTIL_Remove(m_hSprite);
        if (m_hTrail) UTIL_Remove(m_hTrail);
        BaseClass::UpdateOnRemove();
    }

    EHANDLE m_hTarget;
    float m_flDieTime;
    float m_flNextBite;
    float m_flRetreatUntil;
    float m_flPhase;
    CHandle<CSprite> m_hSprite;
    CHandle<CSpriteTrail> m_hTrail;
};
LINK_ENTITY_TO_CLASS(gamma_alienassassin_pest, CAlienAssassinPest);
BEGIN_DATADESC(CAlienAssassinPest)
    DEFINE_FIELD(m_hTarget, FIELD_EHANDLE),
    DEFINE_FIELD(m_flDieTime, FIELD_TIME),
    DEFINE_FIELD(m_flNextBite, FIELD_TIME),
    DEFINE_FIELD(m_flRetreatUntil, FIELD_TIME),
    DEFINE_FIELD(m_flPhase, FIELD_FLOAT),
    DEFINE_FIELD(m_hSprite, FIELD_EHANDLE),
    DEFINE_FIELD(m_hTrail, FIELD_EHANDLE),
    DEFINE_THINKFUNC(PestThink),
END_DATADESC()

void CAlienAssassinPest::Precache()
{
    PrecacheModel(ALIEN_ASSASSIN_PEST_SPRITE);
    PrecacheModel(ALIEN_ASSASSIN_BEAM);
}

void CAlienAssassinPest::Spawn()
{
    Precache();
    SetMoveType(MOVETYPE_FLY);
    SetSolid(SOLID_BBOX);
    // Debris: bullets and melee hit it, but it never blocks anyone.
    SetCollisionGroup(COLLISION_GROUP_DEBRIS);
    AddEffects(EF_NODRAW);
    UTIL_SetSize(this, Vector(-4,-4,-4), Vector(4,4,4));
    m_takedamage = DAMAGE_YES;
    SetHealth(5);
    m_flDieTime = gpGlobals->curtime + random->RandomFloat(9.0f, 11.0f);
    m_flNextBite = gpGlobals->curtime + 0.5f;
    m_flRetreatUntil = 0;
    m_flPhase = random->RandomFloat(0, 2 * M_PI);
    m_hSprite = CSprite::SpriteCreate(ALIEN_ASSASSIN_PEST_SPRITE, GetAbsOrigin(), false);
    if (m_hSprite)
    {
        m_hSprite->SetTransparency(kRenderTransAdd, 170, 230, 90, 200, kRenderFxNone);
        m_hSprite->SetScale(0.12f);
        m_hSprite->SetParent(this);
        m_hSprite->SetLocalOrigin(vec3_origin);
    }
    m_hTrail = CSpriteTrail::SpriteTrailCreate(ALIEN_ASSASSIN_BEAM, GetAbsOrigin(), false);
    if (m_hTrail)
    {
        m_hTrail->SetParent(this);
        m_hTrail->SetLocalOrigin(vec3_origin);
        m_hTrail->SetTransparency(kRenderTransAdd, 150, 210, 80, 120, kRenderFxNone);
        m_hTrail->SetStartWidth(3.0f);
        m_hTrail->SetEndWidth(0.5f);
        m_hTrail->SetLifeTime(0.25f);
    }
    SetThink(&CAlienAssassinPest::PestThink);
    SetNextThink(gpGlobals->curtime + 0.1f);
}

int CAlienAssassinPest::OnTakeDamage(const CTakeDamageInfo &info)
{
    SetHealth(GetHealth() - (int)ceilf(info.GetDamage()));
    if (GetHealth() <= 0) Pop();
    return 1;
}

void CAlienAssassinPest::Pop()
{
    if (IsMarkedForDeletion()) return;
    m_takedamage = DAMAGE_NO;
    UTIL_BloodSpray(GetAbsOrigin(), Vector(0,0,1), BLOOD_COLOR_YELLOW, 2, FX_BLOODSPRAY_ALL);
    UTIL_Remove(this);
}

void CAlienAssassinPest::PestThink()
{
    if (gpGlobals->curtime >= m_flDieTime) { Pop(); return; }
    SetNextThink(gpGlobals->curtime + 0.1f);

    // Follow the Assassin's current enemy; keep the last one if it is gone.
    CBaseEntity *owner = GetOwnerEntity();
    CAI_BaseNPC *npc = owner ? owner->MyNPCPointer() : NULL;
    if (npc && npc->GetEnemy()) m_hTarget = npc->GetEnemy();
    CBaseEntity *target = m_hTarget.Get();
    if (!target || !target->IsAlive())
    {
        SetAbsVelocity(GetAbsVelocity() * 0.8f + Vector(0, 0, 10));
        return;
    }

    const Vector goal = target->WorldSpaceCenter() + Vector(0, 0, 12);
    Vector direction = goal - GetAbsOrigin();
    const float distance = VectorNormalize(direction);

    if (distance < 36.0f && gpGlobals->curtime >= m_flNextBite)
    {
        CTakeDamageInfo damage(this, owner ? owner : this, MAX(0.0f, sk_alienassassin_pest.GetFloat()), DMG_SLASH);
        damage.SetDamagePosition(GetAbsOrigin());
        damage.SetDamageForce(direction * 500.0f);
        target->TakeDamage(damage);
        m_flNextBite = gpGlobals->curtime + 1.0f;
        m_flRetreatUntil = gpGlobals->curtime + 0.5f;
        if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN pest_bite target=%d damage=%.2f\n", target->entindex(), damage.GetDamage());
    }
    if (gpGlobals->curtime < m_flRetreatUntil) direction = -direction;

    // Erratic, insect-like weave around the line of flight.
    Vector right, up;
    VectorVectors(direction, right, up);
    const float t = gpGlobals->curtime * 6.0f + m_flPhase;
    direction += right * (0.6f * sinf(t)) + up * (0.4f * cosf(t * 1.3f));
    VectorNormalize(direction);
    SetAbsVelocity(direction * 260.0f);
}

enum
{
    SCHED_ALIENASSASSIN_SMOKE = LAST_SHARED_SCHEDULE,
    SCHED_ALIENASSASSIN_TRIPWIRE,
    SCHED_ALIENASSASSIN_PESTS,
    SCHED_ALIENASSASSIN_ROPE_RUN,
};
enum
{
    TASK_ALIENASSASSIN_SMOKE = LAST_SHARED_TASK,
    TASK_ALIENASSASSIN_TRIPWIRE,
    TASK_ALIENASSASSIN_PESTS,
    TASK_ALIENASSASSIN_ROPE_RUN,
};

// Rope running phases.
enum { ROPE_NONE = 0, ROPE_LEAP, ROPE_RUN, ROPE_DROP };

// A point along the leap onto a rope: a straight line lifted by a parabola.
static Vector RopeLeapPoint(const Vector &from, const Vector &to, float u)
{
    Vector point;
    VectorLerp(from, to, u, point);
    point.z += (32.0f + 0.15f * (to - from).Length()) * 4.0f * u * (1.0f - u);
    return point;
}

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
    void OnScheduleChange();
    void InputDeploySmoke(inputdata_t &data);

private:
    void ThrowKnife();
    void Claw();
    void ReleaseSmoke();
    Vector HandPosition();
    bool PlaySpecialSequence(const char *const *names, int count);
    bool FindTripwireSpot();
    void PlaceTripwire();
    void ReleasePests();
    bool HullPathClear(const Vector &from, const Vector &to);
    Vector RopePoint(float s) const;
    bool FindRopeRun();
    void StartRopePhase(int phase);
    void EndRopeRun();
    float m_flNextKnife;
    float m_flNextSmoke;
    float m_flSmokeTaskEnd;
    float m_flFootstepCycle;
    int m_nFootstepSequence;
    bool m_bReleasedSmoke;
    CHandle<SmokeTrail> m_hSmoke;

    // Reconstructed abilities.
    float m_flNextTripwire;
    float m_flNextPests;
    float m_flNextRopeRun;
    float m_flSpecialTaskEnd;
    bool m_bSpecialDone;
    int m_nSpecialSequence;
    Vector m_vecWireStart;
    Vector m_vecWireEnd;
    Vector m_vecRopeEnd0;     // rope end nearest the Assassin when planned
    Vector m_vecRopeEnd1;
    Vector m_vecRopeLeapFrom;
    float m_flRopeSag;
    float m_flRopeS0;         // run starts/ends this far along the rope
    float m_flRopeS1;
    int m_iRopePhase;
    float m_flRopePhaseStart;
    float m_flRopePhaseDuration;
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
    DEFINE_FIELD(m_flNextTripwire, FIELD_TIME),
    DEFINE_FIELD(m_flNextPests, FIELD_TIME),
    DEFINE_FIELD(m_flNextRopeRun, FIELD_TIME),
    DEFINE_FIELD(m_flSpecialTaskEnd, FIELD_TIME),
    DEFINE_FIELD(m_bSpecialDone, FIELD_BOOLEAN),
    DEFINE_FIELD(m_nSpecialSequence, FIELD_INTEGER),
    DEFINE_FIELD(m_vecWireStart, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_vecWireEnd, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_vecRopeEnd0, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_vecRopeEnd1, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_vecRopeLeapFrom, FIELD_POSITION_VECTOR),
    DEFINE_FIELD(m_flRopeSag, FIELD_FLOAT),
    DEFINE_FIELD(m_flRopeS0, FIELD_FLOAT),
    DEFINE_FIELD(m_flRopeS1, FIELD_FLOAT),
    DEFINE_FIELD(m_iRopePhase, FIELD_INTEGER),
    DEFINE_FIELD(m_flRopePhaseStart, FIELD_TIME),
    DEFINE_FIELD(m_flRopePhaseDuration, FIELD_FLOAT),
    DEFINE_INPUTFUNC(FIELD_VOID, "DeploySmoke", InputDeploySmoke),
END_DATADESC()

CNPC_AlienAssassin::CNPC_AlienAssassin()
{
    m_flNextKnife = m_flNextSmoke = m_flSmokeTaskEnd = m_flFootstepCycle = 0;
    m_nFootstepSequence = -1;
    m_bReleasedSmoke = false;
    m_flNextTripwire = m_flNextPests = m_flNextRopeRun = m_flSpecialTaskEnd = 0;
    m_bSpecialDone = false;
    m_nSpecialSequence = -1;
    m_vecWireStart = m_vecWireEnd = vec3_origin;
    m_vecRopeEnd0 = m_vecRopeEnd1 = m_vecRopeLeapFrom = vec3_origin;
    m_flRopeSag = m_flRopeS0 = m_flRopeS1 = 0;
    m_iRopePhase = ROPE_NONE;
    m_flRopePhaseStart = m_flRopePhaseDuration = 0;
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
    UTIL_PrecacheOther("gamma_alienassassin_tripwire");
    UTIL_PrecacheOther("gamma_alienassassin_pest");
    PrecacheSound("ambient/creatures/flies1.wav");
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
    // Stagger the reconstructed abilities so they do not all open a fight.
    m_flNextRopeRun = gpGlobals->curtime + 2;
    m_flNextPests = gpGlobals->curtime + 4;
    m_flNextTripwire = gpGlobals->curtime + 6;
    if (gamma_alienassassin_debug.GetBool())
    {
        Msg("GAMMA_ALIENASSASSIN sequences tripwire=%d pest=%d pests=%d\n", LookupSequence("tripwire"),
            LookupSequence("pest"), LookupSequence("pests"));
    }
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

        const float distance = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).Length();
        if (gamma_alienassassin_roperun.GetBool() && gpGlobals->curtime >= m_flNextRopeRun)
        {
            if (FindRopeRun()) return SCHED_ALIENASSASSIN_ROPE_RUN;
            m_flNextRopeRun = gpGlobals->curtime + 2;
        }
        if (gamma_alienassassin_pests.GetBool() && gpGlobals->curtime >= m_flNextPests &&
            HasCondition(COND_SEE_ENEMY) && distance > 160 && distance < 900 &&
            CountOwnedEntities("gamma_alienassassin_pest", this) + 3 <= ALIEN_ASSASSIN_MAX_PESTS)
            return SCHED_ALIENASSASSIN_PESTS;
        if (gamma_alienassassin_tripwires.GetBool() && gpGlobals->curtime >= m_flNextTripwire &&
            distance > 192 &&
            CountOwnedEntities("gamma_alienassassin_tripwire", this) < ALIEN_ASSASSIN_MAX_TRIPWIRES)
        {
            if (FindTripwireSpot()) return SCHED_ALIENASSASSIN_TRIPWIRE;
            m_flNextTripwire = gpGlobals->curtime + 3;
        }
        if (HasCondition(COND_CAN_RANGE_ATTACK1)) return SCHED_RANGE_ATTACK1;
        return SCHED_CHASE_ENEMY;
    }
    return BaseClass::SelectSchedule();
}

// Plays one of the model's named clips (the supplied Assassin.mdl carries
// tripwire and pest clips without activities). Returns false if none exist.
bool CNPC_AlienAssassin::PlaySpecialSequence(const char *const *names, int count)
{
    m_nSpecialSequence = -1;
    for (int i = 0; i < count && m_nSpecialSequence < 0; ++i) m_nSpecialSequence = LookupSequence(names[i]);
    if (m_nSpecialSequence < 0) return false;
    SetIdealActivity(ACT_DO_NOT_DISTURB);
    ResetSequence(m_nSpecialSequence);
    SetCycle(0);
    return true;
}

// A wire across the corridor the Assassin stands in, perpendicular to its
// enemy, at knee height so anything that follows walks into it.
bool CNPC_AlienAssassin::FindTripwireSpot()
{
    if (!GetEnemy()) return false;
    Vector toEnemy = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
    toEnemy.z = 0;
    if (VectorNormalize(toEnemy) < 1.0f) return false;
    const Vector right(toEnemy.y, -toEnemy.x, 0);
    const Vector center = GetAbsOrigin() + Vector(0, 0, 22);
    trace_t left, rightTrace;
    UTIL_TraceLine(center, center - right * 256, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &left);
    UTIL_TraceLine(center, center + right * 256, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &rightTrace);
    if (left.fraction >= 1.0f || rightTrace.fraction >= 1.0f || left.startsolid || rightTrace.startsolid) return false;
    const Vector start = left.endpos + left.plane.normal;
    const Vector end = rightTrace.endpos + rightTrace.plane.normal;
    const float span = (end - start).Length();
    if (span < 48.0f || span > 480.0f) return false;
    // Do not stack wires on top of each other.
    for (CBaseEntity *entity = gEntList.FindEntityByClassname(NULL, "gamma_alienassassin_tripwire"); entity;
        entity = gEntList.FindEntityByClassname(entity, "gamma_alienassassin_tripwire"))
    {
        if ((entity->GetAbsOrigin() - center).LengthSqr() < 96.0f * 96.0f) return false;
    }
    m_vecWireStart = start;
    m_vecWireEnd = end;
    return true;
}

void CNPC_AlienAssassin::PlaceTripwire()
{
    if (m_bSpecialDone) return;
    m_bSpecialDone = true;
    CAlienAssassinTripwire *wire = CAlienAssassinTripwire::Create(this, m_vecWireStart, m_vecWireEnd);
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN tripwire id=%d wire=%d\n", entindex(), wire ? wire->entindex() : -1);
}

void CNPC_AlienAssassin::ReleasePests()
{
    if (m_bSpecialDone) return;
    m_bSpecialDone = true;
    const Vector hand = HandPosition();
    Vector forward;
    GetVectors(&forward, NULL, NULL);
    for (int i = 0; i < 3; ++i)
    {
        CBaseEntity *pest = CreateEntityByName("gamma_alienassassin_pest");
        if (!pest) break;
        pest->SetOwnerEntity(this);
        pest->SetAbsOrigin(hand + forward * 8);
        if (DispatchSpawn(pest) < 0) continue;
        Vector launch = forward + RandomVector(-0.5f, 0.5f);
        launch.z += 0.4f;
        VectorNormalize(launch);
        pest->SetAbsVelocity(launch * 200.0f);
    }
    EmitSound("ambient/creatures/flies1.wav");
    CSoundEnt::InsertSound(SOUND_COMBAT, hand, 256, 0.25f, this);
    if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN pests id=%d\n", entindex());
}

bool CNPC_AlienAssassin::HullPathClear(const Vector &from, const Vector &to)
{
    trace_t trace;
    AI_TraceHull(from, to, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &trace);
    return !trace.startsolid && trace.fraction >= 1.0f;
}

// Ropes hang in a curve; approximate it with a parabola whose sag matches the
// rope's slack (the extra length over the straight-line span).
Vector CNPC_AlienAssassin::RopePoint(float s) const
{
    Vector point;
    VectorLerp(m_vecRopeEnd0, m_vecRopeEnd1, s, point);
    point.z -= 4.0f * m_flRopeSag * s * (1.0f - s);
    return point;
}

// Looks for a map rope the Assassin can leap onto, run along and drop off to
// close on its enemy (or to get away when badly hurt). Every leg is
// hull-traced before it commits.
bool CNPC_AlienAssassin::FindRopeRun()
{
    if (!GetEnemy() || !(GetFlags() & FL_ONGROUND)) return false;
    const Vector feet = GetAbsOrigin();
    const Vector enemy = GetEnemy()->GetAbsOrigin();
    const float enemyDistance = (enemy - feet).Length();
    const bool escaping = GetHealth() < GetMaxHealth() / 2;
    if (!escaping && enemyDistance < 384.0f) return false;

    static const char *const ropeClasses[] = { "move_rope", "keyframe_rope" };
    for (int c = 0; c < 2; ++c)
    {
        for (CBaseEntity *entity = gEntList.FindEntityByClassname(NULL, ropeClasses[c]); entity;
            entity = gEntList.FindEntityByClassname(entity, ropeClasses[c]))
        {
            CRopeKeyframe *rope = dynamic_cast<CRopeKeyframe *>(entity);
            Vector a, b;
            if (!rope || !rope->GetEndPointPositions(a, b)) continue;
            if ((a - feet).LengthSqr() > (b - feet).LengthSqr()) { Vector t = a; a = b; b = t; }

            const float span = (b - a).Length();
            const float spanXY = (b - a).Length2D();
            if (span < 128.0f || span > 1024.0f || fabsf(b.z - a.z) > spanXY * 0.5f) continue;
            const Vector toNear = a - feet;
            if (toNear.Length2D() > 400.0f || toNear.z < -48.0f || toNear.z > 320.0f) continue;

            m_vecRopeEnd0 = a;
            m_vecRopeEnd1 = b;
            m_flRopeSag = MIN(sqrtf(3.0f * span * MAX(0, (int)rope->m_Slack.Get()) / 8.0f), span * 0.25f);
            // Stay clear of whatever the rope is tied to.
            const float inset = MIN(48.0f, span * 0.15f) / span;
            m_flRopeS0 = inset;
            m_flRopeS1 = 1.0f - inset;
            const Vector runStart = RopePoint(m_flRopeS0);
            const Vector runEnd = RopePoint(m_flRopeS1);

            // Where it lands after dropping off the far end.
            trace_t ground;
            AI_TraceHull(runEnd, runEnd - Vector(0, 0, 1024), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID,
                this, COLLISION_GROUP_NONE, &ground);
            if (ground.startsolid || ground.fraction >= 1.0f) continue;
            const float landDistance = (enemy - ground.endpos).Length();
            if (escaping ? landDistance < enemyDistance + 128.0f : landDistance + 128.0f > enemyDistance) continue;

            // Leap arc, then the run along the rope.
            bool clear = true;
            Vector previous = feet + Vector(0, 0, 2);
            for (int i = 1; i <= 6 && clear; ++i)
            {
                const Vector point = RopeLeapPoint(feet, runStart, i / 6.0f);
                clear = HullPathClear(previous, point);
                previous = point;
            }
            for (int i = 1; i <= 8 && clear; ++i)
            {
                const Vector point = RopePoint(m_flRopeS0 + (m_flRopeS1 - m_flRopeS0) * i / 8.0f);
                clear = HullPathClear(previous, point);
                previous = point;
            }
            if (!clear) continue;

            if (gamma_alienassassin_debug.GetBool()) Msg("GAMMA_ALIENASSASSIN rope_run id=%d rope=%d escaping=%d\n", entindex(), rope->entindex(), escaping);
            return true;
        }
    }
    return false;
}

void CNPC_AlienAssassin::StartRopePhase(int phase)
{
    m_iRopePhase = phase;
    m_flRopePhaseStart = gpGlobals->curtime;
    if (phase == ROPE_LEAP)
    {
        m_vecRopeLeapFrom = GetAbsOrigin();
        m_flRopePhaseDuration = clamp((RopePoint(m_flRopeS0) - m_vecRopeLeapFrom).Length() / 350.0f, 0.35f, 1.0f);
        SetMoveType(MOVETYPE_FLY);
        SetGroundEntity(NULL);
        SetAbsVelocity(vec3_origin);
        SetIdealActivity(SelectWeightedSequence(ACT_JUMP) >= 0 ? ACT_JUMP : ACT_RUN);
    }
    else if (phase == ROPE_RUN)
    {
        const float length = (RopePoint(m_flRopeS1) - RopePoint(m_flRopeS0)).Length();
        m_flRopePhaseDuration = MAX(0.3f, length / 320.0f);
        SetIdealActivity(ACT_RUN);
    }
    else if (phase == ROPE_DROP)
    {
        // Hand back to normal step physics and let gravity bring it down.
        Vector forward = m_vecRopeEnd1 - m_vecRopeEnd0;
        forward.z = 0;
        VectorNormalize(forward);
        SetMoveType(MOVETYPE_STEP);
        SetGroundEntity(NULL);
        SetAbsVelocity(forward * 150.0f);
        m_flRopePhaseDuration = 3.0f;
        SetIdealActivity(SelectWeightedSequence(ACT_GLIDE) >= 0 ? ACT_GLIDE : ACT_IDLE);
    }
}

void CNPC_AlienAssassin::EndRopeRun()
{
    if (m_iRopePhase == ROPE_LEAP || m_iRopePhase == ROPE_RUN)
    {
        // Interrupted in the air: fall from wherever it is.
        SetMoveType(MOVETYPE_STEP);
        SetGroundEntity(NULL);
    }
    m_iRopePhase = ROPE_NONE;
}

void CNPC_AlienAssassin::OnScheduleChange()
{
    EndRopeRun();
    // Hand animation back to the activity system after a named clip.
    if (GetIdealActivity() == ACT_DO_NOT_DISTURB) SetIdealActivity(ACT_IDLE);
    m_nSpecialSequence = -1;
    BaseClass::OnScheduleChange();
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
    if (task->iTask == TASK_ALIENASSASSIN_TRIPWIRE)
    {
        static const char *const names[] = { "tripwire", "Tripwire", "wire" };
        m_bSpecialDone = false;
        m_flNextTripwire = gpGlobals->curtime + 12;
        m_flSpecialTaskEnd = gpGlobals->curtime + 2.5f;
        if (!PlaySpecialSequence(names, ARRAYSIZE(names))) { PlaceTripwire(); TaskComplete(); }
        return;
    }
    if (task->iTask == TASK_ALIENASSASSIN_PESTS)
    {
        static const char *const names[] = { "pest", "pests", "Pest" };
        m_bSpecialDone = false;
        m_flNextPests = gpGlobals->curtime + 14;
        m_flSpecialTaskEnd = gpGlobals->curtime + 2.5f;
        if (!PlaySpecialSequence(names, ARRAYSIZE(names))) { ReleasePests(); TaskComplete(); }
        return;
    }
    if (task->iTask == TASK_ALIENASSASSIN_ROPE_RUN)
    {
        m_flNextRopeRun = gpGlobals->curtime + 10;
        GetNavigator()->ClearGoal();
        StartRopePhase(ROPE_LEAP);
        return;
    }
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
    if (task->iTask == TASK_ALIENASSASSIN_TRIPWIRE || task->iTask == TASK_ALIENASSASSIN_PESTS)
    {
        if (GetEnemy()) GetMotor()->SetIdealYawToTargetAndUpdate(GetEnemy()->GetAbsOrigin());
        if (IsSequenceFinished() || gpGlobals->curtime >= m_flSpecialTaskEnd)
        {
            // The clip's own event normally fires first; this covers clips without one.
            if (task->iTask == TASK_ALIENASSASSIN_TRIPWIRE) PlaceTripwire();
            else ReleasePests();
            if (GetIdealActivity() == ACT_DO_NOT_DISTURB) SetIdealActivity(ACT_IDLE);
            TaskComplete();
        }
        return;
    }
    if (task->iTask == TASK_ALIENASSASSIN_ROPE_RUN)
    {
        const float u = clamp((gpGlobals->curtime - m_flRopePhaseStart) / MAX(0.01f, m_flRopePhaseDuration), 0.0f, 1.0f);
        Vector position = GetAbsOrigin();
        Vector heading = m_vecRopeEnd1 - m_vecRopeEnd0;
        if (m_iRopePhase == ROPE_LEAP)
        {
            const Vector target = RopePoint(m_flRopeS0);
            position = RopeLeapPoint(m_vecRopeLeapFrom, target, u);
            heading = target - m_vecRopeLeapFrom;
            SetAbsOrigin(position);
            if (u >= 1.0f) StartRopePhase(ROPE_RUN);
        }
        else if (m_iRopePhase == ROPE_RUN)
        {
            position = RopePoint(m_flRopeS0 + (m_flRopeS1 - m_flRopeS0) * u);
            SetAbsOrigin(position);
            if (u >= 1.0f) StartRopePhase(ROPE_DROP);
        }
        else if (m_iRopePhase == ROPE_DROP)
        {
            if ((GetFlags() & FL_ONGROUND) || u >= 1.0f)
            {
                m_iRopePhase = ROPE_NONE;
                SetMoveType(MOVETYPE_STEP);
                TaskComplete();
            }
            return;
        }
        else
        {
            TaskFail(FAIL_NO_ROUTE);
            return;
        }
        heading.z = 0;
        if (VectorNormalize(heading) > 0.1f)
        {
            QAngle angles = GetAbsAngles();
            angles.y = UTIL_VecToYaw(heading);
            SetAbsAngles(angles);
            GetMotor()->SetIdealYaw(angles.y);
        }
        return;
    }
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
    // The tripwire/pest clips' numeric events are not documented; whichever one
    // they carry releases the ability at its authored time.
    if (!(event->type & AE_TYPE_NEWEVENTSYSTEM) && event->event >= 1 && event->event <= 7 &&
        m_nSpecialSequence >= 0 && GetSequence() == m_nSpecialSequence && GetIdealActivity() == ACT_DO_NOT_DISTURB)
    {
        if (IsCurSchedule(SCHED_ALIENASSASSIN_TRIPWIRE, false)) { PlaceTripwire(); return; }
        if (IsCurSchedule(SCHED_ALIENASSASSIN_PESTS, false)) { ReleasePests(); return; }
    }
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
    EndRopeRun();
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
    DECLARE_TASK(TASK_ALIENASSASSIN_TRIPWIRE)
    DECLARE_TASK(TASK_ALIENASSASSIN_PESTS)
    DECLARE_TASK(TASK_ALIENASSASSIN_ROPE_RUN)
    DEFINE_SCHEDULE(SCHED_ALIENASSASSIN_TRIPWIRE,
        " Tasks"
        "  TASK_STOP_MOVING 0"
        "  TASK_ALIENASSASSIN_TRIPWIRE 0"
        "  TASK_FIND_COVER_FROM_ENEMY 0"
        "  TASK_RUN_PATH 0"
        "  TASK_WAIT_FOR_MOVEMENT 0"
        " Interrupts"
        "  COND_NEW_ENEMY"
        "  COND_ENEMY_DEAD"
        "  COND_CAN_MELEE_ATTACK1"
    )
    DEFINE_SCHEDULE(SCHED_ALIENASSASSIN_PESTS,
        " Tasks"
        "  TASK_STOP_MOVING 0"
        "  TASK_FACE_ENEMY 0"
        "  TASK_ALIENASSASSIN_PESTS 0"
        " Interrupts"
        "  COND_ENEMY_DEAD"
        "  COND_CAN_MELEE_ATTACK1"
    )
    // No interrupts: it must not be left hanging mid-air. Damage still kills it.
    DEFINE_SCHEDULE(SCHED_ALIENASSASSIN_ROPE_RUN,
        " Tasks"
        "  TASK_STOP_MOVING 0"
        "  TASK_ALIENASSASSIN_ROPE_RUN 0"
        " Interrupts"
    )
AI_END_CUSTOM_NPC()
