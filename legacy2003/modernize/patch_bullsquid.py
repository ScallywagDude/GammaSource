#!/usr/bin/env python3
"""Restore essential Bullsquid combat without requiring an unknown anim event ID.

The 2003 implementation's melee whip event is commented out and its skill
health/damage defaults are all zero. Reuse the registered tail-whip sound
animation event to deliver one range-limited hit with optional knockback.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'dlls/hl2_dll/npc_bullsquid.cpp'
raw=p.read_bytes(); crlf=b'\r\n' in raw
s=raw.decode('latin-1').replace('\r\n','\n')
marker='HL2MODERN_BULLSQUID_COMBAT_RESTORATION'
if marker not in s:
    for name,old,new in [('health','0','70'),('dmg_bite','0','20'),('dmg_whip','0','35')]:
        anchor='ConVar sk_bullsquid_'+name+' ( "sk_bullsquid_'+name+'", "'+old+'" );'
        assert s.count(anchor)==1,(name,s.count(anchor))
        s=s.replace(anchor,(' // HL2MODERN_BULLSQUID_COMBAT_RESTORATION\n' if name=='health' else '')+
                    'ConVar sk_bullsquid_'+name+' ( "sk_bullsquid_'+name+'", "'+new+'" );',1)
    # Apply damage on the existing registered tail whip event, not an invented
    # animation event ID; the older disabled code never executed.
    anchor='''\t\tcase BSQUID_AE_WHIP_SND:
\t\t{
\t\t\tEmitSound( "NPC_Bullsquid.TailWhip" );
\t\t\tbreak;
\t\t}'''
    assert s.count(anchor)==1
    replacement='''\t\tcase BSQUID_AE_WHIP_SND:
\t\t{
\t\t\tEmitSound( "NPC_Bullsquid.TailWhip" );
\t\t\tconst float whipDamage = sk_bullsquid_dmg_whip.GetFloat();
\t\t\tif (whipDamage > 0.0f && whipDamage <= 1000.0f)
\t\t\t{
\t\t\t\tCBaseEntity *pHurt = CheckTraceHullAttack( 70,
\t\t\t\t\tVector(-16,-16,-16), Vector(16,16,16), whipDamage, DMG_SLASH );
\t\t\t\tif (pHurt)
\t\t\t\t{
\t\t\t\t\tVector right, up;
\t\t\t\t\tAngleVectors( GetAbsAngles(), NULL, &right, &up );
\t\t\t\t\tpHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + right * 180 + up * 80 );
\t\t\t\t\tif (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
\t\t\t\t\t\tpHurt->RemoveFlag( FL_ONGROUND );
\t\t\t\t}
\t\t\t}
\t\t\tbreak;
\t\t}'''
    s=s.replace(anchor,replacement,1)
    # Avoid dereferencing a missing combat enemy when the schedule runs after
    # an entity has disconnected or died.
    anchor='if ( GetEnemy()->m_iHealth <= sk_bullsquid_dmg_whip.GetFloat() && flDist <= 85 && flDot >= 0.7 )'
    assert s.count(anchor)==1
    s=s.replace(anchor,'if ( GetEnemy() && GetEnemy()->m_iHealth <= sk_bullsquid_dmg_whip.GetFloat() && flDist <= 85 && flDot >= 0.7 )',1)
    # Guard failed spit projectile construction before calling Spawn().
    anchor='''\t\t\t\tCGrenadeSpit *pGrenade = (CGrenadeSpit*)CreateNoSpawn( "grenade_spit", vSpitPos, vec3_angle, this );
\t\t\t\t//pGrenade->KeyValue( "velocity", vToss );'''
    assert s.count(anchor)==1
    s=s.replace(anchor,'''\t\t\t\tCGrenadeSpit *pGrenade = (CGrenadeSpit*)CreateNoSpawn( "grenade_spit", vSpitPos, vec3_angle, this );
\t\t\t\tif (!pGrenade) break;
\t\t\t\t//pGrenade->KeyValue( "velocity", vToss );''',1)
    s=s.replace('random->RandomFloat ( -100, -500 )','random->RandomFloat ( -500, -100 )')
    # The HL1-era classname no longer exists in this HL2 tree. Classify()
    # recognizes standard, fast, poison and black headcrab variants alike.
    old_to_new = {
        'FClassnameIs( GetEnemy(), "monster_headcrab" )':
            '(GetEnemy()->Classify() == CLASS_HEADCRAB)',
        'FClassnameIs ( pTarget, "monster_headcrab" )':
            '(pTarget && pTarget->Classify() == CLASS_HEADCRAB)',
        'FClassnameIs ( inputInfo.GetAttacker(), "monster_headcrab" )':
            '(inputInfo.GetAttacker() && inputInfo.GetAttacker()->Classify() == CLASS_HEADCRAB)',
    }
    for old, new in old_to_new.items():
        assert old in s, old
        s=s.replace(old,new)
    # The new enemy condition isn't guaranteed to carry a live pointer.
    old='m_fCanThreatDisplay && IRelationType( GetEnemy() ) == D_HT'
    assert s.count(old)==1
    s=s.replace(old,'m_fCanThreatDisplay && GetEnemy() && IRelationType( GetEnemy() ) == D_HT',1)
    if s.count(marker)!=1: raise RuntimeError('Bullsquid restoration marker missing')
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin-1'))
    print('Restored Bullsquid skill health/damage, tail whip, enemy and projectile guards')
else:
    expected=['sk_bullsquid_health", "70"','sk_bullsquid_dmg_bite", "20"',
              'sk_bullsquid_dmg_whip", "35"','if (!pGrenade) break;',
              'const float whipDamage = sk_bullsquid_dmg_whip.GetFloat();']
    for value in expected:
        if value not in s: raise RuntimeError('Incomplete previous Bullsquid patch: '+value)
    if s.count(marker)!=1: raise RuntimeError('Duplicate Bullsquid patch marker')
    print('Bullsquid source already patched')
