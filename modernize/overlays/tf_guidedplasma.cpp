#include "cbase.h"
#include "tf_guidedplasma.h"

// HL2MODERN_GUIDED_PLASMA: restore the omitted projectile on the existing
// powered-plasma simulation/network/rendering path. No replacement art.
ConVar weapon_combat_chargeableplasma_damage("weapon_combat_chargeableplasma_damage","10",FCVAR_NONE,"Chargeable plasma base damage");
ConVar weapon_combat_chargeableplasma_range("weapon_combat_chargeableplasma_range","500",FCVAR_NONE,"Chargeable plasma maximum flight range");

LINK_ENTITY_TO_CLASS(guided_plasma,CGuidedPlasma);
BEGIN_DATADESC(CGuidedPlasma)
    DEFINE_FIELD(CGuidedPlasma,m_hGuidanceTarget,FIELD_EHANDLE),
    DEFINE_FIELD(CGuidedPlasma,m_vecGuidanceOffset,FIELD_VECTOR),
END_DATADESC()

CGuidedPlasma *CGuidedPlasma::Create(const Vector &origin,const Vector &direction,CBaseEntity *target,const Vector &offset,CBaseEntity *owner)
{
    if (!owner) return NULL;
    CGuidedPlasma *shot=dynamic_cast<CGuidedPlasma *>(CreateEntityByName("guided_plasma"));
    if (!shot) return NULL;
    shot->SetupProjectile(origin,direction,DMG_ENERGYBEAM,owner);
    shot->m_hOwner=owner;
    shot->m_hGuidanceTarget=target;
    shot->m_vecGuidanceOffset=offset;
    shot->SetPower(1.0f);
    shot->SetDamage(max(0.0f,weapon_combat_chargeableplasma_damage.GetFloat()));
    shot->SetMaxRange(max(1.0f,weapon_combat_chargeableplasma_range.GetFloat()));
    shot->Activate();
    return shot;
}

void CGuidedPlasma::SetPowerLevel(float power)
{
    power=max(1.0f,min((float)MAX_CHARGED_POWER,power));
    SetPower(power);
    SetDamage(max(0.0f,weapon_combat_chargeableplasma_damage.GetFloat())*power);
}

void CGuidedPlasma::PerformCustomPhysics(Vector *position,Vector *velocity,QAngle *angles,QAngle *angularVelocity)
{
    CBaseEntity *target=m_hGuidanceTarget.Get();
    CBaseEntity *owner=GetOwnerEntity();
    if (target && owner && target->IsAlive() && !target->InSameTeam(owner))
    {
        Vector targetPosition;
        VectorTransform(m_vecGuidanceOffset,target->EntityToWorldTransform(),targetPosition);
        Vector desired=targetPosition-GetAbsOrigin();
        Vector direction=GetAbsVelocity();
        float speed=VectorNormalize(direction);
        if (VectorNormalize(desired)>1.0f && speed>0.0f)
        {
            float turn=max(0.0f,min(1.0f,gpGlobals->frametime*4.0f));
            direction=direction*(1.0f-turn)+desired*turn;
            if (VectorNormalize(direction)>0.0f)
            {
                // Refresh the existing replicated flight segment, preserving
                // its absolute expiry so steering cannot extend lifetime.
                float expiry=m_Shared.GetDeathTime();
                m_Shared.Init(GetAbsOrigin(),direction,speed);
                m_Shared.SetSpawnTime(gpGlobals->curtime);
                m_Shared.SetDeathTime(expiry);
            }
        }
    }
    BaseClass::PerformCustomPhysics(position,velocity,angles,angularVelocity);
}
