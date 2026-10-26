# Cremator reconstruction — 26 October 2026

`game/server/gamma/npc_cremator.cpp` adds `npc_cremator` to the HL2 server source list. This is reconstructed AI, not recovered 2003 NPC code. The supplied original trees contain the model, sound definitions, hull identity and AI relationship table, but no Cremator implementation.

## Recovered assets and contracts

- Uses the supplied `models/cremator.mdl`, with its original `ACT_IDLE`, `ACT_WALK`, `ACT_RUN`, `ACT_RUN_HURT`, `ACT_SMALL_FLINCH` and `ACT_RANGE_ATTACK1` sequences. No retail NPC/model is substituted.
- Uses `HULL_MEDIUM`, explicitly identified as the Cremator hull in the supplied `ai_hull.h`.
- Original attachment `0` is on the `muzzle` bone; original attachment `1` is on `Bip01 Head`. Attachment lookup uses their authored names, with a safe world-space fallback when an explicitly overridden model lacks them.
- Uses the existing `CLASS_CREMATOR` relationship table recovered from 2003, rather than assigning another enemy's class.
- Copies `hl2/scripts/game_sounds_npc_cremator.txt` unchanged into the private live overlay and adds it once to the sounds manifest. All 12 wave references resolve to mounted files byte-identical to the Mega Build originals.
- The full Cremator has no supplied `.phy`; its separate `cremator_body` and `cremator_head` have original physics companions. Death creates those original assets instead of relying on a complete-body client ragdoll that lacks physics data.

## Reconstructed behavior and defaults

Patrols using normal ground navigation, participates in squads and choreographed NPC states, turns toward enemies, and uses the supplied range-attack animation while firing a green immolator beam. Damage requires a real trace contact and respects intervening solids. It periodically finds server corpses/ragdolls, routes toward them and dissolves them only after continuous beam contact. Failure to route releases the cleanup goal.

These values are new tuning defaults, **not historically established beta balance**:

| ConVar | Default | Meaning |
| --- | ---: | --- |
| `sk_cremator_health` | 300 | Health unless a map supplies positive health |
| `sk_cremator_dps` | 40 | Immolator damage per second |
| `sk_cremator_range` | 600 | Immolator range in world units |
| `sk_cremator_cleanup_time` | 2 | Continuous clear corpse contact before dissolution |

The attack lasts two seconds, then has a one-second cooldown. Patrol, cleanup policy, attack timing, damage, optional death pickup, limb selection for the separated corpse, and footstep phase fallback are reconstruction choices. Existing player weapon code and repaired weapon values are unchanged.

Map keyvalues are `StartEnabled`, `patrol`, `cleanupcorpses`, and `dropimmolator` (all default true). Inputs are `Enable`, `Disable`, and `SetCleanupTarget <dead targetname>`. Outputs are `OnImmolatorStarted`, `OnImmolatorStopped`, and `OnCorpseCleaned`. The existing NPC health, relationship, squad, scene and death interfaces remain available. `SF_NPC_NO_WEAPON_DROP` suppresses the optional existing `weapon_immolator` pickup.

Beam handles, target handles, timers, attack state and map settings are saved. Disable, schedule interruption, death and entity removal remove the owned beam and stop the breathing loop. Activate restarts the one breathing channel after a load.

## Validation and limits

The coordinating agent compiled the source natively and ran the combat probe. `cremator_combat_runtime.log` records the actual beta model (76 bones / 13 sequences), advancing `firebreathing2` range attacks, visible green muzzle beam, original speech/death sound starts, and real bullseye damage from 100,000 to 99,295 health. Save/load resumed attacks; Disable held target health at 99,454 until Enable. Player position stayed unchanged. This proves those sampled paths, **not every Cremator behavior**. Asset provenance checks and native fixtures are in `cremator_asset_and_probe_manifest.json` under the external NPC proof folder.

That first run exposed a death-head compatibility defect: the legacy head has physics but no modern propdata, so ordinary `prop_physics` deleted it. The source now creates the head through `prop_physics_override`; final rebuild/native verification of that fix remains separate. The unsupported `ent_find` diagnostic calls in the initial fixture were replaced with existing `gamma_actorinfo` probes.

Private test boots copied into `t_round3/cfg` and preserved in `verification/npc_scene_cfg/phaseb_cremator`:

- `gamma_phaseb_cremator_combat_boot.cfg`: normal enemy selection and actual bullseye damage, beam capture, save/load, Disable/Enable, death and body/head/pickup inspection.
- `gamma_phaseb_cremator_cleanup_boot.cfg`: original server ragdoll, cleanup targeting, navigation and continuous-contact dissolve. The corpse's actual engine placement is logged; route failures remain test failures.

Run only in the isolated profile. The boots contain `unbindall`; they must never run in the player's normal game profile. They use the nonteleporting observer camera and record player placement.

Outstanding proof requirements: obstruction scenarios, full corpse/head physics after the compatibility fix, successful corpse route/cleanup, interruption without leaked beam entities, and useful long-term patrol behavior on original maps. Server-side ragdolls and dead NPC entities are implemented cleanup targets; client-only ragdolls are not discoverable by server AI and are not falsely claimed supported.

The unrelated Alien Assassin still has no recovered AI implementation in the supplied source. Its model alone does not certify a faithful independent NPC variant.
