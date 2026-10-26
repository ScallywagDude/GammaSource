# GammaSource — 2003 NPC AI restoration, Round 3 (26 October 2026)

The **2003 beta builds and classes remain authoritative**. Newer APIs can supply scheduling, damage routing, networking, and navigation infrastructure, but must **not replace a beta enemy's intended identity, model, behavior or animations**. Same classname is never proof that variants share behavior.

## Concrete code fixes (source-level only)

- **2003 camouflage Combine Elite — `npc_combineelite`**: the original source initialized `sk_combine_elite_health` to zero, producing a nonviable enemy unless the config overrode it. Default is now 100 HP (per beta documentation), with a separately tunable kick and max health. Retains `models/Combine_Elite.mdl` and original `CNPC_CombineE` class; no retail class swap.
- **2003 Synth Combine Guard — `npc_combineguard`**: now defaults to positive health, initializes its effects pointers and tolerates unavailable optional attachment sprites. Replaced a legacy `TraceAttack(CBaseEntity*,...)` method (not invoked by the modern damage dispatcher) with `TraceAttack(const CTakeDamageInfo&, ..., CDmgAccumulator*)`. Preserves 2003 bodygroup armor absorption and hit effects. Regular damage now reaches the inherited NPC damage handler, instead of the old unconditional zero-damage return.
- **Synth Guard gun**: its original E3 demo attack traced and rendered a laser, but only inflicted one point on scripted bullseyes. The attack now dispatches actual configurable beam damage (`sk_combineguard_beam_damage`, default 30) to the traced recipient and retains the 2003 tracers, concussive impact and charging visual effects. This damage value is a **new gameplay tuning default**, not a historically verified original stat.
- **`skill.cfg`** records the four independent cut-unit settings separately from the later `npc_combine_s` elite/guard settings. Existing map entities were not renamed or removed.
- **No model or weapon substitution**: no beta NPC registration, Annabelle, `impulse 101`, classic loading UI, first-person hands, or deformation assets were altered.

### Important: these are not yet functional in-game certifications

This environment does **not** have the Windows compiler/runtime, original 16 GB staged installation, nor the full beta Mega Build content. Unit/static checks **cannot** prove live models, animations, AI planning, squadding, damage or weapon effects. An x64 Waf build and isolated spawn tests are required. Do **not** mark cut Combine restoration complete without them.

## Armor stability extension (integrated after the initial Combine repair)

- Guard knockdown no longer runs a potentially infinite random retry when its remaining armor pieces have already been destroyed. It counts valid intact armor and chooses a bounded random ordinal (C++98 helper `game/server/gamma/beta_guard_armor_safety.h`).
- Armor destruction is committed **even when** the currently mounted beta model lacks a numbered attachment or an optional armor-gib model. Missing optional art no longer traps the entity in a perpetual alive-armor state. If gib/model data is incomplete, the broken piece may be visually absent, but the gameplay state advances.
- Initialization and removal guard against models with fewer actual bodygroups. Glow sprites are only attached when the model exposes the requested attachment.
- Corrected Guard armor health decrement for positive fractional damage and oversized hits; invalid/nonpositive damage does not destroy armor. Intact armor still uses the original per-piece state and `AllArmorDestroyed()` condition.
- The cut `npc_combineguard` keeps its 2003 physics-stagger, topple, and helpless schedules. This is **source-level stabilization**, not a validated scene or model conversion.
- The wiki specifically documents that the original playable-leak Guard model was missing bodygroup and attachment data required by its AI; repair of an appropriate model from the Mega Build or historical model source remains necessary for full visual fidelity. [Combine Guard (Synth)](https://www.combineoverwiki.net/wiki/Combine_Guard_%28Synth%29).

**Mega Build provenance:** The exact Drive ID supplied by the user (`1ODwvLQVL9AKRl_VxD45R9wN1trHFpWxc`) is associated with a Mega Build 2024 patch that requires a Mega Build base installation. The archive contents could **not** be fetched or verified in this environment; no original Mega Build AI/assets are falsely claimed imported into this round. The Project Beta site separately lists Mega Build 2024 and MegaPatch 5. [Project Beta](https://hl2-beta.ru/?language=english).

**Remaining blockers:** Windows x64 Waf compilation, physical/laser damage gameplay, Guard defeat animations and hitgroups, actual model attachments/bodygroups and gib VTX/VVD, Conscript + turret separation, Assassin/Combot variants, and Cremator AI reconstruction. None is marked complete by the 15 regression tests.

## Reused beta names and missing reimaginings

| Entity | Historical variants needing distinction | State |
| --- | --- | --- |
| `npc_combineelite` | Beta camouflage Combine Elite | source-repaired; Windows compile and runtime unverified |
| `npc_combineguard` | Synth Combine Guard | source-repaired; Windows compile and runtime unverified |
| `npc_combine_s` | 2003 Combine Soldier | compiled classname; beta behavior not proven |
| `npc_combine` | 2003 Combine grunt/base AI | compiled classname; beta behavior not proven |
| `npc_assassin` | Alien Assassin and Combine Assassin | registered; model/abilities/variant runtime unverified |
| `npc_cscanner` | Combot and City Scanner | compiled classname; combot behavior not proven |
| `npc_cremator` | Cremator | reconstruction needed |
| `npc_conscript` | Conscript | disabled source |
| `npc_miniturret` | 2003 miniturret | disabled source |
| `npc_sentry` | 2003 sentry | source not build-listed |
| `npc_hydra` | Hydra | build registration only |
| `npc_bullsquid` | Beta Bullsquid | build registration only |
| `npc_houndeye` | Beta Houndeye | build registration only |
| `npc_spotlight` | Beta spotlight | build registration only |

More than one beta enemy may have used `npc_assassin`; 2003's Alien Assassin and Combine Assassin are **not** interchangeable just because their entity name matches. Similarly, the Combot reused `npc_cscanner` but had distinct gas-style behavior. The beta camo `npc_combineelite` must not silently become `npc_combine_s` using a later elite model. See [Alien Assassin](https://combineoverwiki.net/wiki/Alien_Assassin), [Combine Elite](https://combineoverwiki.net/wiki/Combine_Elite), and [Combot](https://combineoverwiki.net/wiki/Combot).

The Cremator's original AI **is not present** in the leaked source, so that enemy needs a clearly marked reconstruction from documented patrol, clean-up, attack, sound and relationship behavior. See [Cremator](https://combineoverwiki.net/wiki/Cremator).

## Remaining required implementation work

1. **Complete true 2003 soldier AI** for `npc_combine` and `npc_combine_s`, diffing original schedules, grenade and squad logic against the compatibility port. This round **does not** claim the current base AI has been replaced entirely.
2. **Port legacy Conscript code** that is still disabled by `#if 0`, including draw/holster, grenades and scripted speech where supported by original assets.
3. **Isolate legacy Sentry / Miniturret classes** from `npc_turret_ceiling` so every entity name has one registration and its correct beta model.
4. **Restore both beta Assassin variants** with separate selectable model/AI profiles, without mapping either to retail zombie behavior.
5. **Restore original Combot attack/skin logic** while keeping distinct map compatibility with later scanner maps; preserve original names.
6. **Reconstruct undocumented cut AI carefully** for Cremator and other conceptual enemies; state which behavior is speculative and which is evidenced by source, maps or wiki.
7. **Audit 2003 HL1 and TF2 prototype NPC branches** separately; do not collapse `monster_*`, `npc_bug_*`, or visually similar classes into unrelated HL2 entities.
8. **Import original Mega Build assets only from confirmed exact source**. The user's supplied Drive ID is `1ODwvLQVL9AKRl_VxD45R9wN1trHFpWxc`; availability/contents of the actual large archive have not been established in this runtime. Full assets are not bundled with this source ZIP.
9. Native Windows x64 build, then isolated map + NPC spawn, combat, navigation, animations, sound, ragdoll, save/load, damage, and multiplayer tests; record results per classname.
10. Complete the `zoo_item` sky/physgun/HEV runtime check and upper-left black-square rendering investigation as **separate** renderer work; source registration alone cannot fix those.

## Regression coverage

Run at repository root:

```bat
py -3 -m unittest verification.test_beta_combine_round3 verification.test_beta_npc_restoration verification.test_impulse101_weapon_filter -v
```

The tests check relevant source structure and game-data invariants. They do not replace compilation or gameplay tests.
