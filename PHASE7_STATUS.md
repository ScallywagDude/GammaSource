# Phase 7 — Weapon restoration

**Latest GitHub source milestone: v25. Status: active, not complete or gameplay-verified.** This is the single living Phase 7 status file. Finish Phase 7 first, then Phase 3 (Vulkan/OpenGL/DX11/DX12 rendering API modernization, shaders and lighting), then continue the remaining phases.

## Current branch inventory

| Branch | Registered entries | Extra `impulse 101` grants | Uncompiled grant candidates excluded | Fully verified |
| --- | ---: | ---: | ---: | ---: |
| HL2 | 32 | 21 | 1 | 0 |
| HL1 | 15 | 15 | 0 | 0 |
| Counter-Strike | 28 | 25 | 0 | 0 |
| TF2 | 38 | 24 | 6 | 0 |
| **Total** | **113** | **85** | **7** | **0** |

These are current source/project inventory counts, not successful game builds. The 113 entries include 13 grant exclusions for bases, internal entities and unfinished source. There are 100 nonexcluded source candidates; six TF2 candidates are absent from the active server project, leaving 94 project-included candidates (HL2 30, HL1 15, CS 25, TF2 24). A project-included source file is not proof that it compiles or works. The HL2 uncompiled candidate `weapon_brickbat` has only a commented registration and is outside the 32 active entries. Extra grants exclude each branch's original cheat loadout.

## Latest source changes

**v25**

- Sticky launcher: a failed grenade factory no longer dereferences null, spends ammunition, or produces successful firing effects. Player and NPC paths share the success result.
- Hopwire: failed grenade creation is safe; throw/roll/lob animation events require a player owner and positive ammunition, and consume ammunition only after successful creation. Successful trajectories, timer, redraw and final-ammo removal are retained. Its unfinished combat behavior remains a blocker below.
- TF2 repair gun: injured vehicle passengers are prioritized over healthy passengers; both passenger orders are tested using the actual source method.
- TF2 grants: `weapon_grenade_rocket` is a `CBaseAnimating` projectile, not an equipable weapon. It remains registered for its launcher but is excluded from `impulse 101`. Both historical grant generators use the authoritative exclusions so replay cannot add it back.
- Legacy HUD auditing: bare material names in `640`/`320` sprite blocks resolve under `sprites/`, matching the client. Direct/explicit paths retain their meaning and traversal stays rejected.
- Completion gate: checks individual Debug/Release build rows, aggregates, source paths, exclusions, grants, asset status/issues and explicit boolean evidence. Missing/contradictory reports cannot establish completion; a source inventory alone cannot establish a Windows build.
- Windows prerequisites: repaired legacy string/CRT compilation, preserved vstdlib's static runtime choice, linked the matching fresh tier0 import library, and added the modern `_msize_base` entry through the same allocator. Existing string API signatures are retained. Explicit VC6 `/ML` switches map to the supported static CRT, including the generated engine configurations.
- Every source repair has a repeatable overlay; targeted C++98 tests execute actual weapon methods with controlled factories and targets. The Windows studio replay test uses its running Python interpreter.

**Validation: 217 tests run, 216 passed, 1 skipped, 0 failures.** All portable C++98 checks ran with the installed compiler. Reapplying the complete overlay left 7,072 protected source/project/tool files byte-for-byte unchanged. Structural validation passes for 153 projects and 348 configurations. The [recorded check results](modernize/reports/phase7_checks.json) distinguish passing tests from the failed full builds. No runtime verification flags are set by this milestone.

**v24 (historical result)**

- Layered weapon auditor resolves Source `Patch` materials through `include`, `replace`, and `insert`, recording the search layer for inherited textures.
- Detects malformed/cyclic/unsafe material includes and excessive inheritance depth.
- Locates VTX companions independently across ordered asset layers.
- Historical validation: 184 tests run, 183 passed, 1 skipped, 0 failed; overlay reapplication matched 8,267 source files from v23.

**v23 (historical result)**

- Introduced per-branch layered asset auditing for models, scripts, HUD materials, VTF dimensions and direct/640/320 legacy sprite layouts.
- Added safe weapon model-path checks rejecting `/.mdl` as an empty basename.
- Historical validation: 180 tests run, 179 passed, 1 skipped, 0 failed; replay reproduced 8,264 local source files from v22.

## Build and asset evidence

The ordered Windows x86 Release build passes its first five steps: `tier0`, `vstdlib`, `unitlib`, `vtf`, and `shaderlib`. Step 6, `stdshader_dbg`, fails because the bundled legacy Perl runtime cannot start on this Windows installation (Illegal System DLL Relocation; MSBuild custom-build exit 9020). An isolated DLL rebasing experiment also fails; repository tool binaries and system settings are unchanged. This is not a successful engine build.

Debug `tier0` builds successfully. Debug `vstdlib` compiles but fails to link because its legacy allocator overrides conflict with the modern debug CRT. No forced linking or substituted allocator was used.

A direct `Release HL2` server build was attempted independently of the shader step. It fails on legacy member-function pointer syntax, network-variable `offsetof` expressions and other compiler compatibility errors. These must be resolved before the changed weapons can be tested in a new game DLL. Existing LeakNet/alpha runtime binaries cannot validate these source changes.

Matching asset directories were found locally for all four branches. A read-only layered audit reports:

| Branch | Metadata candidates | Registered entries | Gameplay verified |
| --- | ---: | ---: | ---: |
| HL2 | 32 | 32 | 0 |
| HL1 | 15 | 15 | 0 |
| Counter-Strike | 24 | 28 | 0 |
| TF2 | 25 | 38 | 0 |

These totals include internal entries and are not counts of restored weapons. CS flags include three excluded bases and Scout's absent ordinary crosshair/autoaim entries, with zoom reticles present. TF2 has missing prototype scripts, a missing harpoon script, and an MDL35 rocket viewmodel requiring conversion/layout validation. Its included README explicitly says weapon models were replaced with HL2 models; correct TF2 weapon identity is therefore not established. HL2/HL1's referenced models are MDL37 with matching VTX6 companions; header checks do not prove the full loader layout, animation compatibility, materials or correct appearance. Local game paths/assets are not published.

## Remaining completion blockers

- Complete the Windows compiler/toolchain migration and successful per-branch game builds; resolve debug allocator linkage and the shader tool runtime.
- Implement and verify unfinished weapon behavior. Hopwire `CombatThink` still has an unfinished combat/damage TODO; manhack attacks remain empty and excluded from grants. Exclusion does not mean restoration is complete.
- Resolve the six uncompiled TF2 prototypes (`weapon_combat_chargeableplasma`, `weapon_infiltrator`, `weapon_laserdesignator`, `weapon_laserrifle`, `weapon_mortar`, `weapon_pistols`) with their dependencies, prediction/network behavior and assets; enabling project inputs alone is insufficient.
- Verify each actual gameplay weapon's script, viewmodel, world model, companion layout, attachment points, animations, hand position, materials, UV/texture scale, HUD art identity and crosshair behavior using correct branch content.
- Test pickups, both attacks, reload, ammunition, inventory limits, repeated/full-inventory `impulse 101`, save/load, death/drop and multiplayer behavior in rebuilt games. Record failures and rerun affected tests after fixes.

## Reproducing checks

With Python and a usable C++ compiler in the developer environment:

```sh
python -m unittest discover -s modernize/tests -v
python modernize/validate.py
python phase7_manifest_check.py .
```

The gate currently reports valid but incomplete evidence and exits **1**, with **0/113 registered entries verified**. Exit 0 requires complete reported build/asset/runtime evidence; exit 2 denotes corrupt or inconsistent input. A passing report still requires trustworthy gameplay evidence; the validator does not launch a game.

For actual Windows builds, use an x86 Visual Studio developer environment:

```sh
python modernize/build_driver.py Release
python modernize/build_driver.py Debug
```

Keep this status file and the main README updated in place with every published code milestone. Git commits preserve history; do not create `PHASE7_Vxx.md` reports.
