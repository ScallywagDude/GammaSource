#!/usr/bin/env python3
"""Make hopwire animation events spend ammunition only on created grenades."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEAPON = "dlls/hl2_dll/weapon_hopwire.cpp"
GRENADE = "dlls/hl2_dll/grenade_hopwire.cpp"


def once(source, before, after):
    if after in source:
        return source
    if source.count(before) != 1:
        raise ValueError("hopwire patch target missing or ambiguous: " + before[:100])
    return source.replace(before, after, 1)


def weapon(source):
    for name in ("ThrowGrenade", "RollGrenade", "LobGrenade"):
        source = once(source, "\tvoid\t" + name + "( CBasePlayer *pPlayer );",
                      "\tbool\t" + name + "( CBasePlayer *pPlayer );")
        source = once(source, "void CWeaponHopWire::" + name + "( CBasePlayer *pPlayer )",
                      "bool CWeaponHopWire::" + name + "( CBasePlayer *pPlayer )")
        source = once(source,
                      "\t\t\t" + name + "( pOwner );\n\t\t\tDecrementAmmo( pOwner );",
                      "\t\t\tif ( pOwner && pOwner->GetAmmoCount(m_iPrimaryAmmoType)>0 &&\n"
                      "\t\t\t\t " + name + "( pOwner ) ) DecrementAmmo( pOwner );")
    for call in (
        "HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GRENADE_TIMER )",
        "HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(200,random->RandomInt(-600,600),0), pPlayer, GRENADE_TIMER )",
        "HopWire_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, GRENADE_TIMER )",
    ):
        source = once(source, "\t" + call + ";", "\tif ( !" + call + " ) return false;")
    # Only the three throw helpers end in this redraw assignment.
    source = source.replace("\tm_bRedraw = true;\n}", "\tm_bRedraw = true;\n\treturn true;\n}")
    return source


def grenade(source):
    if 'HL2MODERN_HOPWIRE_COMBAT' in source:
        return source
    return once(source,
                '\tCGrenadeHopWire *pGrenade = (CGrenadeHopWire *) CBaseEntity::Create( "npc_grenade_hopwire", position, angles, pOwner );\n\t\n\tpGrenade->SetTimer( timer );',
                '\tCGrenadeHopWire *pGrenade = (CGrenadeHopWire *) CBaseEntity::Create( "npc_grenade_hopwire", position, angles, pOwner );\n'
                '\t// HL2MODERN_HOPWIRE_SPAWN: an exhausted entity factory cannot supply a grenade.\n'
                '\tif ( !pGrenade ) return NULL;\n\t\n\tpGrenade->SetTimer( timer );')


def patch(relative, change):
    path = ROOT / relative
    raw = path.read_bytes()
    crlf = b"\r\n" in raw
    source = raw.decode("latin1").replace("\r\n", "\n")
    updated = change(source)
    if source != updated:
        path.write_bytes((updated.replace("\n", "\r\n") if crlf else updated).encode("latin1"))


def main():
    patch(WEAPON, weapon)
    patch(GRENADE, grenade)
    print("Hopwire spawn-failure and animation-event guards applied; build and gameplay verification still required.")


if __name__ == "__main__":
    main()
