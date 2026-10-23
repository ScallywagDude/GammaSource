# NPC, material and shader implementation evidence — v18

This report distinguishes source repairs from restored visual content. The supplied v17 archive contains no NPC MDL, QC, SMD, VMT, VTF or WAV content. Utility TGA samples under `utils/viewbump` provide no recoverable NPC geometry, rig or animation. No installed game assets were copied into this release.

## Implemented source changes

- `dlls/hl2_dll/npc_assassin.cpp`: level precaching for the existing red-eye glow and blue trail; create eye effects only when the named Eye attachment exists; skip pistol fire when the intended left/right muzzle transform is unavailable.
- `dlls/hl2_dll/npc_bullsquid.cpp`: require the intended Mouth attachment before calculating a spit projectile trajectory.
- `dlls/hl2_dll/npc_mortarsynth.cpp`: skip an unavailable energy-gun attachment rather than using an invalid transform.
- `dlls/hl2_dll/npc_combineguard.cpp`: reject aiming calculations when the muzzle index or transform is unavailable.
- `dlls/hl2_dll/npc_lightstalk.cpp`: guard failed glow-sprite allocation. Existing animation and fade logic remains active.
- `dlls/hl2_dll/npc_launcher.cpp`: avoid precaching empty map-supplied projectile/sound names and skip firing without MissileModel. Its original invisible player-model dummy is explicitly identified as a dummy, not a restored enemy appearance. Hammer now exposes the actual MissileModel/LaunchSound/FlySound/StartOn fields.
- `materialsystem/stdshaders/WaterCheap_ps20.fxc`: bound eye-vector normalization in both NV3X and standard BLEND permutations and clamp the Fresnel dot to prevent negative reflection from quantized normal overshoot. The distance floor is 0.01 world units; squared floor 0.0001 is representable in half precision. Normal viewing distances preserve the prior computation.

The idempotent source overlays are `modernize/patch_npc_assets.py` and `modernize/patch_material_assets.py`. They preserve source encodings and line endings and refuse an unexpected patch site.

## Inventory and Hammer integration

The source inventory covers 21 cut/experimental classes; 20 have unconditional source registrations and Debug HL2 project inputs. Conscript remains behind its original whole-file `#if 0`; enabling the obsolete implementation would require an AI/API port, not simply an asset reference change. Registrations are not newly asserted to work in game.

`generate_cut_npcs.py` now removes comments, resolves string-valued model macros, distinguishes disabled/conditional registration, records all project configurations, follows verified Roller model inheritance and records direct effect, sound, attachment, sequence and activity dependencies. This repairs the old Combine Guard inventory selecting a commented metal-gib model and the old missing model entries for Ichthyosaur/Light Stalk. Default Roller Bull uses the Roller model, with its alternate spikes retained as a dependency.

`hl2_2003_cut_npcs.fgd` provides 20 experimental Hammer definitions with the source model displayed when installed. The invisible Launcher uses a point box. Twenty VMF source test rooms are supplied; these are not compiled maps and provide neither missing content nor runtime verification. Aquatic/complex AI and combat require appropriately authored runtime scenarios.

| Classname | Primary source model | Registered / HL2 input | External file / case | Model format |
|---|---|---|---|---|
| `npc_assassin` | `models/fassassin.mdl` | True / True | found_unpacked / exact | 37 |
| `npc_bullsquid` | `models/bullsquid.mdl` | True / True | found_unpacked / exact | 37 |
| `npc_houndeye` | `models/houndeye.mdl` | True / True | found_unpacked / exact | 37 |
| `npc_hydra` | `models/Hydra.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_crabsynth` | `models/synth.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_mortarsynth` | `models/mortarsynth.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_combineguard` | `models/combine_guard.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_conscript` | `models/conscript.mdl` | False / True | found_unpacked / case_mismatch | 37 |
| `npc_ichthyosaur` | `models/ichthyosaur.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_lightstalk` | `models/lightstalk.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_rollerbull` | `models/roller.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_missiledefense` | `models/missile_defense.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_roller` | `models/roller.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_rollerbuddy` | `models/roller.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_rollerdozer` | `models/roller.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_rollerturret` | `models/roller.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_sscanner` | `models/shield_scanner.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_wscanner` | `models/wscanner_single.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_combineelite` | `models/Combine_Elite.mdl` | True / True | found_unpacked / exact | 37 |
| `npc_odell` | `models/odell.mdl` | True / True | found_unpacked / case_mismatch | 37 |
| `npc_launcher` | `models/player.mdl` | True / True | found_unpacked / case_mismatch | 37 |

## Actual asset results and missing content

The source-only audit finds 0 unpacked models. Therefore every primary and secondary NPC model remains missing from the distributable source package. The external audit scans 3,146 distinct MDL paths in the developer-supplied `hl2_2003` and common `hl2` directories, read-only. All 21 referenced primary paths exist there (including the Launcher dummy), but all 21 MDLs report version 37. The supplied `public/studio.h` specifies version 36. These installed models are reported as unsupported-version metadata, not verified compatible models. Seventeen primary paths differ in case. The files are evidence of external availability only.

The bounded `source_asset_formats.py` reader decodes only the supplied 32-bit versions 35/36, using the source version-conversion evidence for 35. It checks declared lengths, signed counts, table spans, strings and attachment bone indices. It reports newer versions without guessing their layout. Version 35/36 vertices are embedded; a missing retail-style VVD is not itself a fault. Metadata success still does not verify mesh topology, UV alignment, weight painting, collision fitting, animation quality or effects.

Restoration requires legally supplied compatible model geometry/rig/animations, matching VTX files and referenced materials/textures; collision PHY where the NPC needs it; the listed audio/event definitions; and any projectile/effect dependencies. There is no sufficient NPC source material here for an honest reconstruction of the original rigs or appearances. Nothing in this release substitutes an unrelated enemy model.

The exact static requirements follow. Sound events need their original sound-script definitions and referenced WAVs. Numeric attachments, animation-event payloads, dynamic assets and dependencies inherited beyond the noted models require runtime/model-authoring review; the inventory does not pretend those are fully resolved.

### npc_assassin

- Source: `dlls/hl2_dll/npc_assassin.cpp`.
- Model files: `models/fassassin.mdl`.
- Effect materials: `sprites/redglow1.vmt`, `sprites/bluelaser1.vmt`.
- Named attachments: `Eye`, `LeftMuzzle`, `RightMuzzle`.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_ASSASSIN_FLIP_BACK`, `ACT_ASSASSIN_FLIP_FORWARD`, `ACT_ASSASSIN_FLIP_LEFT`, `ACT_ASSASSIN_FLIP_RIGHT`, `ACT_ASSASSIN_PERCH`, `ACT_RANGE_ATTACK1`, `ACT_RUN`, `ACT_TURN_LEFT`, `ACT_TURN_RIGHT`, `ACT_WALK`.
- Direct sound files: `player/pl_shoot1.wav`, `player/pl_shoot2.wav`, `player/pl_step1.wav`, `player/pl_step2.wav`, `player/pl_step3.wav`, `player/pl_step4.wav`.
- Sound-script events: `Assassin.AttackMiss`, `Zombie.AttackHit`.

### npc_bullsquid

- Source: `dlls/hl2_dll/npc_bullsquid.cpp`.
- Model files: `models/bullsquid.mdl`.
- Effect materials: `sprites/greenspit1.vmt`.
- Named attachments: `Mouth`.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_HOP`, `ACT_IDLE`, `ACT_RANGE_ATTACK1`, `ACT_RUN`, `ACT_SQUID_DETECT_SCENT`, `ACT_SQUID_EAT`, `ACT_SQUID_EXCITED`, `ACT_SQUID_INSPECT_FLOOR`, `ACT_WALK`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_Bullsquid.Alert`, `NPC_Bullsquid.Attack1`, `NPC_Bullsquid.Death`, `NPC_Bullsquid.Growl`, `NPC_Bullsquid.Idle`, `NPC_Bullsquid.Pain`, `NPC_Bullsquid.TailWhip`.

### npc_houndeye

- Source: `dlls/hl2_dll/npc_houndeye.cpp`.
- Model files: `models/houndeye.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_CROUCHIDLE`, `ACT_HOUND_GUARD`, `ACT_IDLE`, `ACT_IDLE_ANGRY`, `ACT_LEAP`, `ACT_RANGE_ATTACK1`, `ACT_RUN`, `ACT_TURN_LEFT`, `ACT_TURN_RIGHT`, `ACT_WALK`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_Houndeye.Alert`, `NPC_Houndeye.Anger1`, `NPC_Houndeye.Anger2`, `NPC_Houndeye.Die`, `NPC_Houndeye.GroupAttack`, `NPC_Houndeye.GroupFollow`, `NPC_Houndeye.Pain`, `NPC_Houndeye.Retreat`, `NPC_Houndeye.SonicAttack`, `NPC_Houndeye.SpeakSentence`, `NPC_Houndeye.WarmUp`, `NPC_Houndeye.Warn`.

### npc_hydra

- Source: `dlls/hl2_dll/npc_hydra.cpp`.
- Model files: `models/Hydra.mdl`, `models/props_junk/cardboard_box001a.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_180_LEFT`, `ACT_HYDRA_COWER`, `ACT_HYDRA_STAB`, `ACT_TURN_LEFT`, `ACT_TURN_RIGHT`.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_crabsynth

- Source: `dlls/hl2_dll/npc_crabsynth.cpp`.
- Model files: `models/synth.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_BODY_THROW`, `ACT_CHARGE_END`, `ACT_CHARGE_HITLEFT`, `ACT_CHARGE_HITRIGHT`, `ACT_CHARGE_LOOP`, `ACT_CHARGE_START`, `ACT_CHARGE_START_MOVE`.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_mortarsynth

- Source: `dlls/hl2_dll/npc_mortarsynth.cpp`.
- Model files: `models/mortarsynth.mdl`, `models/gibs/mortarsynth_gibs.mdl`.
- Effect materials: `sprites/physbeam.vmt`, `sprites/glow01.vmt`, `sprites/physring1.vmt`.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_IDLE`, `ACT_MSYNTH_FLINCH_BACK`, `ACT_MSYNTH_FLINCH_FRONT`, `ACT_MSYNTH_FLINCH_LEFT`, `ACT_MSYNTH_FLINCH_RIGHT`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_MSynth.EnergyShoot`, `NPC_MSynth.Hover`, `NPC_MSynth.Shoot`, `NPC_MSynth.WarmUp`.

### npc_combineguard

- Source: `dlls/hl2_dll/npc_combineguard.cpp`.
- Model files: `models/combine_guard.mdl`.
- Effect materials: `sprites/blueflare1.vmt`.
- Named attachments: `muzzle`.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_CGUARD_IDLE_TO_ANGRY`, `ACT_COMBINEGUARD_CLOBBERED`, `ACT_COMBINEGUARD_GETUP`, `ACT_COMBINEGUARD_HELPLESS`, `ACT_COMBINEGUARD_TOPPLE`, `ACT_IDLE`, `ACT_IDLE_ANGRY`, `ACT_RANGE_ATTACK1`, `ACT_RUN`, `ACT_WALK`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_CombineGuard.Charging`, `NPC_CombineGuard.Fire`, `NPC_CombineGuard.FootstepLeft`, `NPC_CombineGuard.FootstepRight`.

### npc_conscript

- Source: `dlls/hl2_dll/npc_conscript.cpp`.
- Model files: `models/conscript.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_ARM`, `ACT_BARNACLE_CHEW`, `ACT_BARNACLE_CHOMP`, `ACT_BARNACLE_HIT`, `ACT_BARNACLE_PULL`, `ACT_CONSCRIPT_AIM`, `ACT_IDLE`, `ACT_RUN`, `ACT_WALK`.
- Direct sound files: `barney/ba_close.wav`, `barney/ba_die1.wav`, `barney/ba_die2.wav`, `barney/ba_die3.wav`, `barney/ba_pain1.wav`, `barney/ba_pain2.wav`, `barney/ba_pain3.wav`.
- Sound-script events: none statically identified.

### npc_ichthyosaur

- Source: `dlls/hl2_dll/npc_ichthyosaur.cpp`.
- Model files: `models/ichthyosaur.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_ICH_BITE_HIT`, `ACT_ICH_BITE_MISS`, `ACT_ICH_THRASH`, `ACT_MELEE_ATTACK1`, `ACT_RUN`, `ACT_WALK`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_Ichthyosaur.AttackGrowl`, `NPC_Ichthyosaur.Bite`, `NPC_Ichthyosaur.BiteMiss`.

### npc_lightstalk

- Source: `dlls/hl2_dll/npc_lightstalk.cpp`.
- Model files: `models/lightstalk.mdl`.
- Effect materials: `sprites/glow03.vmt`.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_CROUCH`, `ACT_CROUCHIDLE`, `ACT_IDLE`, `ACT_STAND`.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_rollerbull

- Source: `dlls/hl2_dll/npc_rollerbull.cpp`.
- Model files: `models/roller.mdl`, `models/roller_spikes.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: `npc/roller/bull_buzz.wav`, `npc/roller/bull_fast.wav`, `npc/roller/bull_slow.wav`.
- Sound-script events: `NPC_RollerBull.Bark`, `NPC_RollerBull.CloseSpikes`, `NPC_RollerBull.Cut`, `NPC_RollerBull.Idle`, `NPC_RollerBull.Measure`, `NPC_RollerBull.OpenSpikes`, `NPC_RollerBull.Summon`, `NPC_RollerBull.Taunt`, `NPC_RollerBull.Tossed`.

### npc_missiledefense

- Source: `dlls/hl2_dll/npc_missiledefense.cpp`.
- Model files: `models/missile_defense.mdl`, `models/gibs/missile_defense_gibs.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: `npc/entity/turret/turret_loop1.wav`.
- Sound-script events: `NPC_MissileDefense.Attack`, `NPC_MissileDefense.Reload`, `NPC_MissileDefense.Turn`.

### npc_roller

- Source: `dlls/hl2_dll/npc_roller.cpp`.
- Model files: `models/roller.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: `npc/roller/code1.wav`, `npc/roller/code2.wav`, `npc/roller/code3.wav`, `npc/roller/code4.wav`, `npc/roller/code5.wav`, `npc/roller/code6.wav`.
- Sound-script events: `NPC_Roller.Marco`, `NPC_Roller.Polo`, `NPC_Roller.PowerOff`, `NPC_Roller.PowerOn`.

### npc_rollerbuddy

- Source: `dlls/hl2_dll/npc_rollerbuddy.cpp`.
- Model files: `models/roller.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: `npc/roller/code1.wav`, `npc/roller/code2.wav`, `npc/roller/code3.wav`, `npc/roller/code4.wav`, `npc/roller/code5.wav`, `npc/roller/code6.wav`, `npc/roller/roll.wav`.
- Sound-script events: `NPC_RollerBuddy.Confirm`, `NPC_RollerBuddy.RemoteYes`.

### npc_rollerdozer

- Source: `dlls/hl2_dll/npc_rollerdozer.cpp`.
- Model files: `models/roller.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_rollerturret

- Source: `dlls/hl2_dll/npc_rollerturret.cpp`.
- Model files: `models/roller.mdl`.
- Effect materials: `sprites/muzzleflash2.vmt`.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_RollerTurret.Shoot`.

### npc_sscanner

- Source: `dlls/hl2_dll/npc_sscanner.cpp`.
- Model files: `models/shield_scanner.mdl`, `models/scanner_shield.mdl`, `models/gibs/mortarsynth_gibs.mdl`.
- Effect materials: `sprites/physbeam.vmt`, `sprites/glow01.vmt`, `sprites/physring1.vmt`.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_IDLE`, `ACT_IDLE_ANGRY`, `ACT_SSCANNER_CLOSE`, `ACT_SSCANNER_FLINCH_BACK`, `ACT_SSCANNER_FLINCH_FRONT`, `ACT_SSCANNER_FLINCH_LEFT`, `ACT_SSCANNER_FLINCH_RIGHT`, `ACT_SSCANNER_LOOK`, `ACT_SSCANNER_OPEN`.
- Direct sound files: `npc/waste_scanner/grenade_fire.wav`, `npc/waste_scanner/hover.wav`.
- Sound-script events: `NPC_SScanner.FlySound`.

### npc_wscanner

- Source: `dlls/hl2_dll/npc_wscanner.cpp`.
- Model files: `models/wscanner_single.mdl`, `models/wscanner_pair.mdl`, `models/wscanner_tri.mdl`.
- Effect materials: `sprites/glow01.vmt`, `sprites/lgtning.vmt`.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_RANGE_ATTACK1`, `ACT_WSCANNER_FLINCH_BACK`, `ACT_WSCANNER_FLINCH_FRONT`, `ACT_WSCANNER_FLINCH_LEFT`, `ACT_WSCANNER_FLINCH_RIGHT`, `ACT_WSCANNER_JOIN`.
- Direct sound files: none statically identified.
- Sound-script events: `NPC_WScanner.Alarm`, `NPC_WScanner.Attach`, `NPC_WScanner.AttachEvent`, `NPC_WScanner.Hover`, `NPC_WScanner.HoverAlarm`, `NPC_WScanner.LaserShoot`, `NPC_WScanner.LaserWarmUp`, `NPC_WScanner.ShootEvent`, `NPC_WScanner.Split`.

### npc_combineelite

- Source: `dlls/hl2_dll/npc_combinee.cpp`.
- Model files: `models/Combine_Elite.mdl`, `models/combine_elite.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_odell

- Source: `dlls/hl2_dll/npc_odell.cpp`.
- Model files: `models/odell.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: `ACT_180_LEFT`, `ACT_TURN_LEFT`, `ACT_TURN_RIGHT`.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

### npc_launcher

- Source: `dlls/hl2_dll/npc_launcher.cpp`.
- Model files: `models/player.mdl`.
- Effect materials: none statically identified.
- Named attachments: none statically identified.
- Explicit sequence labels: none statically identified.
- Activities referenced by source: none statically identified.
- Direct sound files: none statically identified.
- Sound-script events: none statically identified.

## Rendering/material scope and verification

The supplied V17 DX9 shader source, normal-map sampling, lightmap/Fresnel safeguards, transparency/refraction paths and experimental postprocess resources were inspected. The specific additional numerical repair above changes the real existing cheap-water shader. No material or texture files for restored enemies are present to repair UV assignment, alpha channels, normal/specular maps, cubemaps or textures. Existing utility brick/bump TGAs were retained; they cannot substitute for enemy textures.

The five modern postprocess shader sources remain future integration assets. A later installed-SDK search found fxc/dxc outside PATH, exposed and repaired an HLSL reserved-word syntax error, and compiled four D3D11/D3D12 stage binaries. These project-owned experimental outputs and provenance are under `modernize/shaders/compiled_v18`; exact results are in `reports/SHADER_VALIDATION_V18.md`. Four GLSL/Vulkan jobs remain blocked by unavailable glslangValidator. No DX11, DX12, Vulkan or OpenGL backend is claimed functional, and the legacy DX9 shader permutations remain uncompiled.

Fifteen targeted automated tests passed: three bounded MDL metadata/malformed-content tests, four NPC asset-audit tests, two source-inventory tests, three map-source tests, and three cheap-water numerical/source tests. Both source overlays were rerun successfully. Full release validation is recorded separately by the release integrator. No game build, GPU test, Hammer/VBSP compilation or in-game NPC/animation/audio validation has been performed.

Reproduce the targeted tests from the project root:

```text
python -m unittest modernize.tests.test_source_asset_formats modernize.tests.test_npc_asset_audit modernize.tests.test_cut_npc_inventory modernize.tests.test_cut_npc_maps modernize.tests.test_material_assets -v
python modernize/generate_cut_npcs.py
python modernize/generate_npc_testmaps.py
python modernize/npc_asset_audit.py --assets-root . --out modernize/reports/NPC_ASSET_AUDIT_V18.json
python modernize/build_modern_shaders.py --plan
```

Detailed machine-readable records: `cut_npc_inventory.json`, `reports/NPC_ASSET_AUDIT_V18.json`, `reports/NPC_EXTERNAL_ASSET_AUDIT_V18.json`. The external-root report is machine-specific and is intentionally distinct from deliverable content verification.
