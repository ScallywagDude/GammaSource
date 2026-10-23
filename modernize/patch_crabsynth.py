#!/usr/bin/env python3
"""Restore an actual, visible-target-only Crab Synth charge hit.

Fixes the left/right schedule typo and prevents damaging targets behind the
NPC. The source is still an unfinished 2003 prototype: this patch does not
supply animations, models, sound scripts, or a validated navigation mesh.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
p = root / 'dlls/hl2_dll/npc_crabsynth.cpp'
raw = p.read_bytes()
crlf = b'\r\n' in raw
s = raw.decode('latin-1').replace('\r\n', '\n')
marker = 'HL2MODERN_CRABSYNTH_CHARGE_DAMAGE'
if marker not in s:
    include = '#include "AI_Memory.h"'
    if s.count(include) != 1:
        raise RuntimeError('Unexpected Crab Synth includes')
    s = s.replace(include, include + '\n#include "../../modernize/crabsynth_charge_policy.h"', 1)
    # A tunable damage amount, zero to disable damage for animation testing.
    anchor = 'LINK_ENTITY_TO_CLASS( npc_crabsynth, CCrabSynth );'
    if s.count(anchor) != 1:
        raise RuntimeError('Unexpected Crab Synth entity registration')
    s = s.replace(anchor,
        'ConVar sk_crabsynth_charge_dmg( "sk_crabsynth_charge_dmg", "35", FCVAR_NONE,\n'
        '    "Damage from a Crab Synth charge impact (0 disables damage)." );\n\n' + anchor, 1)
    anchor = 'if (IsUnreachable(GetEnemy()))'
    if s.count(anchor) != 1:
        raise RuntimeError('Missing original charge path guard')
    s = s.replace(anchor, 'if (!GetEnemy() || IsUnreachable(GetEnemy()))', 1)
    # The original continued to activate charge movement and complete a task
    # even after navigator SetGoal failed, masking FAIL_NO_ROUTE.
    failed_goal = (
        "\t\t\t\tRememberUnreachable(GetEnemy());\n"
        "\t\t\t\tTaskFail(FAIL_NO_ROUTE);\n"
        "\t\t\t}\n"
        "\t\t\tGetNavigator()->SetMovementActivity")
    if s.count(failed_goal) != 1:
        raise RuntimeError('Unexpected SetGoal failure branch')
    s = s.replace(failed_goal, failed_goal.replace(
        "\n\t\t\t}", "\n\t\t\t\treturn;\n\t\t\t}"), 1)
    start = s.index('\t\t\t\tfloat dot = DotProduct( forward, pEnemy->GetAbsOrigin() );')
    end = s.index('\n\t\t\t}', start)
    replacement = '''                // HL2MODERN_CRABSYNTH_CHARGE_DAMAGE
                // Evaluate the forward/lateral projections with unit-tested
                // geometry before applying any server-side damage.
                const Vector delta = pEnemy->WorldSpaceCenter() - WorldSpaceCenter();
                const float forwardDistance = DotProduct( forward, delta );
                const float lateralDistance = DotProduct( right, delta );
                const HL2ModernCrabCharge::Decision decision =
                    HL2ModernCrabCharge::Classify(forwardDistance, lateralDistance,
                        CHARGE_ATTACK_HIT, CHARGE_ATTACK_WIDTH, CHARGE_ATTACK_TRAMPLE);
                if (decision != HL2ModernCrabCharge::APPROACH)
                {
                    // Do not hit a dead or occluded enemy. End a missed charge
                    // rather than repeatedly dealing damage during RunTask.
                    if (decision == HL2ModernCrabCharge::MISS ||
                        !pEnemy->IsAlive() || !FVisible(pEnemy))
                    {
                        SetSchedule(SCHED_CRAB_CHARGE_STOP);
                    }
                    else
                    {
                        const float damage = sk_crabsynth_charge_dmg.GetFloat();
                        if (damage > 0.0f && damage <= 1000.0f)
                        {
                            CTakeDamageInfo info(this, this, damage, DMG_CRUSH);
                            CalculateMeleeDamageForce(&info, forward, pEnemy->WorldSpaceCenter());
                            pEnemy->TakeDamage(info);
                        }

                        if (decision == HL2ModernCrabCharge::HIT_LEFT)
                            SetSchedule(SCHED_CRAB_CHARGE_HITLEFT);
                        else if (decision == HL2ModernCrabCharge::HIT_RIGHT)
                            SetSchedule(SCHED_CRAB_CHARGE_HITRIGHT);
                        else
                            TaskComplete();  // Continue to TASK_CHARGE_HIT.
                    }
                }'''
    s = s[:start] + replacement + s[end:]
    if s.count(marker) != 1:
        raise RuntimeError('Crab Synth marker not installed')
    p.write_bytes((s.replace('\n', '\r\n') if crlf else s).encode('latin-1'))
    print('Crab Synth: repaired charge geometry, right animation and damage')
else:
    for assertion in ('sk_crabsynth_charge_dmg',
                      'HL2ModernCrabCharge::Classify',
                      'SCHED_CRAB_CHARGE_HITRIGHT',
                      '!GetEnemy() || IsUnreachable(GetEnemy())',
                      'TaskFail(FAIL_NO_ROUTE);\n\t\t\t\treturn;'):
        if assertion not in s:
            raise RuntimeError('Incomplete pre-existing Crab Synth patch: ' + assertion)
    if s.count(marker) != 1:
        raise RuntimeError('Duplicate Crab Synth patch')
    print('Crab Synth: already patched')
