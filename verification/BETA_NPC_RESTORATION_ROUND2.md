# GammaSource — Beta NPC restoration, Round 2 (26 October 2026)

**Goal: prefer original 2003 beta NPC behavior and models when a name is recycled.** Do not swap a beta entity for its retail counterpart just because they use the same classname. All gameplay validation remains pending until a native Windows x64 rebuild and an in-game test.

## Source wiring completed this round

The following existing Source 2003 beta-based implementations are now directly listed in the HL2 server Waf/VPC source set:

| Entity | Active beta-based source | Notes |
| --- | --- | --- |
| `npc_assassin` | `game/server/hl2/npc_assassin.cpp` | Original combat/flip AI; needs native compilation, assets and spawn tests |
| `npc_bullsquid` | `game/server/hl2/npc_bullsquid.cpp` | Original AI; also registered its `grenade_spit` attack projectile source |
| `npc_houndeye` | `game/server/hl2/npc_houndeye.cpp` | Original squad/sonic behavior; depends on existing `energy_wave` implementation |
| `npc_hydra` | `game/server/hl2/npc_hydra.cpp` | Includes impaling helper. **Paired** `game/client/hl2/c_npc_hydra.cpp` now listed in HL2 client target for networked chain animation |
| `npc_spotlight` | `game/server/hl2/npc_spotlight.cpp` | Original spotlight NPC and beam behavior; needs visual and AI testing |

These source files were already present in the unified tree and remain close adaptations of the historical 2003 implementation. This change restores **build registration**, not verified gameplay functionality. The sources and compile commands are not a substitute for loading real maps and models.

## Larger audit — more than the original seven registrations

Two separate scripts now distinguish an entity's old classname, actual active implementation, registration under `#if 0`, and the specific files included by the HL2 Waf/VPC build:

- `verification/audit_beta_npc_class_registration.py` and `verification/beta_npc_class_audit.json` — 75 historical HL2 `npc_` classnames, **72 with an effective registration in a file listed by the build**. This is not a count of functional NPCs.
- `verification/audit_beta_npc_variants.py`, `verification/beta_npc_variant_audit.json`, and `verification/BETA_NPC_VARIANT_AUDIT.md` — source provenance and approximate code similarity for each reused classname, plus other 2003 branches. This identifies 3 historical TF2 `npc_bug_*` prototype entities and 40 named HL1 `monster_*`/`npc_*` registrations. Their different game rules and asset dependencies still need a separate port to GammaSource's HL2 game mode.

The earlier filename-only count of 68/75 missed both newly compiled beta NPCs and disabled sources. The current effective-registration count is **72/75**, not a runtime restoration or completeness percentage.

### Next exact blockers

1. `npc_conscript`: the current `game/server/gamma/npc_conscript.cpp` is in the VPC **but nearly all implementation code is disabled with `#if 0`**. Remove the compile blocker only after adapting the beta AI to the current classes; listing the file does nothing.
2. `npc_miniturret`: original `game/server/hl2/npc_turret.cpp` registration itself sits inside `#if 0`.
3. `npc_sentry`: its beta implementation is also in `npc_turret.cpp`, which is not build-listed. That entire file **also registers `npc_turret_ceiling`**, already independently provided by `game/server/hl2/npc_turret_ceiling.cpp`; enabling the file wholesale would cause duplicate registration. Parts of the old file contain syntax/porting defects. Split and port the old turret base and variants before introducing a build entry.
4. Recycled classnames requiring careful beta-specific review include `npc_antlionmaker`, `npc_metropolice`, `npc_alyx`, `npc_combine`, `npc_citizen`, `npc_vortigaunt`, `npc_strider`, `npc_barney`, `npc_helicopter` and `npc_turret_ceiling`. Code similarity alone cannot prove whether differences are required portability work or unwanted retail behavior.
5. The three TF2 cut `npc_bug_builder`, `npc_bug_warrior`, and `npc_bughole` are separately inventoried, not overwritten by HL2/HL1 monsters that have superficially similar names.
6. Verify each restored NPC's models, skins, attachments, animations, sounds, relationships, map placement and scripted sequences. The complete Mega Build assets are **not** present in this handoff. Use the user-specified link `https://drive.google.com/file/d/1ODwvLQVL9AKRl_VxD45R9wN1trHFpWxc` for the nominated update source, and inspect whether a separate base package is also needed. Do not replace it silently.

## Validation

The focused registration and prior Annabelle/`impulse 101` tests pass. They check source wiring, the Hydra client/server send table pair, old beta source similarity, disabled legacy code, duplicate turret avoidance and retained weapon filtering. Run at repo root:

```
py -3 -m unittest verification.test_beta_npc_restoration verification.test_impulse101_weapon_filter -v
py -3 verification/audit_beta_npc_class_registration.py
py -3 verification/audit_beta_npc_variants.py
```

**Not verified:** full MSVC/x64 compilation of newly enabled NPC code, runtime spawning, real asset availability, gameplay animations, scripted scenes, or Mega Build import. The old compiled executable does **not** contain these new source registrations until rebuilt.

### Safe Windows smoke-test checklist (after successful build and staging)

Use an isolated test profile. Spawn/inspect `npc_assassin`, `npc_bullsquid`, `npc_houndeye`, `npc_hydra`, and `npc_spotlight` one by one. Confirm full model and material loading, AI relationships, attack behavior, sounds, navigation, deaths/ragdolls, save/load, and stability. Test Hydra's chain with the client DLL from the same build. Check Bullsquid spits and Houndeye sound waves, not just that the classname appears.

## Preservation

No existing beta weapon, Annabelle label, `impulse 101` exclusions, HUD, classic green loading dialog on black, maps, hands, player movement, NPC assets, or deformable terrain was changed. Both turret variants remain source-only until a collision-free beta port is ready.
