#!/usr/bin/env python3
"""Emit a simple prototype Half-Life 2 multiplayer arena Hammer VMF source.

This is intentionally a map SOURCE, not a compiled BSP, and it uses the
existing multiplayer spawn/weapon classes; it does not implement networking.
"""
from pathlib import Path
from generate_npc_testmaps import ROOT, entity, vmf

OUT = ROOT / 'modernize' / 'maps' / 'multiplayer'
SPAWNS = [
    (-384, -256, 24), (-384, 0, 24), (-384, 256, 24),
    (384, -256, 24), (384, 0, 24), (384, 256, 24),
    (0, -384, 24), (0, 384, 24),
]
PICKUPS = [
    ('weapon_crowbar', (-256, -256, 24)),
    ('weapon_pistol', (256, -256, 24)),
    ('weapon_smg1', (-256, 256, 24)),
    ('item_healthkit', (256, 256, 24)),
    ('item_battery', (0, 0, 24)),
]


def build_map():
    # Reuse the exact same sealed geometric level, but replace the one cut NPC.
    s = vmf('npc_houndeye')
    npc = entity(102, 'npc_houndeye', (192, 0, 24),
                 angles='0 180 0', targetname='prototype_test_npc')
    assert s.count(npc) == 1
    spawns = [entity(200+i, 'info_player_deathmatch', xyz, angles='0 0 0')
              for i, xyz in enumerate(SPAWNS)]
    pickups = [entity(300+i, classname, xyz) for i, (classname, xyz) in enumerate(PICKUPS)]
    return s.replace(npc, '\n'.join(spawns + pickups))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'mp_modern_test.vmf').write_text(build_map(), encoding='utf-8')
    cfg = ROOT / 'modernize' / 'configs'
    cfg.mkdir(exist_ok=True)
    (cfg / 'server_deathmatch.cfg').write_text('''// HL2 2003 modernization: experimental dedicated server template.
// Copy into your mod cfg/ directory; verify engine/gamerules startup first.
// deathmatch must be enabled when starting the level/game server.
hostname "HL2 2003 Modernization - Deathmatch"
sv_lan 1
mp_teamplay 0
mp_timelimit 20
mp_fraglimit 30
mp_forcerespawn 1
mp_fair_spawns 1
// Two-second protection against enemy damage, cancelled on attack.
mp_spawn_protection_seconds 2
mp_friendlyfire 0
mp_chat_cooldown 0.65
// Vote commands: callvote map <map>, vote yes/no, votestatus
mp_mapvote 1
mp_mapvote_duration 30
mp_mapvote_cooldown 120
mapcyclefile "mapcycle_modern.txt"
''', encoding='utf-8')
    (cfg / 'server_teamdeathmatch.cfg').write_text('''// Experimental team deathmatch mode using original teamplay_gamerules.
// Requires a compatible map with team/spawn definitions.
hostname "HL2 2003 Modernization - Team Deathmatch"
sv_lan 1
mp_teamplay 1
// Limit player-initiated team changes that would imbalance current teams.
mp_team_balance 1
mp_timelimit 20
mp_fraglimit 50
mp_forcerespawn 1
mp_fair_spawns 1
// Two-second protection against enemy damage, cancelled on attack.
mp_spawn_protection_seconds 2
mp_friendlyfire 0
mp_chat_cooldown 0.65
// Vote commands: callvote map <map>, vote yes/no, votestatus
mp_mapvote 1
mp_mapvote_duration 30
mp_mapvote_cooldown 120
mapcyclefile "mapcycle_modern.txt"
''', encoding='utf-8')
    (cfg / 'mapcycle_modern.txt').write_text('mp_modern_test\n', encoding='utf-8')
    (OUT / 'README.md').write_text('''# Experimental multiplayer map sources

`mp_modern_test.vmf` provides eight `info_player_deathmatch` spawns and five
ordinary pickups. Compile it to BSP with the matching 2003-era map tools and
copy the BSP to the game `maps/` folder. This is not a functional multiplayer
mode by itself. Multiplayer game rules exist in the original source tree,
but builds, client/server synchronization, and game behavior are untested.

Copy a server preset from `modernize/configs/` into your mod `cfg/` folder;
`sv_lan 1` is deliberately used for first-pass local testing. To enable true
multiplayer, the executable needs to start with multiplayer/deathmatch game
state, load the `multiplay_gamerules` class, and connect multiple clients.
No public server exposure is recommended until security and stability testing.
The example cfg enables optional player map votes: callvote map mp_modern_test,
vote yes/no, and votestatus; public servers default to mp_mapvote 0.

''', encoding='utf-8')
    print('Generated arena VMF and two multiplayer cfg templates')


if __name__ == '__main__':
    main()
