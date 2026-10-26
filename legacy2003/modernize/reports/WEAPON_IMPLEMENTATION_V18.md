# Weapon implementation and branch inventory — v18

Status: source repairs and automated checks completed within the supplied source. No game DLL was successfully built or launched. No weapon is certified playable, visually correct, or animation-complete by this report. The source archive contains no original weapon model, material, texture, animation, or weapon-script payload to restore. Read-only installation inspection is recorded separately by the asset audit; those proprietary assets are not included in this release.

## Implemented behavior

- `GiveCheatWeapon` validates a branch-supplied classname, duplicate ownership, the 48-entry inventory capacity, a loadable weapon script, both configured model paths, the installed MDL header/signature/declared length, and registered ammunition before spawning. It accepts current studio version 36 and the source's explicit 35→36 compatibility path. Other version metadata is rejected conservatively; the old engine converter overwriting a newer version field is not proof that the newer layout works. These checks do not certify the full model, companion mesh, skeleton, materials, attachments, or animation data.
- The helper preserves/restores `gEvilImpulse101`, checks actual ownership after Touch, and removes a newly created rejected orphan. Already owned weapons are not duplicated. Normal `give`, map pickups, team rules, and their asset loading remain unchanged.
- All four branch overrides now honor `g_iWeaponCheat` and demo-build restrictions. HL2 retains its original suit/health/base arsenal behavior. HL1 keeps its 14 original weapons plus the deliberately shared, project-compiled physics tool. The shared cubemap debugging tool is inventoried and excluded from gameplay grants.
- Refill derives primary and secondary reserve limits from each actually owned weapon's branch ammo registry and fills valid clips. This covers prototype ammo and CS HE/flashbang ammunition without adding foreign ammo names. Infinite/nonpositive ammo limits are not converted into invalid counts.
- CS expanded cheats temporarily allow multiple rifles/pistols while granting. Normal pickup/drop slot limits remain in effect. When script positions collide, all weapons in that slot receive a deterministic client/server order from original position plus classname, bounded by the existing 20-position HUD capacity. The present CS arsenal fits that limit. The cheat supplies the branch's existing armor-and-helmet state and preserves its money grant; absent night-vision/defuser implementations are not invented.
- HL2 HUD fitting from v17 is preserved. CS and TF2 selection icons now retain aspect ratio and original resolution within their existing layouts, using actual active/inactive artwork. Shared HUD texture loading/reloading rejects out-of-atlas rectangles against actual loaded dimensions, avoids zero-size division and invalid crop drawing, and permits the existing HL2 missing-icon label to appear. No substitute art is mislabeled as a finished original icon.
- The shared HUD loader now supports existing legacy nested raster layouts: a nonempty direct file wins with its original path unchanged; otherwise the first nonempty 640 file, then 320 file is selected. The top-level role name is preserved while rectangles come from the selected layout. Only a leaf filename selected from a nested legacy layout receives the sprites/ prefix. Direct leaf materials and already qualified paths retain existing behavior. Font-only or selected font metadata remains unsupported by this raster HUD source and runtime-unverified; no new glyph renderer is implied. Synthetic C++98 tests exercise precedence, empty values, font metadata, path preservation, prefix bounds, and the actual production helper.
- TF2 right-hand container weapons now receive server ItemPostFrame updates without an inappropriate client-prediction condition; handle comparisons use explicit `.Get()`. The container is excluded from standalone cheat grants because normal class loadouts must initialize compatible children.

## Recoverable and unfinished prototypes

| Weapon | Source work and remaining limits |
|---|---|
| HL2 incendiary rifle `weapon_irifle` | Removed the leading `f` compile typo; precaches its existing flare entity; validates ammo and consumes it only after successful flare creation; updates attack timers; switching to it no longer manufactures 90 rounds. Existing original flare, weapon script, meshes and animations are still required; runtime unverified. |
| HL2 tripwire `weapon_tripwire` | Removed unconditional self-removal from Spawn so the existing deployment code can run; validates the returned mine pointer; stops post-removal animation calls; fixes the member-function callback syntax. Existing scripts, compatible weapon/projectile models, rope/missile effects, animations, and damage settings remain required. Its historical global precache registration stays disabled because those resources are not supplied. |
| HL2 thumper `weapon_thumper` | Portable entity precaches before SetModel; ammo is charged only for successful grounded placement; pickup restores ammo or confirms inventory ownership before removing the placed device; null activators are safe; callback pointers are explicit. Original device model, sounds, and weapon animations remain required. |
| HL2 immolator `weapon_immolator` | Initializes the target sentinel, guards missing/dead owners, applies the existing burn tick on direct entity impacts, attributes burn damage to the owner, and stops burning on holster; explicit callback pointer. This retains the existing experimental damage/radius tuning and does not certify effects, game balance, or animations. |
| HL2 manhack weapon `weapon_manhack` | Both attack bodies are empty; ItemPostFrame only idles. It is registered/compiled but excluded as incomplete. A gameplay design, actual deployment/control implementation, and weapon-specific visual/animation integration are required. The unrelated existing NPC is not presented as a finished weapon. |
| HL2 brickbat `weapon_brickbat` | Registration and global precache lines are commented out; it is not a registered weapon. It also has disabled pickup-ammo logic and uncompiled source. Remains an explicit restoration candidate excluded from grants, rather than activating an unsafe partially implemented class. |
| TF2 `weapon_combat_chargeableplasma`, `weapon_infiltrator`, `weapon_laserdesignator`, `weapon_laserrifle`, `weapon_mortar`, `weapon_pistols` | Registered in their source files but excluded from the branch's Debug and Release server project inputs. Historical opt-in tooling is preserved. Simply adding source files does not validate their obsolete API usage, network counterparts, class/technology dependencies, zero-valued damage settings, mortar build-object chain, scripts, models, animations, projectiles, or effects. No default build inclusion or working status is fabricated. Chargeable-plasma and mortar client implementations are present in source; presence alone does not establish compatible builds. |
| TF2 `weapon_objectselection`, `weapon_twohandedcontainer` | Internal builder/class-loadout objects. The selection needs a valid object subtype; the two-handed container needs compatible children. Neither is granted as an independently functioning weapon. Normal branch initialization paths are preserved. |
| CS smoke grenade / other retail-only names | No concrete registration or source implementation exists in this supplied CS branch. No guessed registration, retail script, or another game's weapon is added. The definitive supported inventory below is the source inventory. |

The original model UVs, origins, proportions, attachment transforms, hand positions, and animation curves cannot be edited or reconstructed faithfully from weapon C++ alone. No model source/geometry/animation files or authorized replacement mesh were supplied in the archive. Placeholder weapons/icons were not generated. Refer to the complete weapon asset audit for exact model, material, HUD texture and script paths actually found in an authorized installation.

## Validation and compilation limits

- Targeted weapon tests: 20 passed, 0 failed, 0 skipped, using the bundled Python and installed Visual Studio LLVM. Includes actual C++98 compile-and-execute checks for grant path/header/ammo policy and HUD atlas/fitting, source/inventory regression checks, branch contamination checks, and old/new generator idempotence. Command: `python -m unittest modernize.tests.test_weapon_restoration_v18 modernize.tests.test_weapon_asset_audit modernize.tests.test_weapon_hud modernize.tests.test_prototype_weapon_compile -v` with `clang++` available in PATH.
- All active registrations were checked after stripping comments against preprocessor nesting: none of the current 113 branch inventory rows is hidden behind an unevaluated `#if`/`#ifdef` registration guard. Commented brickbat and duplicate historical thumper code are not counted.
- `check_weapon_syntax_v18.py` makes 11 real clang-cl translation-unit probes using Release/Win32 server project definitions and includes; results and exact diagnostics are in `weapon_syntax_v18.json`. The probes are blocked by legacy compiler/header incompatibilities, so no server compilation pass is claimed. They did reach weapon bodies and exposed issues repaired here: const-correct CS ordering, missing IFileSystem include, private base weapon datamap access, old member-function callback syntax, and ambiguous TF2 network-handle comparisons.
- Remaining diagnostic families include const-incorrect `Q_strrchr`/`Q_strstr` wrappers, ambiguous x86 inline-assembly operand sizes, a temporary reference in `props.h`, and existing player/datamap compiler issues. The JSON records the tested state; other agents may separately repair shared modernization code. A complete Windows build, linking, weapon selection, firing/reloading, multiplayer replication, class restrictions and asset loading still require future validation.

## Definitive source inventory

Each script path is derived from the actual `ReadWeaponDataFromFileForSlot` convention and is a required reference, not proof the script is supplied. “Project included” means both Debug and Release server inputs, not successful compilation. For detailed VM/world model, materials and HUD content, use the companion asset audit. All rows remain runtime-unverified.

### HL2

| Entity | Implementation | Required script | Server project | Grant/source status |
|---|---|---|---|---|
| `weapon_ar1` | `dlls/hl2_dll/weapon_ar1.cpp` | `scripts/weapon_ar1.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_ar2` | `dlls/hl2_dll/weapon_ar2.cpp` | `scripts/weapon_ar2.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_binoculars` | `dlls/hl2_dll/weapon_binoculars.cpp` | `scripts/weapon_binoculars.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_bugbait` | `dlls/hl2_dll/weapon_bugbait.cpp` | `scripts/weapon_bugbait.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_cguard` | `dlls/hl2_dll/weapon_cguard.cpp` | `scripts/weapon_cguard.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_crowbar` | `dlls/hl2_dll/weapon_crowbar.cpp` | `scripts/weapon_crowbar.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_cubemap` | `dlls/weapon_cubemap.cpp` | `scripts/weapon_cubemap.txt` | included | Excluded: shared cubemap debugging tool, not a branch gameplay weapon |
| `weapon_extinguisher` | `dlls/hl2_dll/weapon_extinguisher.cpp` | `scripts/weapon_extinguisher.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_flaregun` | `dlls/hl2_dll/weapon_flaregun.cpp` | `scripts/weapon_flaregun.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_frag` | `dlls/hl2_dll/weapon_frag.cpp` | `scripts/weapon_frag.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_gauss` | `dlls/hl2_dll/weapon_gauss.cpp` | `scripts/weapon_gauss.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_hmg1` | `dlls/hl2_dll/weapon_hgm1.cpp` | `scripts/weapon_hmg1.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_hopwire` | `dlls/hl2_dll/weapon_hopwire.cpp` | `scripts/weapon_hopwire.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_iceaxe` | `dlls/hl2_dll/weapon_iceaxe.cpp` | `scripts/weapon_iceaxe.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_immolator` | `dlls/hl2_dll/weapon_immolator.cpp` | `scripts/weapon_immolator.txt` | included | Expanded cheat candidate; asset-validated spawn; repaired source, runtime unverified |
| `weapon_irifle` | `dlls/hl2_dll/weapon_irifle.cpp` | `scripts/weapon_irifle.txt` | included | Expanded cheat candidate; asset-validated spawn; repaired source, runtime unverified |
| `weapon_manhack` | `dlls/hl2_dll/weapon_manhack.cpp` | `scripts/weapon_manhack.txt` | included | Excluded: incomplete prototype: both attacks are empty and ItemPostFrame only idles |
| `weapon_molotov` | `dlls/hl2_dll/weapon_molotov.cpp` | `scripts/weapon_molotov.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_physcannon` | `dlls/hl2_dll/weapon_physcannon.cpp` | `scripts/weapon_physcannon.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_physgun` | `dlls/physgun.cpp` | `scripts/weapon_physgun.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_pistol` | `dlls/hl2_dll/weapon_pistol.cpp` | `scripts/weapon_pistol.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_rollerwand` | `dlls/hl2_dll/weapon_rollerwand.cpp` | `scripts/weapon_rollerwand.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_rpg` | `dlls/hl2_dll/weapon_rpg.cpp` | `scripts/weapon_rpg.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_shotgun` | `dlls/hl2_dll/weapon_shotgun.cpp` | `scripts/weapon_shotgun.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_slam` | `dlls/hl2_dll/weapon_slam.cpp` | `scripts/weapon_slam.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_smg1` | `dlls/hl2_dll/weapon_smg1.cpp` | `scripts/weapon_smg1.txt` | included | Original base cheat; asset-validated spawn |
| `weapon_smg2` | `dlls/hl2_dll/weapon_smg2.cpp` | `scripts/weapon_smg2.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_sniperrifle` | `dlls/hl2_dll/weapon_sniperrifle.cpp` | `scripts/weapon_sniperrifle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_stickylauncher` | `dlls/hl2_dll/weapon_stickylauncher.cpp` | `scripts/weapon_stickylauncher.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_stunstick` | `dlls/hl2_dll/weapon_stunstick.cpp` | `scripts/weapon_stunstick.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_thumper` | `dlls/hl2_dll/weapon_thumper.cpp` | `scripts/weapon_thumper.txt` | included | Expanded cheat candidate; asset-validated spawn; repaired source, runtime unverified |
| `weapon_tripwire` | `dlls/hl2_dll/weapon_tripwire.cpp` | `scripts/weapon_tripwire.txt` | included | Expanded cheat candidate; asset-validated spawn; repaired source, runtime unverified |
| `weapon_brickbat` | `dlls/hl2_dll/weapon_brickbat.cpp` | not a registered script contract | excluded/unregistered | Commented registration; unfinished prototype |

### HL1

| Entity | Implementation | Required script | Server project | Grant/source status |
|---|---|---|---|---|
| `weapon_357` | `dlls/hl1_dll/hl1_weapon_357.cpp` | `scripts/weapon_357.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_crossbow` | `dlls/hl1_dll/hl1_weapon_crossbow.cpp` | `scripts/weapon_crossbow.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_crowbar` | `dlls/hl1_dll/hl1_weapon_crowbar.cpp` | `scripts/weapon_crowbar.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_egon` | `dlls/hl1_dll/hl1_weapon_egon.cpp` | `scripts/weapon_egon.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_gauss` | `dlls/hl1_dll/hl1_weapon_gauss.cpp` | `scripts/weapon_gauss.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_glock` | `dlls/hl1_dll/hl1_weapon_glock.cpp` | `scripts/weapon_glock.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_handgrenade` | `dlls/hl1_dll/hl1_weapon_handgrenade.cpp` | `scripts/weapon_handgrenade.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_hornetgun` | `dlls/hl1_dll/hl1_weapon_hornetgun.cpp` | `scripts/weapon_hornetgun.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_mp5` | `dlls/hl1_dll/hl1_weapon_mp5.cpp` | `scripts/weapon_mp5.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_physgun` | `dlls/physgun.cpp` | `scripts/weapon_physgun.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_rpg` | `dlls/hl1_dll/hl1_weapon_rpg.cpp` | `scripts/weapon_rpg.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_satchel` | `dlls/hl1_dll/hl1_weapon_satchel.cpp` | `scripts/weapon_satchel.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_shotgun` | `dlls/hl1_dll/hl1_weapon_shotgun.cpp` | `scripts/weapon_shotgun.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_snark` | `dlls/hl1_dll/hl1_weapon_snark.cpp` | `scripts/weapon_snark.txt` | included | Branch-local existing cheat; asset-validated spawn |
| `weapon_tripmine` | `dlls/hl1_dll/hl1_weapon_tripmine.cpp` | `scripts/weapon_tripmine.txt` | included | Branch-local existing cheat; asset-validated spawn |

### CSTRIKE

| Entity | Implementation | Required script | Server project | Grant/source status |
|---|---|---|---|---|
| `weapon_ak47` | `game_shared/cstrike/weapon_ak47.cpp` | `scripts/weapon_ak47.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_aug` | `game_shared/cstrike/weapon_aug.cpp` | `scripts/weapon_aug.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_awp` | `game_shared/cstrike/weapon_awp.cpp` | `scripts/weapon_awp.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_basecsgrenade` | `game_shared/cstrike/weapon_basecsgrenade.cpp` | `scripts/weapon_basecsgrenade.txt` | included | Excluded: grenade base class without a concrete projectile |
| `weapon_c4` | `game_shared/cstrike/weapon_c4.cpp` | `scripts/weapon_c4.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_cs_base` | `game_shared/cstrike/weapon_csbase.cpp` | `scripts/weapon_cs_base.txt` | included | Excluded: weapon base class |
| `weapon_csbase_gun` | `game_shared/cstrike/weapon_csbasegun.cpp` | `scripts/weapon_csbase_gun.txt` | included | Excluded: weapon base class; Spawn and PrimaryAttack assert |
| `weapon_deagle` | `game_shared/cstrike/weapon_deagle.cpp` | `scripts/weapon_deagle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_famas` | `game_shared/cstrike/weapon_famas.cpp` | `scripts/weapon_famas.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_fiveseven` | `game_shared/cstrike/weapon_fiveseven.cpp` | `scripts/weapon_fiveseven.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_flashbang` | `game_shared/cstrike/weapon_flashbang.cpp` | `scripts/weapon_flashbang.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_g3sg1` | `game_shared/cstrike/weapon_g3sg1.cpp` | `scripts/weapon_g3sg1.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_galil` | `game_shared/cstrike/weapon_galil.cpp` | `scripts/weapon_galil.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_glock` | `game_shared/cstrike/weapon_glock.cpp` | `scripts/weapon_glock.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_hegrenade` | `game_shared/cstrike/weapon_hegrenade.cpp` | `scripts/weapon_hegrenade.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_knife` | `game_shared/cstrike/weapon_knife.cpp` | `scripts/weapon_knife.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_m249` | `game_shared/cstrike/weapon_m249.cpp` | `scripts/weapon_m249.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_m3` | `game_shared/cstrike/weapon_m3.cpp` | `scripts/weapon_m3.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_m4a1` | `game_shared/cstrike/weapon_m4a1.cpp` | `scripts/weapon_m4a1.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_mac10` | `game_shared/cstrike/weapon_mac10.cpp` | `scripts/weapon_mac10.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_mp5navy` | `game_shared/cstrike/weapon_mp5navy.cpp` | `scripts/weapon_mp5navy.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_p228` | `game_shared/cstrike/weapon_p228.cpp` | `scripts/weapon_p228.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_p90` | `game_shared/cstrike/weapon_p90.cpp` | `scripts/weapon_p90.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_scout` | `game_shared/cstrike/weapon_scout.cpp` | `scripts/weapon_scout.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_sg550` | `game_shared/cstrike/weapon_sg550.cpp` | `scripts/weapon_sg550.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_sg552` | `game_shared/cstrike/weapon_sg552.cpp` | `scripts/weapon_sg552.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_ump45` | `game_shared/cstrike/weapon_ump45.cpp` | `scripts/weapon_ump45.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_usp` | `game_shared/cstrike/weapon_usp.cpp` | `scripts/weapon_usp.txt` | included | Expanded cheat candidate; asset-validated spawn |

### TF2

| Entity | Implementation | Required script | Server project | Grant/source status |
|---|---|---|---|---|
| `weapon_arcwelder` | `game_shared/tf2/weapon_arcwelder.cpp` | `scripts/weapon_arcwelder.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_basecombatobject` | `game_shared/tf2/weapon_basecombatobject.cpp` | `scripts/weapon_basecombatobject.txt` | included | Excluded: TF2 object weapon base class |
| `weapon_builder` | `game_shared/tf2/weapon_builder.cpp` | `scripts/weapon_builder.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_basegrenade` | `game_shared/tf2/weapon_combat_basegrenade.cpp` | `scripts/weapon_combat_basegrenade.txt` | included | Excluded: TF2 grenade base class |
| `weapon_combat_burstrifle` | `game_shared/tf2/weapon_combat_burstrifle.cpp` | `scripts/weapon_combat_burstrifle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_chargeableplasma` | `game_shared/tf2/weapon_combat_chargeableplasma.cpp` | `scripts/weapon_combat_chargeableplasma.txt` | excluded | Prototype excluded from default build |
| `weapon_combat_grenade` | `game_shared/tf2/weapon_combat_grenade.cpp` | `scripts/weapon_combat_grenade.txt` | included | Excluded: TF2 grenade base without a concrete projectile |
| `weapon_combat_grenade_emp` | `game_shared/tf2/weapon_combat_grenade_emp.cpp` | `scripts/weapon_combat_grenade_emp.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_laserrifle` | `game_shared/tf2/weapon_combat_laserrifle.cpp` | `scripts/weapon_combat_laserrifle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_plasmagrenadelauncher` | `game_shared/tf2/weapon_combat_plasma_grenade_launcher.cpp` | `scripts/weapon_combat_plasmagrenadelauncher.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_plasmarifle` | `game_shared/tf2/weapon_combat_plasmarifle.cpp` | `scripts/weapon_combat_plasmarifle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_plasmarifle_alien` | `game_shared/tf2/weapon_combat_plasmarifle.cpp` | `scripts/weapon_combat_plasmarifle_alien.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_plasmarifle_base` | `game_shared/tf2/weapon_combat_plasmarifle.cpp` | `scripts/weapon_combat_plasmarifle_base.txt` | included | Excluded: TF2 plasma rifle base class |
| `weapon_combat_shield` | `game_shared/tf2/weapon_combatshield.cpp` | `scripts/weapon_combat_shield.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_shield_alien` | `game_shared/tf2/weapon_combatshield.cpp` | `scripts/weapon_combat_shield_alien.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_shotgun` | `game_shared/tf2/weapon_combat_shotgun.cpp` | `scripts/weapon_combat_shotgun.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_combat_usedwithshieldbase` | `game_shared/tf2/weapon_combat_usedwithshieldbase.cpp` | `scripts/weapon_combat_usedwithshieldbase.txt` | included | Excluded: TF2 shield combination base class |
| `weapon_drainbeam` | `game_shared/tf2/weapon_drainbeam.cpp` | `scripts/weapon_drainbeam.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_flame_thrower` | `game_shared/tf2/weapon_flame_thrower.cpp` | `scripts/weapon_flame_thrower.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_gas_can` | `game_shared/tf2/weapon_gas_can.cpp` | `scripts/weapon_gas_can.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_grenade_rocket` | `game_shared/tf2/weapon_grenade_rocket.cpp` | `scripts/weapon_grenade_rocket.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_harpoon` | `game_shared/tf2/weapon_harpoon.cpp` | `scripts/weapon_harpoon.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_infiltrator` | `game_shared/tf2/weapon_infiltrator.cpp` | `scripts/weapon_infiltrator.txt` | excluded | Prototype excluded from default build |
| `weapon_laserdesignator` | `game_shared/tf2/weapon_laserdesignator.cpp` | `scripts/weapon_laserdesignator.txt` | excluded | Prototype excluded from default build |
| `weapon_laserrifle` | `game_shared/tf2/weapon_laserrifle.cpp` | `scripts/weapon_laserrifle.txt` | excluded | Prototype excluded from default build |
| `weapon_limpetmine` | `game_shared/tf2/weapon_limpetmine.cpp` | `scripts/weapon_limpetmine.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_minigun` | `game_shared/tf2/weapon_minigun.cpp` | `scripts/weapon_minigun.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_mortar` | `game_shared/tf2/weapon_mortar.cpp` | `scripts/weapon_mortar.txt` | excluded | Prototype excluded from default build |
| `weapon_obj_empgenerator` | `game_shared/tf2/weapon_obj_empgenerator.cpp` | `scripts/weapon_obj_empgenerator.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_obj_rallyflag` | `game_shared/tf2/weapon_obj_rallyflag.cpp` | `scripts/weapon_obj_rallyflag.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_objectselection` | `game_shared/tf2/weapon_objectselection.cpp` | `scripts/weapon_objectselection.txt` | included | Excluded: internal builder selection entry; requires an initialized object subtype |
| `weapon_pistols` | `game_shared/tf2/weapon_pistols.cpp` | `scripts/weapon_pistols.txt` | excluded | Prototype excluded from default build |
| `weapon_plasmarifle` | `game_shared/tf2/weapon_plasmarifle.cpp` | `scripts/weapon_plasmarifle.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_repairgun` | `game_shared/tf2/weapon_repairgun.cpp` | `scripts/weapon_repairgun.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_rocket_launcher` | `game_shared/tf2/weapon_rocketlauncher.cpp` | `scripts/weapon_rocket_launcher.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_shield` | `game_shared/tf2/weapon_shield.cpp` | `scripts/weapon_shield.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_shield_grenade` | `game_shared/tf2/weapon_shieldgrenade.cpp` | `scripts/weapon_shield_grenade.txt` | included | Expanded cheat candidate; asset-validated spawn |
| `weapon_twohandedcontainer` | `game_shared/tf2/weapon_twohandedcontainer.cpp` | `scripts/weapon_twohandedcontainer.txt` | included | Excluded: internal class-loadout support container; requires initialized compatible child weapons |

## Reproduction and changed source

Run `patch_weapon_restoration_v18.py` after the v17 patches/project conversion; it is in the standard apply chain. The historical grant patches preserve the v18 canonical manifest. Regenerate inventory/report data with the branch/build auditors and `write_weapon_report_v18.py`. No build projects, third-party assets, or binaries are silently enabled or replaced by this patch.

Modified engine/game sources: `dlls/player.cpp`, `dlls/player.h`, the HL2/HL1/CS/TF2 player overrides, `weapon_irifle.cpp`, `weapon_tripwire.cpp`, `weapon_thumper.cpp`, `weapon_immolator.cpp`, `game_shared/basecombatweapon_shared.cpp`, `game_shared/hl2/basehlcombatweapon_shared.h`, `game_shared/tf2/weapon_twohandedcontainer.cpp`, `cl_dll/hud.cpp`, and CS/TF2 weapon-selection HUD source. Supporting changes: portable grant/header policy, extended HUD atlas policy, source inventory/build/grant JSON, regression/smoke tests, reproducible final patch, syntax-probe tool/report, report generator, the two historical grant generators, and the standard apply-chain entry.
