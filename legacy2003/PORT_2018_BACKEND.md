# Porting the 2018 Source backend into GammaSource

Goal: run GammaSource on the backend from `source-engine-master` (nillerusr, TF2 2018 base) — 64-bit, its renderer, materialsystem, filesystem, physics and fixes — while the frontend stays exactly as it is now: HUD, weapon selector, menus and their background, options tabs, every restored weapon, terrain deformation, and all E3/2002-03 maps.

The current 32-bit GammaSource build stays playable the whole time. Port work happens in a separate tree and only replaces `GammaSource_Build` once a milestone is verified in game.

## What the survey found

| Area | 2003 GammaSource | 2018 engine |
| --- | --- | --- |
| Maps | BSP v18 only | BSP v19–v21 (minimum 19) |
| Models | MDL v36 (v37 converted) | MDL v44–v49 with separate .vvd vertex files |
| Textures | VTF 7.0–7.2 | VTF up to 7.5 |
| Engine/client/server interfaces | 51 shared interfaces: 38 changed version, 13 same | 115 interfaces that don't exist in 2003 |
| Foundation libraries | tier0, vstdlib (no tier1/2/3, mathlib split) | tier0–tier3, mathlib, datacache, inputsystem, soundsystem, particles |
| 64-bit blockers in 2003 code | 162 inline `__asm` blocks (130 in `public/`), ~385 pointer↔int casts, prebuilt 32-bit libs: Havok/IVP, Miles (mss32), d3dx9, S3TC, vmpi, tracker | 64-bit already works |

So "bring the backend in" means: the 2018 engine, materialsystem, shaders, studiorender, vphysics, filesystem, vgui and foundation libraries replace the 2003 ones; the 2003 game code, HUD and GameUI are moved onto the 2018 interfaces; and the 2018 engine learns to load the 2003-era content.

## Milestones (each ends with an in-game check)

1. **Build the 2018 engine here, 32- and 64-bit**, and boot it with retail-format content (BSP v19/20 maps already in the Mega Build) to prove the toolchain.
2. **Legacy content in the 2018 engine**
   - BSP v18 loading (lump/struct differences from v19, mostly leaf ambient lighting and lump headers).
   - One engine loads every model version: MDL v36 and v37 are upgraded to the v48 layout in memory as they load (vertices into a .vvd-style block, header and sequence tables rebuilt), next to the native v44–v49 path, so 2001–2003 viewmodels, hands and NPCs keep their geometry, animations and attachments with no offline step.
   - One engine loads every map version: BSP v18 is read natively alongside v19–v21 (faces without the old average-light block, leaves without ambient cubes, old world lights). An offline converter (`modernize/bsp18_to_19.py`) already exists for testing.
   - Old VMT/shader names mapped onto 2018 shaders so E3 maps look the same.
   Check: every E3 2002/2003 map loads and renders correctly.

   Status (2026-10-25):
   - Done: BSP v18 loads natively (`engine/modelloader.cpp`, `BSP_ConvertV18ToV19InPlace`); `e3_strider` renders lit and textured.
   - Done: MDL v36/v37 load natively. `datacache/legacymdl.cpp` is a C++ port of `modernize/model_conversion/mdl36_to_48.py` and `model37.py`, called from `CMDLCache::ReadMDLFile`. The converted `.mdl/.vvd/.vtx` set is written to `<mod>/legacy_models_cache`, which goes at the head of the GAME path, so each model converts once. Delete that folder to reconvert. `mdl_legacy_convert 0` turns it off.
     - When a v36 vertex has four weights, the lightest one is dropped and the order is kept. The VTX bone indices are remapped to match.
     - In 64-bit builds every model needs a studiohdr2, which holds the cache back link. Without it, all models share one vertex cache.
   - Done: shaders. The 2018 stdshaders match the Source 2013 `.vcs` combo layout (for example `lightmappedgeneric_ps20b` has 96 dynamic combos). Retail HL2 (2024 update) ships a different layout, and its world surfaces render white. Garry's Mod's `sourceengine/hl2_misc_dir.vpk` matches. Its `shaders/` folder is extracted to `source2018_assets/gmodshaders` and mounted first. Building our own `.vcs` from `materialsystem/stdshaders` will remove the GMod dependency.
   - Done: 2002-2003 shader names are aliased in `materialsystem/cmaterial.cpp` (`GammaLegacyShaderName`).
   - Done: the converter also carries flexes (vertex deltas re-encoded as float16), flex controllers/rules and eyeballs. Only the 2003 header flag bits that mean the same in v48 are kept.
   - Done: the 2003 Predator (refraction) and JellyFish (hydra glow) shaders. They are rewritten in HLSL as `gamma_predator_vs20/ps20/ps20b.fxc` with C++ in `gamma_legacy_shaders.cpp`. `gamma_build_shaders.py` compiles them with the Windows SDK `fxc.exe` into VCS v6 files (output in `source2018_assets/gammashaders`) and writes their combo headers.
   - Done: mesh ids are renumbered 0..n-1 (2003 models often leave them all 0). The 2018 hardware mesh data is indexed by them, and static prop lighting crashed without this.
   - Still to do: IK rules/locks and autolayers in the converter, and v37 models with external animation groups.

   Ground deformation (2026-10-25):
   - Done in the engine: `engine/terrainmod_functions.*` (from 2003), `CDispInfo::ApplyTerrainMod` (render verts and the static vertex buffer), `CDispCollTree::ApplyTerrainMod` (collision verts, triangle planes, AABB tree, tri cache), and `CM_ApplyTerrainMod`. Displacements are linked into extra BSP leaves at load (the mod limits: 80 sideways, 80 up, 20 down) so deformed ground keeps colliding.
   - Done for the game DLLs: the `GammaTerrainMod001` interface (`public/igammaterrainmod.h`), `game/server/gamma/terrainmodmgr.cpp`, `env_terrainmorph`/`tectonic`, the client `TerrainMod` message, and the scorch decal.
   - Changed from 2003: mods are no longer put in the signon buffer, which overflows on the 2018 engine. The server keeps the level's mods and streams them to each client a few per frame.
   - Done: save/load carries the mods. A "GammaTerrainMods" save block holds the list; on restore, collision is re-applied and the client gets the list again.
   - Collision gotchas, both fixed:
     - The trace code culls with a cached copy of each displacement's bounds (`g_pDispBounds`), which must be refreshed after a mod.
     - Displacements are linked into leaves at least 512 units around them. GammaSource's autoexec sets `disp_modlimit*` to 512.
   - Checked on `e3_techdemo_2`: firing `dent` deforms the ground, player-sized traces and walking collide with it, and after save and load both the look and the collision match.
   - Still to do: re-linking render leaves (deformed ground can be culled at the edge of a leaf).
   - Dev helper: `gamma_mapstart_cmd` / `gamma_mapstart_delay` run a command after a map starts. This is used for automated checks.
3. **Game code on the 2018 interfaces**: port `cl_dll`, `dlls`, `game_shared` (2003 HL2 game, all restored weapons and NPCs, deformation/terrain mod, hopwire, manhack control, brickbat, TF2 prototypes) to the 2018 engine/client/server APIs. 64-bit-clean as it goes (remove inline asm, fix pointer casts).
   - Ground deformation: the 2018 engine removed it. Port `engine/terrainmod_functions`, `public/terrainmod.h` and the displacement hooks into the 2018 displacement code, plus the client/server `terrainmodmgr`.
   Check: weapons, NPCs, ground deformation and the E3 scripted sequences behave as they do now.
4. **Frontend parity**: port the GammaSource HUD (bitmap counters, ENERGY, HL1-style selector, colour tab) and GameUI (menus, options tabs incl. Window Mode/HUD, the menu background) onto the 2018 vgui2/GameUI so they are pixel-identical to the current build.
   Check: side-by-side screenshots against the current build.
5. **Third-party replacements for 64-bit**: IVP physics from source (both trees have it) instead of the prebuilt Havok libs; 2018 audio path instead of Miles; no prebuilt d3dx9/S3TC.
6. **64-bit release build**, staged into `GammaSource_Build` after the full checklist passes; 32-bit kept as a fallback.

## Milestone 3 in practice (2026-10-25)

The 2018 HL2 game code stays the base, because many 2018 systems (player, NPCs, physics pickups) depend on its classes. On top of it:
- The cut 2003 entities are ported into `game/server/gamma/` and `game/client/gamma/`.
- Classes we changed in GammaSource replace or patch their 2018 counterparts.

Tooling, in `modernize/port2018/`:
- `port2003.py` makes the mechanical 2003 to 2018 API renames: datadesc macros, effects, `SetFOV`, `GetAbsMins`, `RadiusDamage`, `Holster`, `min`/`max`, and so on.
- `fixups2003*.py` holds the per-file hand fixes, so a re-port reproduces them.
- `patch2018_*.py` holds the edits to 2018 files.
- `shot2018.ps1`, `bisect2018.ps1`, `minidbg.py` (with a dbghelp stack walk) and `sync2018.ps1` run the in-game checks.

Gotchas:
- vpc `$File` lines cannot be commented out with `//`. The parser still compiles them, so delete the line.
- New server classes need client classes. Use `c_weapon__stubs_gamma.cpp` for weapons, and port the 2003 client class for effects.
- The client stub's `LINK_ENTITY_TO_CLASS` name is what the weapon script is looked up by.
- 2003 `CBaseGrenade::GetOwner/SetOwner` is the 2018 `GetThrower/SetThrower`.
- `MAX_AMMO_TYPES` and `MAX_AMMO_SLOTS` are raised to 64. The 2003 ammo types (`SmallRound`, `MediumRound`, ..., `Extinguisher`, `Hopwire`, plus `Gasoline` for the immolator) are added in `hl2_gamerules.cpp`.

Status:
- Ported and checked in game: ar1, binoculars, brickbat (+grenade), extinguisher (+jet and its client effect), gauss, hmg1, hopwire (+tetherhook), iceaxe, immolator, irifle, manhack (+player_manhack), molotov, slam (+satchel), smg2, sniperrifle, stickylauncher (+client sticky bomb), thumper (+rotorwash TE), tripwire, and cguard (ours replaces the 2018 one).
- Stunstick: our delayed hit is patched into the 2018 stunstick, because the 2018 metropolice uses its API.
- AR2: the 2003 AR2 is `weapon_ar2` (`CWeaponAR2Beta`), and the 2018 AR2 is `weapon_ar2future`. Its script is currently the retail one; beta hands and an icon are still to do.
- Kept as 2018 code with our scripts and models: crowbar, pistol, smg1, shotgun, rpg, frag, physcannon, flaregun, bugbait. Other 2018 systems depend on these classes.
- NPCs: the cut 2003 NPCs are ported and spawn in game. They are combineguard, conscript, odell (+npc_leader), crabsynth, mortarsynth, lightstalk, antliongrub, sscanner (+shield beam/scanner_shield), wscanner, the roller family, combineelite, grenade_scanner and energy_wave. Rollerwand is ported too.
  - The NPC classes and hints they need are added: `CLASS_MORTAR_SYNTH`, `CLASS_WASTE_SCANNER`, `CLASS_CREMATOR` with their 2003 relationships, and `HINT_ROLLER_*`.
  - The combine guard client effects are rewritten for the 2018 particle API. The energy wave client is ported.
  - The 2018 response system accepts the 2003 `wav` response type.
  - There is no `models/Combine_Elite.mdl` in any of our content, so npc_combineelite has no model. It was the same in the 2003 build.
- Sound scripts: GammaSource's 2003 scripts override the retail ones by name. `merge_soundscripts.py` builds `gammasource_overlay/` (2003 entries plus the retail entries they lack, and a merged manifest). That folder is mounted first in the 2018 build.
- World entities the maps actually use: `trigger_camera` (alias of `point_viewcontrol`), `npc_antlionmaker` (ported), `npc_tripmine` (2003 port), and `weapon_physgun` (the 2018 `physgun.cpp` plus its client, with small fixes). The rest of the "missing" list is unused by any map: trigger_vphysics was a commented-out example in 2003, and the teamplay rules, znpc_helicopter and gunshipcrashfx are not used.
- Check (2026-10-25): e3_strider, e3_hydra, e3_lab and e3_techdemo_2 load and run for 80 s with no unknown-entity, missing-class or crash reports.
- Dev tools: `gamma_mapstart_cmd`, `gamma_delay <s> <cmd>` (run as the local player), `gamma_trace_down`, `gamma_trace_hull`.

## Milestone 4 in practice (2026-10-25)

- HUD: the 2003 HUD elements (numeric displays, health, battery, ammo, suit power/ENERGY, quick info, flashlight, HL1-style weapon selection) replace the 2018 ones in `game/client/gamma/`; `hud_color_custom` / `hud_color_r/g/b` are in `hud.cpp`.
- Main menu (`gameui/BasePanel.cpp`):
  - Always draws the static `console/console_background` (no background map image).
  - The menu items sit bottom-left at the 2003 `CTaskbar` metrics (inset 32, 64-pixel bar), and the game title is the 2003 `resource/game_menu` image.
  - The touch-device CONSOLE item is removed.
  - `resource/SourceScheme.res` (the 2003 TrackerScheme) gains the 2018 key names `MainMenu.TextColor`, `MainMenu.MenuItemHeight` and so on, with the 2003 InGameDesktop values.
- The 2018 menu never faded in while another Source game (our 2003 build) was running. It waited on the old Steam `ValvePlatformUIMutex`; that wait is removed in `GameUI_Interface.cpp`. A missing `PlatformMenu.vdf` also counts as "platform ready".
- Options dialog: the 2003 tabs and order (Keyboard, Mouse, Audio, Video, Voice, Lock, HUD, Multiplayer) and tab width 72.
  - Lock is the 2003 content-lock page (`OptionsSubAdvanced`).
  - HUD is our color tab (`OptionsSubHUD.h`).
  - Video has "Borderless windowed", backed by the new engine convar `vid_borderless` (same as `-noborder`).
- Dialogs fall back to centering on the screen when the workspace is still the tiny startup window.

## Weapon roster in practice (2026-10-25)

- `impulse 101` gives every registered `weapon_*` class that has a script, plus every ammo type at its maximum.
- `weapon_vortexhopwire`: the 2018 episodic vortex hopwire (what HL2: Aftermath shipped). `modernize/port2018/make_vortexhopwire.py` copies and renames it into `gamma/`. It uses our hopwire models.
- `weapon_proto1`: Aftermath ships no source, so this is a reconstruction (`gamma/weapon_proto1.cpp`). It is built from the convars, sounds and messages its `server.dll` exposes.
  - Primary fires a bolt that passes through up to 64 units of solid and blasts on the far side.
  - Secondary is a ping that shoves light objects and echoes.
  - It uses the revolver model and `ammo_proto1`.
- Beta hands on retail viewmodels: `game/client/gamma/c_betahands.cpp`, configured in `scripts/gamma_betahands.txt`.
  - The retail `v_hand_sheet` (and Chell's `v_hands`) are `$no_draw`.
  - A beta viewmodel with its weapon parts hidden (`v_betahands_*`) is drawn with its bones following the retail ones.
  - Tune it live with `cl_betahands_tweak_pos` / `_ang`.
- Portal gun: the Portal-Base (SonicEraZoR) merge of Portal into HL2.
  - `patch2018_portal.py` copies its portal code over the tree's and builds the HL2 DLLs with `PORTAL;GAMMA_PORTALGUN`. `patch2018_portal_core.py` adds the small core hooks it needs.
  - The player is `CPortal_Player`. Both portals fire by default.
  - GammaSource keeps its own ammo table, HUD, weapon stubs and clientmode.
  - The glow sprites use Portal's additive materials under `sprites/portalgun_fx/`.
  - Portal's content (`portal_pak.vpk`) is mounted after HL2.
- Selector icons: `make_icons.ps1` draws `sprites/w_icons5b`, and `set_icons.py` points the scripts at it. The art is outline art in the HL1-style selector's look: the HL2 icon-font revolver and AR2, and RealBeta's portal gun.
- Builds: `make_build.ps1` stages a Steam-free build in `Documents\GammaSourceTest\<date_time>`.
- `toolsnodraw` is `$no_draw` in the 2018 layer. The 2003 brush entities keep their nodraw faces, which otherwise drew as yellow NODRAW panels.
- Known: on `e3_techdemo_2` and `e3_hydra` the portal gun once placed portals with no input (not reproduced after the ammo table fix).

## Game-code inventory (2003 vs 2018 HL2)

The 2003 HL2 game code registers 493 entity classes; the 2018 HL2 game code has 450 of them and lacks 43. These must come back from the 2003 code:

- Terrain deformation: `env_terrainmorph`, `tectonic` (plus the engine-side terrain mod).
- Weapons: `weapon_binoculars`, `weapon_gauss`, `weapon_hmg1`, `weapon_iceaxe`, `weapon_rollerwand`, `weapon_stickylauncher` (+ `grenade_stickybomb`), and the projectiles/helpers `grenade_crematorhead` (brickbat), `tetherhook` (hopwire), `grenade_scanner`, `energy_wave`, `player_manhack`, `player_missile`.
- NPCs: `npc_antliongrub`, `npc_antlionmaker`, `npc_combineelite`, `npc_combineguard`, `npc_conscript`, `npc_crabsynth`, `npc_lightstalk`, `npc_mortarsynth`, `npc_odell`, `npc_roller`, `npc_rollerbuddy`, `npc_rollerbull`, `npc_rollerdozer`, `npc_rollerturret`, `npc_sscanner` (+ `shield_beam`, `shieldbeam_node`, `scanner_shield`), `npc_wscanner`, `znpc_helicopter`, `gunshipcrashfx`.
- World/triggers: `env_rotorwash`, `trigger_camera`, `trigger_fog`, `trigger_vphysics`, `game_team_master`, and the multiplayer/teamplay game rules.

The other 450 share a name with 2018 classes but many behave differently in 2003 (and many were changed by our restoration work), so the port keeps the 2003 implementation of every gameplay class rather than the 2018 one. The 2018 game code supplies only the engine-facing plumbing (client/server DLL interfaces, networking tables, prediction, rendering hooks).

## After the port: weapon roster additions (requested 2026-10-25)

- Restore every 2003-only entity, weapon, scripted event, mode and technology the 2018 code cut.
- `weapon_357`: the 2018 revolver, re-rigged onto the beta hands, added to the selector.
- `weapon_ar2future`: the 2018 AR2 (the 2003 `weapon_ar2` keeps its name and slot), beta hands.
- From HL2 Aftermath: `weapon_hopwire` renamed `weapon_vortexhopwire`, using the same world and view models as our `weapon_hopwire`; `weapon_proto1` with beta hands and models.
- Portal gun (Portal-Base): fires both portals by default, beta hands.
- Every weapon is granted by `impulse 101`; every new icon is styled like the current weapon selector.

## Risks to know about

- Milestones 3 and 4 are the largest: ~1,900 game-code files written against 2003 APIs.
- Some 2003 behaviour (physics feel, lighting) may differ on the 2018 backend; anything that does gets flagged against the current build rather than silently accepted.
- Both trees are leaked proprietary code; this stays a non-commercial project as the README says.
