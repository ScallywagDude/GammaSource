#!/usr/bin/env python3
"""Repair server-side Houndeye AI and sonic attacks in the actual 2003 NPC.

Source modifications are idempotent, retain the original sound/animation
behavior and require in-engine verification with a legally supplied model.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'dlls/hl2_dll/npc_houndeye.cpp'
raw=p.read_bytes(); crlf=b'\r\n' in raw
s=raw.decode('latin-1').replace('\r\n','\n')
marker='HL2MODERN_HOUNDEYE_SONIC_RESTORATION'
if marker not in s:
    anchor='#include "movevars_shared.h"'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n#include "../../modernize/houndeye_sonic_policy.h"',1)

    health='ConVar\tsk_Houndeye_health( "sk_Houndeye_health","0");'
    damage='ConVar\tsk_Houndeye_dmg_blast( "sk_Houndeye_dmg_blast","0");'
    assert s.count(health)==1 and s.count(damage)==1
    s=s.replace(health,'// HL2MODERN_HOUNDEYE_SONIC_RESTORATION\n// Prototype values were zero, causing lifeless spawns and zero-damage blasts.\nConVar\tsk_Houndeye_health( "sk_Houndeye_health","60");',1)
    s=s.replace(damage,'ConVar\tsk_Houndeye_dmg_blast( "sk_Houndeye_dmg_blast","25");',1)

    anchor='ADD_CUSTOM_CONDITION(CNPC_Houndeye,\tCOND_HOUND_GROUP_RETREAT);'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n\tADD_CUSTOM_CONDITION(CNPC_Houndeye,\tCOND_HOUND_GROUP_RALLEY);',1)

    anchor='if (pEntity->Classify() == CLASS_HOUNDEYE)'
    assert s.count(anchor)==1
    s=s.replace(anchor,'if (pEntity && pEntity->Classify() == CLASS_HOUNDEYE)',1)

    # On a very short distance the original ignored the fire cooldown.
    start=s.index('\t// If I\'m really close to my enemy allow me to attack if ')
    end=s.index('\tif (flDist > ( HOUNDEYE_MAX_ATTACK_RADIUS * 0.5 ))',start)
    s=s[:start]+'''\t// Even at close range, respect the sonic cooldown. Range and facing
\t// are checked after this guard to prevent animation event spam.
\tif ( gpGlobals->curtime < m_flNextAttack )
\t{
\t\treturn COND_NONE;
\t}
\tif ( flDist < 100 && flDot >= 0.3f )
\t{
\t\treturn COND_CAN_RANGE_ATTACK1;
\t}
'''+s[end:]

    anchor='m_flNextSecondaryAttack = 0;'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n\tm_flNextAttack = 0;',1)

    anchor='UTIL_PrecacheOther("grenade_energy");'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n\tUTIL_PrecacheOther("energy_wave");',1)

    anchor='m_pEnergyWave->SetAbsVelocity( 100*vFacingDir );'
    assert s.count(anchor)==1
    s=s.replace(anchor,'if (m_pEnergyWave)\n\t\tm_pEnergyWave->SetAbsVelocity( 100*vFacingDir );',1)

    a='''\t\t\tfloat flDamageAdjuster = 1-( flDist / HOUNDEYE_MAX_ATTACK_RADIUS );'''
    assert s.count(a)==1
    s=s.replace(a,'''\t\t\tfloat flDamageAdjuster = 0.0f;''',1)
    a='''\t\t\tflDamageAdjuster   *= flDotPr;

\t\t\tif (flDamageAdjuster < 0)'''
    assert s.count(a)==1
    s=s.replace(a,'''\t\t\tflDamageAdjuster = HL2ModernHoundeye::SonicFalloff(
\t\t\t\tflDist, HOUNDEYE_MAX_ATTACK_RADIUS, flDotPr);

\t\t\tif (flDamageAdjuster <= 0.0f)''',1)

    a='''\t\t\t\tCTakeDamageInfo info( this, this, flDamageAdjuster * sk_Houndeye_dmg_blast.GetFloat(), DMG_SONIC | DMG_ALWAYSGIB );
\t\t\t\tCalculateExplosiveDamageForce( &info, (pEntity->GetAbsOrigin() - GetAbsOrigin()), pEntity->GetAbsOrigin() );

\t\t\t\tpEntity->TakeDamage( info );'''
    assert s.count(a)==1
    s=s.replace(a,'''\t\t\t\tconst float sonicDamage = HL2ModernHoundeye::BoundedDamage(
\t\t\t\t\tsk_Houndeye_dmg_blast.GetFloat(), flDamageAdjuster);
\t\t\t\tif (sonicDamage > 0.0f)
\t\t\t\t{
\t\t\t\t\tCTakeDamageInfo info( this, this, sonicDamage, DMG_SONIC | DMG_ALWAYSGIB );
\t\t\t\t\tCalculateExplosiveDamageForce( &info, (pEntity->GetAbsOrigin() - GetAbsOrigin()), pEntity->GetAbsOrigin() );
\t\t\t\t\tpEntity->TakeDamage( info );
\t\t\t\t}''',1)

    # Important: bare schedule enum expression had no effect.
    anchor='''\t\t\t\t\tm_flNextAttack = gpGlobals->curtime;
\t\t\t\t\tSCHED_HOUND_ATTACK_STRAFE;'''
    assert s.count(anchor)==1
    s=s.replace(anchor,'''\t\t\t\t\tm_flNextAttack = gpGlobals->curtime;
\t\t\t\t\treturn SCHED_HOUND_ATTACK_STRAFE;''',1)
    anchor='''if ( HasCondition( COND_LIGHT_DAMAGE ) | 
\t\t\t\t HasCondition( COND_HEAVY_DAMAGE ) )'''
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor.replace(' ) | \n',' ) ||\n'),1)

    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin-1'))
    print('Restored Houndeye health/sonic damage, safer attack and squad schedules')
else:
    expected=['HL2ModernHoundeye::SonicFalloff(',
              'HL2ModernHoundeye::BoundedDamage(',
              'return SCHED_HOUND_ATTACK_STRAFE;',
              'ADD_CUSTOM_CONDITION(CNPC_Houndeye,\tCOND_HOUND_GROUP_RALLEY);',
              'UTIL_PrecacheOther("energy_wave");',
              'if (pEntity && pEntity->Classify() == CLASS_HOUNDEYE)',
              'sk_Houndeye_health","60"', 'sk_Houndeye_dmg_blast","25"']
    for val in expected:
        if val not in s: raise RuntimeError('Incomplete prior Houndeye patch: '+val)
    if s.count(marker)!=1: raise RuntimeError('Duplicate Houndeye patch markers')
    print('Houndeye source already patched')
