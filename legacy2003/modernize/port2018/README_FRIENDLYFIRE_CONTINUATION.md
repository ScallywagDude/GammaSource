# Friendly fire restoration — October 25, 2026

This source overlay restores default-on friendly damage to the Half-Life 2 single-player GammaSource game rules without changing weapon definitions, targeting behavior, the 2003 HUD, hands, animation models, loading dialog, or terrain-deformation code.

## Usage

The server declares `gamma_friendlyfire` with default **1** (`FCVAR_ARCHIVE | FCVAR_NOTIFY`). You can run `gamma_friendlyfire 0` to revert to normal 2018 friendly-damage immunity, or `gamma_friendlyfire 1` to reenable beta-style allied damage. This does not change `mp_friendlyfire` in multiplayer modes.

## Damage paths

- `game/shared/hl2/hl2_gamerules.cpp`: allied NPCs take damage from player-held physics objects even in cases previously blocked by `AllowDamage`. Other damage rules remain intact.
- `game/server/ai_basenpc.cpp`: allied NPCs no longer universally reject damage solely due to `bits_CAP_FRIENDLY_DMG_IMMUNE`, while `ai_block_damage` and base damage filters still apply.
- `game/server/player.cpp`: bullets / traces from friendly NPCs no longer get dropped solely because the NPC has `bits_CAP_NO_HIT_PLAYER`, while normal player-vs-player rules remain intact.
- `game/server/hl2/hl2_player.cpp`: friendly NPC damage passes the HL2 player's allied-NPC check, without bypassing player-proxy or base damage filters.

The default affects **damage**, not AI relationships. Friendly NPCs will not intentionally start shooting allies just because this toggle is enabled. Scripted scenes still run as authored.

## Build & runtime status

This patch is source code. It has been compared with the source in the October 25 handoff archive, and targeted source regression tests have been executed. The handoff's previous Windows Waf build happened *before* this patch. The current patch has **not** been compiled with MSVC or tested in a running Windows game in this environment.

On the Windows system, from the 2018 engine root, run `py -3 waf build -j16 -k`. The updated `server.dll` must be staged into the isolated executable profile before testing.

Test in an isolated profile (not the user's active running game): set `developer 1`, set `gamma_friendlyfire 1`, shoot a citizen, strike one with a physcannon-held object, and provoke incidental allied fire at the player. Repeat with `gamma_friendlyfire 0`. Check health damage and logs, not only decals; verify invulnerable scripted actors and `ai_block_damage` are still honored. Do not use `unbindall` or rewrite the delivered user's bindings. No focus-stealing automation.

Other pending handoff runtime checks remain: **normal** `logic_choreographed_scene` body-sequence playback on e3_lab/e3_hydra, and `exec gamma_zoo_visual_check` on zoo_item for sky, physgun beam, and beta HEV pickup. The 2003 classic green loading dialog on plain black remains the chosen loading UI; do not restore the older splash artwork.
