#!/usr/bin/env python3
"""Preserve sticky launcher ammunition when projectile creation fails.

Apply after the historical weapon restoration overlay. The original source
encoding and line endings are retained; rerunning the patch is a no-op.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "dlls/hl2_dll/weapon_stickylauncher.cpp"


def once(source, before, after):
    if after in source:
        return source
    if source.count(before) != 1:
        raise ValueError("sticky launcher patch target missing or ambiguous: " + before[:100])
    return source.replace(before, after, 1)


def restore(source):
    source = once(source,
                  "static void LaunchStickyBomb( CBaseCombatCharacter *pOwner, const Vector &origin, const Vector &direction );",
                  "static bool LaunchStickyBomb( CBaseCombatCharacter *pOwner, const Vector &origin, const Vector &direction );")
    source = once(source,
                  "\t\t\tWeaponSound( SINGLE_NPC );\n\t\t\tLaunchStickyBomb( pOperator, vecShootOrigin, vecShootDir );",
                  "\t\t\tif ( !LaunchStickyBomb( pOperator, vecShootOrigin, vecShootDir ) ) break;\n\t\t\tWeaponSound( SINGLE_NPC );")
    source = once(source,
                  "\tm_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();\n\n\tWeaponSound(SINGLE);",
                  "\tm_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();\n\n"
                  "\tVector vecSrc = pPlayer->Weapon_ShootPosition( );\n"
                  "\tVector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );\n"
                  "\t// HL2MODERN_STICKY_SPAWN: only a created projectile spends a round.\n"
                  "\tif ( !LaunchStickyBomb( pPlayer, vecSrc, vecAiming ) ) return;\n\n\tWeaponSound(SINGLE);")
    source = once(source,
                  "\tVector vecSrc = pPlayer->Weapon_ShootPosition( );\n"
                  "\tVector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );\n"
                  "\tm_iClip1 = m_iClip1 - 1;\n\n"
                  "\tLaunchStickyBomb( pPlayer, vecSrc, vecAiming );\n\tAddViewKick();",
                  "\tm_iClip1 = m_iClip1 - 1;\n\tAddViewKick();")
    return once(source,
                "void LaunchStickyBomb( CBaseCombatCharacter *pOwner, const Vector &origin, const Vector &direction )\n"
                "{\n"
                "\tCStickyBomb *pGrenade = (CStickyBomb *)CBaseEntity::Create( \"grenade_stickybomb\", origin, vec3_angle, pOwner );\n"
                "\tpGrenade->SetVelocity( direction * 1200, vec3_origin );\n"
                "}",
                "bool LaunchStickyBomb( CBaseCombatCharacter *pOwner, const Vector &origin, const Vector &direction )\n"
                "{\n"
                "\tCStickyBomb *pGrenade = (CStickyBomb *)CBaseEntity::Create( \"grenade_stickybomb\", origin, vec3_angle, pOwner );\n"
                "\tif ( !pGrenade ) return false;\n"
                "\tpGrenade->SetVelocity( direction * 1200, vec3_origin );\n"
                "\treturn true;\n"
                "}")


def main():
    path = ROOT / SOURCE
    original = path.read_bytes()
    crlf = b"\r\n" in original
    source = original.decode("latin1").replace("\r\n", "\n")
    updated = restore(source)
    if updated != source:
        path.write_bytes((updated.replace("\n", "\r\n") if crlf else updated).encode("latin1"))
    print("Sticky launcher spawn-failure handling applied; build and gameplay verification still required.")


if __name__ == "__main__":
    main()
