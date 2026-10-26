//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "grenade_hopwire.h"
#include "rope.h"
#include "rope_shared.h"
#include "beam_shared.h"
#include "physics.h"
#include "physics_saverestore.h"
#include "soundent.h"

extern ConVar sk_plr_dmg_grenade;
// HL2MODERN_HOPWIRE_COMBAT: finish the original timed tether charge.



#define TETHERHOOK_MODEL	"models/Weapons/w_hopwire.mdl"

//-----------------------------------------------------------------------------
// Tether hook
//-----------------------------------------------------------------------------

class CTetherHook : public CBaseAnimating
{
	DECLARE_CLASS( CTetherHook, CBaseAnimating );
public:
	typedef CBaseAnimating BaseClass;

	CTetherHook();
	void Precache( void );
	void UpdateOnRemove( void );
	bool	CreateVPhysics( void );
	void	Spawn( void );

	void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	void	StartTouch( CBaseEntity *pOther );

	static CTetherHook	*Create( const Vector &origin, const QAngle &angles, CGrenadeHopWire *pOwner );

	void	CreateRope( void );
	void	HookThink( void );

	DECLARE_DATADESC();

private:
	CHandle<CGrenadeHopWire>	m_hTetheredOwner;
	IPhysicsSpring				*m_pSpring;
	CHandle<CRopeKeyframe> m_pRope;
	CHandle<CSprite> m_pGlow;
	CHandle<CBeam> m_pBeam;
	bool						m_bAttached;
};

BEGIN_DATADESC( CTetherHook )
	DEFINE_FIELD( CTetherHook, m_hTetheredOwner, FIELD_EHANDLE ),
	DEFINE_PHYSPTR( CTetherHook, m_pSpring ),
	DEFINE_FIELD( CTetherHook, m_pRope, FIELD_EHANDLE ),
	DEFINE_FIELD( CTetherHook, m_pGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( CTetherHook, m_pBeam, FIELD_EHANDLE ),
	DEFINE_FIELD( CTetherHook, m_bAttached, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( CTetherHook, HookThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( tetherhook, CTetherHook );

CTetherHook::CTetherHook()
{
    m_pSpring = NULL;
    m_bAttached = false;
}

void CTetherHook::Precache( void )
{
    engine->PrecacheModel(TETHERHOOK_MODEL);
    engine->PrecacheModel("sprites/rollermine_shock.vmt");
    engine->PrecacheModel("sprites/blueflare1.vmt");
    BaseClass::Precache();
}

void CTetherHook::UpdateOnRemove( void )
{
    // Destroy springs before the owning grenade destroys its physics object.
    if (m_pSpring) { physenv->DestroySpring(m_pSpring); m_pSpring=NULL; }
    if (m_pRope) { UTIL_Remove(m_pRope); m_pRope=NULL; }
    if (m_pBeam) { UTIL_Remove(m_pBeam); m_pBeam=NULL; }
    if (m_pGlow) { UTIL_Remove(m_pGlow); m_pGlow=NULL; }
    StopSound(entindex(),"TripwireGrenade.ShootRope");
    SetThink(NULL);
    BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTetherHook::CreateVPhysics()
{
	// Create the object in the physics system
	IPhysicsObject *pPhysicsObject = VPhysicsInitNormal( SOLID_BBOX, 0, false );
	
	// Make sure I get touch called for static geometry
	if ( pPhysicsObject )
	{
		int flags = pPhysicsObject->GetCallbackFlags();
		flags |= CALLBACK_GLOBAL_TOUCH_STATIC;
		pPhysicsObject->SetCallbackFlags(flags);
	}

	return pPhysicsObject != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTetherHook::Spawn( void )
{
    m_bAttached=false;
    Precache();
    SetModel(TETHERHOOK_MODEL);
    UTIL_SetSize(this,vec3_origin,vec3_origin);
    if (!CreateVPhysics()) { UTIL_Remove(this); return; }
    SetThink(&CTetherHook::HookThink);
    SetNextThink(gpGlobals->curtime+0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTetherHook::CreateRope( void )
{
	if (!m_hTetheredOwner) return;
	//Make sure it's not already there
	if ( m_pRope == NULL )
	{
		//Creat it between ourselves and the owning grenade
		m_pRope = CRopeKeyframe::Create( this, m_hTetheredOwner, 0, 0 );
		
		if ( m_pRope != NULL )
		{
			m_pRope->m_Width		= 0.75;
			m_pRope->m_RopeLength	= 32;
			m_pRope->m_Slack		= 64;

			CPASAttenuationFilter filter( this,"TripwireGrenade.ShootRope" );
			EmitSound( filter, entindex(),"TripwireGrenade.ShootRope" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CTetherHook::StartTouch( CBaseEntity *pOther )
{
    if (!m_hTetheredOwner || m_hTetheredOwner->IsMarkedForDeletion())
    { UTIL_Remove(this); return; }
    if (m_bAttached || !pOther || !pOther->IsSolid() || pOther==m_hTetheredOwner.Get()) return;
    IPhysicsObject *pEnd=m_hTetheredOwner->VPhysicsGetObject();
    if (!pEnd || !g_PhysWorldObject) { UTIL_Remove(this); return; }
    springparams_t spring;
    spring.constant=150.0f;
    spring.damping=24.0f;
    spring.naturalLength=32;
    spring.relativeDamping=0.1f;
    spring.startPosition=GetAbsOrigin();
    spring.endPosition=m_hTetheredOwner->WorldSpaceCenter();
    spring.useLocalPositions=false;
    m_pSpring=physenv->CreateSpring(g_PhysWorldObject,pEnd,&spring);
    if (!m_pSpring) { UTIL_Remove(this); return; }
    m_bAttached=true;
    SetVelocity(vec3_origin,vec3_origin);
    SetMoveType(MOVETYPE_NONE);
    VPhysicsDestroyObject();
    EmitSound("TripwireGrenade.Hook");
    StopSound(entindex(),"TripwireGrenade.ShootRope");
    SetThink(&CTetherHook::HookThink);
    SetNextThink(gpGlobals->curtime+0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &velocity - 
//			&angVelocity - 
//-----------------------------------------------------------------------------
void CTetherHook::SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTetherHook::HookThink( void )
{
    if (!m_hTetheredOwner || m_hTetheredOwner->IsMarkedForDeletion())
    { UTIL_Remove(this); return; }
    if (m_bAttached && !m_pBeam)
    {
        m_pBeam=CBeam::BeamCreate("sprites/rollermine_shock.vmt",1.0f);
        if (m_pBeam)
        {
            m_pBeam->EntsInit(this,m_hTetheredOwner);
            m_pBeam->SetNoise(0.5f);
            m_pBeam->SetColor(255,255,255);
            m_pBeam->SetScrollRate(25);
            m_pBeam->SetBrightness(128);
            m_pBeam->SetWidth(4.0f);
            m_pBeam->SetEndWidth(1.0f);
        }
    }
    if (m_bAttached && !m_pGlow)
    {
        m_pGlow=CSprite::SpriteCreate("sprites/blueflare1.vmt",GetAbsOrigin(),false);
        if (m_pGlow)
        {
            m_pGlow->SetParent(this);
            m_pGlow->SetTransparency(kRenderTransAdd,255,255,255,255,kRenderFxNone);
            m_pGlow->SetBrightness(128,0.1f);
            m_pGlow->SetScale(0.5f,0.1f);
        }
    }
    SetNextThink(gpGlobals->curtime+0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			*pOwner - 
//-----------------------------------------------------------------------------
CTetherHook	*CTetherHook::Create( const Vector &origin, const QAngle &angles, CGrenadeHopWire *pOwner )
{
	CTetherHook *pHook = CREATE_ENTITY( CTetherHook, "tetherhook" );
	
	if ( pHook != NULL )
	{
		pHook->m_hTetheredOwner = pOwner;
		pHook->SetAbsOrigin( origin );
		pHook->SetAbsAngles( angles );
		pHook->SetOwnerEntity( (CBaseEntity *) pOwner );
		pHook->Spawn();
		if (pHook->IsMarkedForDeletion()) return NULL;
	}

	return pHook;
}

//-----------------------------------------------------------------------------

#define GRENADE_MODEL "models/Weapons/w_hopwire.mdl"

BEGIN_DATADESC( CGrenadeHopWire )
	DEFINE_FIELD( CGrenadeHopWire, m_nHooksShot, FIELD_INTEGER ),
	DEFINE_FIELD( CGrenadeHopWire, m_pGlow, FIELD_EHANDLE ),
    DEFINE_FIELD( CGrenadeHopWire, m_flTetherDeadline, FIELD_TIME ),
    DEFINE_FIELD( CGrenadeHopWire, m_flCombatDeadline, FIELD_TIME ),
    DEFINE_FIELD( CGrenadeHopWire, m_vecHookVelocity, FIELD_VECTOR ),
    DEFINE_ARRAY( CGrenadeHopWire, m_hHooks, FIELD_EHANDLE, 8 ),

	DEFINE_FUNCTION( CGrenadeHopWire, TetherThink ),
	DEFINE_FUNCTION( CGrenadeHopWire, CombatThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_grenade_hopwire, CGrenadeHopWire );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::Spawn( void )
{
    Precache();
    SetModel(GRENADE_MODEL);
    m_nHooksShot=0;
    m_pGlow=NULL;
    m_flTetherDeadline=0;
    m_flCombatDeadline=0;
    m_vecHookVelocity=vec3_origin;
    for (int i=0;i<8;++i) m_hHooks[i]=NULL;
    // Use the branch's existing grenade skill damage and base grenade radius.
    SetDamage(max(0.0f,sk_plr_dmg_grenade.GetFloat()));
    SetDamageRadius(100.0f);
    if (!CreateVPhysics()) UTIL_Remove(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrenadeHopWire::CreateVPhysics()
{
    return VPhysicsInitNormal(SOLID_BBOX,0,false)!=NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::Precache( void )
{
	engine->PrecacheModel( GRENADE_MODEL );
	engine->PrecacheModel("sprites/blueflare1.vmt");
	UTIL_PrecacheOther("tetherhook");
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timer - 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::SetTimer( float timer )
{
	SetThink( &CGrenadeHopWire::PreDetonate );
	SetNextThink( gpGlobals->curtime + timer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::CombatThink( void )
{
    if (gpGlobals->curtime>=m_flCombatDeadline)
    {
        RemoveTethers();
        // Qualified call is intentional: the override starts the hop sequence.
        BaseClass::Detonate();
        return;
    }
    if (!m_pGlow)
    {
        m_pGlow=CSprite::SpriteCreate("sprites/blueflare1.vmt",GetAbsOrigin(),false);
        if (m_pGlow)
        {
            m_pGlow->SetParent(this);
            m_pGlow->SetTransparency(kRenderTransAdd,255,255,255,255,kRenderFxNone);
            m_pGlow->SetBrightness(255,0.1f);
            m_pGlow->SetScale(1.0f,0.1f);
        }
    }
    SetNextThink(gpGlobals->curtime+0.05f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::TetherThink( void )
{
    // A full entity table must not strand a live grenade indefinitely.
    if (m_nHooksShot>=8 || gpGlobals->curtime>=m_flTetherDeadline)
    {
        m_flCombatDeadline=gpGlobals->curtime+0.75f;
        SetThink(&CGrenadeHopWire::CombatThink);
        SetNextThink(gpGlobals->curtime+0.05f);
        CSoundEnt::InsertSound(SOUND_DANGER,GetAbsOrigin(),400,0.75f);
        return;
    }
    CTetherHook *pHook=CTetherHook::Create(GetAbsOrigin(),GetAbsAngles(),this);
    if (pHook)
    {
        m_hHooks[m_nHooksShot]=pHook;
        pHook->CreateRope();
        if (m_nHooksShot%2) m_vecHookVelocity.Negate();
        else
        {
            m_vecHookVelocity=RandomVector(-1.0f,1.0f);
            if (m_vecHookVelocity.LengthSqr()<0.01f) m_vecHookVelocity=Vector(1,0,0);
            VectorNormalize(m_vecHookVelocity);
        }
        pHook->SetVelocity(m_vecHookVelocity*1500.0f,vec3_origin);
        ++m_nHooksShot;
    }
    SetNextThink(gpGlobals->curtime+0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &velocity - 
//			&angVelocity - 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	
	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopWire::Detonate( void )
{
    // Event_Killed or repeated inputs must not restart an active charge.
    if (m_flTetherDeadline>0 || m_flCombatDeadline>0) return;
    Vector hopVel=RandomVector(-8,8);
    hopVel.z+=800.0f;
    SetVelocity(hopVel,RandomAngularImpulse(-300,300));
    m_flTetherDeadline=gpGlobals->curtime+3.0f;
    SetThink(&CGrenadeHopWire::TetherThink);
    SetNextThink(gpGlobals->curtime+0.6f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseGrenade *HopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer )
{
	CGrenadeHopWire *pGrenade = (CGrenadeHopWire *) CBaseEntity::Create( "npc_grenade_hopwire", position, angles, pOwner );
	// HL2MODERN_HOPWIRE_SPAWN: an exhausted entity factory cannot supply a grenade.
	if ( !pGrenade || pGrenade->IsMarkedForDeletion() ) return NULL;
	
	pGrenade->SetTimer( timer );
	pGrenade->SetVelocity( velocity, angVelocity );
	pGrenade->SetOwner( ToBaseCombatCharacter( pOwner ) );

	return pGrenade;
}

void CGrenadeHopWire::RemoveTethers( void )
{
    for (int i=0;i<8;++i)
        if (m_hHooks[i]) { UTIL_Remove(m_hHooks[i]); m_hHooks[i]=NULL; }
    if (m_pGlow) { UTIL_Remove(m_pGlow); m_pGlow=NULL; }
}

void CGrenadeHopWire::UpdateOnRemove( void )
{
    RemoveTethers();
    SetThink(NULL);
    BaseClass::UpdateOnRemove();
}
