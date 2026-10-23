#ifndef TF_GUIDEDPLASMA_H
#define TF_GUIDEDPLASMA_H
#include "plasmaprojectile.h"
#include "tf_shareddefs.h"
// The omitted prototype used these names. Reuse the supplied plasma rifle's
// two-second charge and three-times power limits rather than unbounded values.
#define MAX_CHARGED_TIME RIFLE_CHARGE_TIME
#define MAX_CHARGED_POWER MAX_RIFLE_POWER

class CGuidedPlasma : public CPowerPlasmaProjectile
{
    DECLARE_CLASS(CGuidedPlasma,CPowerPlasmaProjectile);
public:
    DECLARE_DATADESC();
    static CGuidedPlasma *Create(const Vector &origin,const Vector &direction,CBaseEntity *target,const Vector &offset,CBaseEntity *owner);
    void SetPowerLevel(float power);
    void PerformCustomPhysics(Vector *position,Vector *velocity,QAngle *angles,QAngle *angularVelocity);
private:
    EHANDLE m_hGuidanceTarget;
    Vector m_vecGuidanceOffset;
};
#endif
