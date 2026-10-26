#!/usr/bin/env python3
"""Finish the original hop/eight-tether sequence with a bounded grenade burst.

This reconstructs the unfinished combat transition using the existing grenade
explosion, skill damage, rope/spring effects and original hopwire art. It does
not claim to reproduce an undocumented historical damage design.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MARKER = "HL2MODERN_HOPWIRE_COMBAT"


def method(source, signature, body):
    start = source.index(signature)
    opening = source.index("{", start)
    end = opening + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[:start] + signature + "\n{\n" + body.strip("\n") + "\n}" + source[end:]


def header(source):
    if MARKER in source:
        return source
    source = source.replace('class CGrenadeHopWire :', 'class CTetherHook;\n\nclass CGrenadeHopWire :', 1)
    source = source.replace('\tvoid\tDetonate( void );', '\tvoid\tDetonate( void );\n\tvoid\tUpdateOnRemove( void );\n\tvoid\tRemoveTethers( void );', 1)
    source = source.replace('\tCSprite\t\t*m_pGlow;', '''\t// HL2MODERN_HOPWIRE_COMBAT: saved finite stages and owned child entities.
\tfloat m_flTetherDeadline;
\tfloat m_flCombatDeadline;
\tVector m_vecHookVelocity;
\tCHandle<CTetherHook> m_hHooks[8];
\tCHandle<CSprite> m_pGlow;''', 1)
    return source


def restore(source):
    if MARKER in source:
        return source
    source = source.replace('#include "physics_saverestore.h"', '#include "physics_saverestore.h"\n#include "soundent.h"\n\nextern ConVar sk_plr_dmg_grenade;\n// HL2MODERN_HOPWIRE_COMBAT: finish the original timed tether charge.\n', 1)
    source = source.replace('\tbool\tCreateVPhysics( void );', '\tCTetherHook();\n\tvoid Precache( void );\n\tvoid UpdateOnRemove( void );\n\tbool\tCreateVPhysics( void );', 1)
    source = source.replace('CRopeKeyframe\t\t\t\t*m_pRope;', 'CHandle<CRopeKeyframe> m_pRope;').replace('CSprite\t\t\t\t\t\t*m_pGlow;', 'CHandle<CSprite> m_pGlow;').replace('CBeam\t\t\t\t\t\t*m_pBeam;', 'CHandle<CBeam> m_pBeam;')
    for name in ('m_pRope', 'm_pGlow', 'm_pBeam'):
        source = re.sub(r'(DEFINE_FIELD\( CTetherHook, '+name+r', )FIELD_CLASSPTR', r'\1FIELD_EHANDLE', source)
    source = source.replace('LINK_ENTITY_TO_CLASS( tetherhook, CTetherHook );', '''LINK_ENTITY_TO_CLASS( tetherhook, CTetherHook );

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
}''', 1)
    source = source.replace('\treturn true;\n}\n\n//-----------------------------------------------------------------------------\n// Purpose: \n//-----------------------------------------------------------------------------\nvoid CTetherHook::Spawn', '\treturn pPhysicsObject != NULL;\n}\n\n//-----------------------------------------------------------------------------\n// Purpose: \n//-----------------------------------------------------------------------------\nvoid CTetherHook::Spawn', 1)
    source = method(source, 'void CTetherHook::Spawn( void )', '''
    m_bAttached=false;
    Precache();
    SetModel(TETHERHOOK_MODEL);
    UTIL_SetSize(this,vec3_origin,vec3_origin);
    if (!CreateVPhysics()) { UTIL_Remove(this); return; }
    SetThink(&CTetherHook::HookThink);
    SetNextThink(gpGlobals->curtime+0.1f);
''')
    source = source.replace('\t//Make sure it\'s not already there\n\tif ( m_pRope == NULL )', '\tif (!m_hTetheredOwner) return;\n\t//Make sure it\'s not already there\n\tif ( m_pRope == NULL )', 1)
    source = method(source, 'void CTetherHook::StartTouch( CBaseEntity *pOther )', '''
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
''')
    source = method(source, 'void CTetherHook::HookThink( void )', '''
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
''')
    source = source.replace('\t\tpHook->Spawn();', '\t\tpHook->Spawn();\n\t\tif (pHook->IsMarkedForDeletion()) return NULL;', 1)
    source = source.replace('\tDEFINE_FIELD( CGrenadeHopWire, m_pGlow, FIELD_CLASSPTR ),', '''\tDEFINE_FIELD( CGrenadeHopWire, m_pGlow, FIELD_EHANDLE ),
    DEFINE_FIELD( CGrenadeHopWire, m_flTetherDeadline, FIELD_TIME ),
    DEFINE_FIELD( CGrenadeHopWire, m_flCombatDeadline, FIELD_TIME ),
    DEFINE_FIELD( CGrenadeHopWire, m_vecHookVelocity, FIELD_VECTOR ),
    DEFINE_ARRAY( CGrenadeHopWire, m_hHooks, FIELD_EHANDLE, 8 ),''', 1)
    source = method(source, 'void CGrenadeHopWire::Spawn( void )', '''
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
''')
    source = method(source, 'bool CGrenadeHopWire::CreateVPhysics()', '''
    return VPhysicsInitNormal(SOLID_BBOX,0,false)!=NULL;
''')
    source = source.replace('\tengine->PrecacheModel( GRENADE_MODEL );', '\tengine->PrecacheModel( GRENADE_MODEL );\n\tengine->PrecacheModel("sprites/blueflare1.vmt");\n\tUTIL_PrecacheOther("tetherhook");', 1)
    source = method(source, 'void CGrenadeHopWire::CombatThink( void )', '''
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
''')
    source = method(source, 'void CGrenadeHopWire::TetherThink( void )', '''
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
''')
    source = method(source, 'void CGrenadeHopWire::Detonate( void )', '''
    // Event_Killed or repeated inputs must not restart an active charge.
    if (m_flTetherDeadline>0 || m_flCombatDeadline>0) return;
    Vector hopVel=RandomVector(-8,8);
    hopVel.z+=800.0f;
    SetVelocity(hopVel,RandomAngularImpulse(-300,300));
    m_flTetherDeadline=gpGlobals->curtime+3.0f;
    SetThink(&CGrenadeHopWire::TetherThink);
    SetNextThink(gpGlobals->curtime+0.6f);
''')
    source += '''
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
'''
    source = source.replace('if ( !pGrenade ) return NULL;', 'if ( !pGrenade || pGrenade->IsMarkedForDeletion() ) return NULL;', 1)
    source = source.replace('SetThink( PreDetonate );', 'SetThink( &CGrenadeHopWire::PreDetonate );')
    return source


def main():
    for relative, change in (('dlls/hl2_dll/grenade_hopwire.h', header), ('dlls/hl2_dll/grenade_hopwire.cpp', restore)):
        path=ROOT/relative
        raw=path.read_bytes()
        source=raw.decode('latin1').replace('\r\n','\n')
        updated=change(source)
        if source!=updated:
            path.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in raw else updated).encode('latin1'))
    print('Hopwire finite tether combat and child cleanup restored; runtime verification required.')


if __name__=='__main__':
    main()
