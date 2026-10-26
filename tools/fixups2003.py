"""Hand fixes applied after port2003.py, per file (old -> new, exact text)."""
import re, sys
from pathlib import Path

D = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game\server\gamma")

FIX = {
    # the gauss explosion temp entity lives in the 2018 hl2/te_gaussexplosion.cpp
    'weapon_gauss.h': [
        (r're://=+\n// Gauss explosion\n//=+\n\nclass CTEGaussExplosion.*?\n};\n',
         "// Gauss explosion temp entity: game/server/hl2/te_gaussexplosion.cpp\n"
         "void TE_GaussExplosion( IRecipientFilter& filter, float delay, const Vector &pos, const Vector &dir, int type );\n"),
    ],
    'weapon_gauss.cpp': [
        (r're:CTEGaussExplosion::CTEGaussExplosion\( const char \*name \).*?g_TEGaussExplosion\.Create\( filter, delay \);\n}\n', ""),
        ("tr.endpos, 200.0f, CLASS_NONE );", "tr.endpos, 200.0f, CLASS_NONE, NULL );"),
    ],
    'grenade_hopwire.cpp': [
        ("pGrenade->SetOwner( ToBaseCombatCharacter( pOwner ) );", "pGrenade->SetThrower( ToBaseCombatCharacter( pOwner ) );"),
    ],
    'weapon_binoculars.cpp': [
        # 2003 CBaseEntity::IsSoundSource is gone; NPCs and ambient sounds are what the binoculars listen to
        ("if( pEnt->IsSoundSource() )", "if( pEnt->IsNPC() || FClassnameIs( pEnt, \"ambient_generic\" ) )"),
    ],
    'weapon_brickbat.cpp': [
        ("void CWeaponBrickbat::SetObjectCollisionBox( void )\n{\n\tBaseClass::BaseClass::SetObjectCollisionBox();\n}\n", ""),
        ("pBrickbat->SetOwner( GetOwner() );", "pBrickbat->SetThrower( GetOwner() );"),
    ],
    'weapon_brickbat.h': [
        (r're:[ \t]*void[ \t]+SetObjectCollisionBox\( void \);\n', ""),
    ],
    'grenade_brickbat.cpp': [
        ("if (GetOwner())", "if (GetThrower())"),
        ("CTakeDamageInfo info( this, GetOwner(), m_flDamage, DMG_CRUSH );", "CTakeDamageInfo info( this, GetThrower(), m_flDamage, DMG_CRUSH );"),
        ("QAngle vAngVel;", "AngularImpulse vAngVel;"),
        ("QAngleToAngularImpulse( vAngVel, impAngVel );", "impAngVel = vAngVel;"),
    ],
    'weapon_molotov.cpp': [
        ("pMolotov->SetOwner( GetOwner() );", "pMolotov->SetThrower( GetOwner() );"),
    ],
    'weapon_stickylauncher.cpp': [
        ("m_flAnimTime = gpGlobals->curtime;", "SetAnimTime( gpGlobals->curtime );"),
        ("m_flPlaybackRate = 0.0;", "SetPlaybackRate( 0.0 );"),
        ("m_fEffects |= EF_NOSHADOW|EF_NORECEIVESHADOW;", "AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );"),
    ],
    # 2003 CBaseGrenade::GetOwner/SetOwner is the 2018 GetThrower/SetThrower
    'weapon_slam.cpp': [
        (r're:pSatchel->GetOwner\(\) && GetOwner\(\) && pSatchel->GetOwner\(\)->pev == GetOwner\(\)->pev', "pSatchel->GetThrower() && GetOwner() && pSatchel->GetThrower() == GetOwner()"),
        (r're:pSatchel->SetOwner\(', "pSatchel->SetThrower("),
        ("pSatchel->GetOwner() && pSatchel->GetOwner()->pev == GetOwner()->pev", "pSatchel->GetThrower() && pSatchel->GetThrower() == GetOwner()"),
    ],
    'grenade_satchel.cpp': [
        ("if ( pOther == GetOwner() && GetOwnerEntity() == NULL )", "if ( pOther == GetThrower() && GetOwnerEntity() == NULL )"),
    ],
    'npc_rollerturret.cpp': [
        ("vecAim * 1500, pev, MOVETYPE_FLY", "vecAim * 1500, edict(), MOVETYPE_FLY"),
    ],
    'npc_combineguard.cpp': [
        ("\tfor ( i = 1; i < NUM_CGUARD_BODYGROUPS; i++ )", "\tint i;\n\tfor ( i = 1; i < NUM_CGUARD_BODYGROUPS; i++ )"),
        ("te->BreakModel( filter, 0.0, &vecDamagePoint, &vecSize, &vecVelocity, m_nGibModel, 100, 0, 2.5, BREAK_METAL );",
         "te->BreakModel( filter, 0.0, vecDamagePoint, vec3_angle, vecSize, vecVelocity, m_nGibModel, 100, 0, 2.5, BREAK_METAL );"),
    ],
    # 2018 EmitSound takes an EmitSound_t for explicit channel/volume/attenuation
    'npc_roller.cpp': [
        ("EmitSound( filter, entindex(), CHAN_BODY, pCodeSounds[ m_iAccessCode[ m_iCodeProgress ] ], 1.0, ATTN_NORM );",
         "{ EmitSound_t ep; ep.m_nChannel = CHAN_BODY; ep.m_pSoundName = pCodeSounds[ m_iAccessCode[ m_iCodeProgress ] ]; ep.m_flVolume = 1.0f; ep.m_SoundLevel = ATTN_TO_SNDLVL( ATTN_NORM ); EmitSound( filter, entindex(), ep ); }"),
        ("m_pMotionController->AttachObject( pPhysicsObject );", "m_pMotionController->AttachObject( pPhysicsObject, true );"),
    ],
    'npc_rollerbuddy.cpp': [
        ("EmitSound( filter, entindex(), CHAN_BODY, pCodeSounds[ m_iAccessCode[ m_iCodeProgress ] ], 1.0, ATTN_NORM );",
         "{ EmitSound_t ep; ep.m_nChannel = CHAN_BODY; ep.m_pSoundName = pCodeSounds[ m_iAccessCode[ m_iCodeProgress ] ]; ep.m_flVolume = 1.0f; ep.m_SoundLevel = ATTN_TO_SNDLVL( ATTN_NORM ); EmitSound( filter, entindex(), ep ); }"),
    ],
    'grenade_scanner.cpp': [
        (r're:\bGetOwner\(\)', "GetThrower()"),
    ],
    'npc_wscanner.cpp': [
        (r're:(\w+)->SetOwner\(', r"\1->SetThrower("),
    ],
    # SetObjectCollisionBox is gone; the crab's oversized bounds become specified surrounding bounds
    'npc_crabsynth.cpp': [
        (r're:\tvirtual void SetObjectCollisionBox\( void \)\n\t\{\n.*?SetAbsMaxs\( GetAbsOrigin\(\) \+ Vector\(64\.f, 64\.f, 80\.f\) \);\n\t\}\n', ""),
        (r're:(void CCrabSynth::Spawn\( void \)\n\{\n)', r"\1\t// HACKHACK (2003): the crab is much bigger than its hull\n\tstatic const Vector s_CrabMins( -64, -64, 0 ), s_CrabMaxs( 64, 64, 80 );\n\tCollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &s_CrabMins, &s_CrabMaxs );\n"),
    ],
    # 2003 Odell response concepts (the 2018 lead behaviour uses TLK_LEAD_*)
    'npc_odell.cpp': [
        (r're:(#include "tier0/memdbgon.h"\n)', "\\1\n#ifndef CONCEPT_LEAD_CATCHUP\n#define CONCEPT_LEAD_CATCHUP \"OD_CATCHUP\"\n#endif\n"),
    ],
    # the 2018 rollermine has its own CRollerController
    'npc_roller.cpp#roller': [ (r're:\bCRollerController\b', 'CRollerController2003') ],
    'NPC_Roller.h#roller': [ (r're:\bCRollerController\b', 'CRollerController2003') ],
    'grenade_energy.cpp': [ (r're:\bGetOwner\(\)', 'GetThrower()') ],
    # 2018 grenades are not CBaseCombatCharacters: there is only OnTakeDamage
    'grenade_tripmine.cpp': [ (r're:\bOnTakeDamage_Alive\b', 'OnTakeDamage') ],
    'grenade_tripmine.h': [ (r're:\bOnTakeDamage_Alive\b', 'OnTakeDamage') ],
    'weapon_tripwire.cpp': [
        ("pMine->SetOwner( GetOwner() );", "pMine->SetThrower( GetOwner() );"),
    ],
}

only = set(sys.argv[1:])
for name, fixes in FIX.items():
    if only and name not in only:
        continue
    p = D / name.split('#')[0]
    if not p.exists():
        continue
    t = p.read_text(encoding='latin-1').replace('\r\n', '\n')
    for old, new in fixes:
        if old.startswith('re:'):
            t, n = re.subn(old[3:], new, t, flags=re.S)
            if not n:
                print('  regex fix not applied in %s: %r' % (name, old[:60]))
            continue
        if old in t:
            t = t.replace(old, new)
        elif new not in t or not new:
            print('  fix not applied in %s: %r' % (name, old[:60]))
    p.write_text(t, encoding='latin-1')
print('fixups done')
