# Alien Assassin and historical branch limits — 26 October 2026

## Distinct reconstructed Alien Assassin

`game/server/gamma/npc_alienassassin.cpp` adds `npc_alienassassin` and `npc_assassin_alien` to the HL2 server. This is explicitly reconstructed AI. It does not replace `npc_assassin`, which retains its recovered Combine/Female Assassin implementation and `fassassin.mdl`.

Recovered inputs:

- Original `models/Assassin.mdl` and its supplied `.phy`, rather than the Female Assassin's model. Both model and physics files are present in the mounted beta content.
- The model's `knife` / `ACT_RANGE_ATTACK1` animation emits event 2 at cycle 0.628571; `stab` / `ACT_MELEE_ATTACK1` emits event 3 at cycle 0.628571; `smoke` / `ACT_ASSASSIN_SMOKE` emits event 5 at cycle 0.285714. These events drive the respective attacks/effect at authored animation times.
- Original `npc/assassin/idle2.wav` and `tattle1.wav`, plus existing original Assassin footstep definitions. The sprite trail uses an existing beta material; no unrelated weapon model is labeled an authentic knife.

The reconstruction has ground navigation, normal enemy/squad participation under the Combine faction relationship class, claw attacks, thrown moving blade traces, smoke followed by a cover attempt, original model death handling, and saved cooldowns/effect handles. Smoke can also be requested through `DeploySmoke` while an enemy is present. Removal/death cleans up owned smoke and projectile removal cleans up its trail.

New tuning defaults, **not recovered historical balance**: `sk_alienassassin_health 120`, `sk_alienassassin_claw 20`, `sk_alienassassin_knife 15`; blade speed 1400, range 1024, throw cooldown 1.2 seconds, smoke cooldown eight seconds. Claw reach and cover policy are reconstruction choices. Existing player weapons and their repaired balance are unchanged.

Known limits:

- No matching original Alien Assassin AI implementation was found in the supplied 2003 source. This class must not be presented as restored authentic AI.
- Shock tripwires, pest creatures, rope running and the related unused model activities are **not implemented**. There is no silent substitution with Combine Assassin pistol behavior, Headcrabs or Antlions.
- No original thrown-knife mesh was found. A moving damage projectile and narrow sprite trail provide the implemented blade attack; a standalone knife mesh is still missing.
- Original maps using the ambiguous `npc_assassin` classname still select the existing Combine/Female Assassin. The explicit new classnames select this reconstruction.
- Native Windows compilation passed after adapting `trace_t` assignment to its protected-copy-constructor contract. Runtime gameplay still must be recorded by the coordinating agent; this document alone does not certify attacks, smoke, navigation, death or save/load.

Private probe: `gamma_phaseb_alienassassin_boot.cfg`, preserved under `verification/npc_scene_cfg/phaseb_alienassassin` and copied into `t_round3/cfg`. It tests real target health changes, the smoke animation/effect, close-range claws, save/load and death. It uses the nonteleporting observer and logs player placement. Run only in the isolated profile: the boot contains `unbindall`.

## TF2 prototype bugs

The original `npc_bug_builder`, `npc_bug_warrior` and `npc_bughole` sources are preserved under `legacy2003/dlls/tf2_dll`. Their model requirements were searched across the supplied Mega Build; all three are absent:

- `models/npcs/bugs/bug_builder.mdl`
- `models/npcs/bugs/bug_warrior.mdl`
- `models/npcs/bugs/bug_hole.mdl`

The Warrior source directly uses `CBaseTFPlayer::IsCamouflaged()` and `CBaseObject::IsSentrygun()`, neither part of this HL2 game mode. Bughole spawning also requires coordinated pool/patrol/child save data and its client counterpart `legacy2003/cl_dll/tf2_hud/c_maker_bughole.cpp`; the original server/client data-table names have different spelling and require review together. A source-only registration would create missing models and would not restore these species. They are left unregistered rather than disguised with another NPC's art. Exact source hashes and dependency notes are in the external `tf2_bug_port_barriers.json` proof manifest.

## HL1 monsters

The historical source audit inventories 40 HL1 named monsters/entities, with only two generic compatibility entities registered in the HL2 build. They are not 40 additional working HL2 NPCs.

The current `game/server/hl1/hl1_npc_*` implementations rely on `CHL1BaseNPC`, HL1-specific combat/weapon/projectile support, separate `Class_T` values such as `CLASS_HUMAN_MILITARY` and `CLASS_ALIEN_MILITARY`, HL1 relationships, skill configuration and corresponding content roots. Those class enums belong to the `HL1_DLL` branch rather than the active HL2 branch. The active private HL2 validation profile does not mount the complete `hl1` content root. Enabling these files wholesale would collide with reused entity/resource names and does not constitute a branch-aware port. Their main-game integration remains unfinished and must remain visible in the build's known-bug list.
