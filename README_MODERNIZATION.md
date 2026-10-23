# Half-Life 2 2003 source modernization — WORK IN PROGRESS

> **v18 release:** Use [the v18 release notes](modernize/reports/RELEASE_NOTES_V18.md) for the current source/overlay, actual validation, animated preview, weapon/NPC changes and asset limitations. The v18 overlay applies to the exact supplied v17 ZIP using `modernize/apply_release_overlay.py`. Historical instructions and claims below are retained for provenance; they do not establish current compilation or runtime success.

This is a **source/build migration overlay**, not a functional finished game. It preserves the original 2003 source tree and adds generated Visual Studio 2026 projects, an XInput controller bridge, an opt-in Steamworks client initialization bridge, an experimental console spawn option for cut NPCs, and an FGD for Hammer. This modernization work also repairs the per-file shader build migration, adds a renderer-plug-in selection hook, and includes a GPU capability probe. It has **not** been compiled or run with Visual Studio or the game engine. The generated projects are a first conversion pass, not a guarantee that legacy dependencies resolve.

## Apply to the archive you supplied

1. Extract `source_2003_leak.zip` into a dedicated development directory on Windows. Keep the original archive backed up.
2. Unzip `HL2_2003_Modernization_Overlay_v17.zip` **into the same directory**, preserving paths like `modernize/`, `dlls/`, and `cl_dll/`. (This overlay does not redistribute the original source files.)
3. Open a **Visual Studio 2026 x86 Native Tools Command Prompt**, with the C++ desktop workload and Windows 10/11 SDK installed. VS 2026 uses toolset `v145`.
4. Run `modernize\build_windows.cmd` to (re)generate projects and perform structural checks.
5. Open `HL2_2003_VS2026.sln` to inspect the HL2 workspace. Its build configurations are not checked as auto-build by the conversion script, since dependencies are supplied by the **sequential build script**. Run `modernize\build_windows.cmd Debug` or `modernize\build_windows.cmd Release` to **attempt** compilation. The full build is expected to need manual dependency repairs.
6. Run `py -3 -m unittest discover -s modernize\tests -v` to verify conversion regression tests. For a GPU API capability check, run `modernize\build_gpu_probe.cmd` from a Visual Studio Native Tools prompt.
7. To inspect other games and tools, open `HL2_2003_AllProjects_VS2026.sln` (153 projects).

### What the overlay actually adds

* All 153 legacy VC6 `.dsp` projects converted to Windows x86 `.vcxproj` files, with 348 named build configurations. Preserve the `.dsp` originals. 2 `.sln` files and ordered HL2 build batch files. All `vcxproj` files pass XML parsing and solution GUID/path validation.
* Corrected MSBuild XML namespace emission and unwanted `/nodefaultlib:` parsing in the converter.
* Fixed linker switch misparsing for `/implib:` and `/out:` values ending in `.lib` (48 affected project configuration entries) and preserved 347 per-file custom-build items, including 19 DX9 shader-generation items. These still invoke legacy tools that may need repairs on modern Windows.
* `modernize/build_audit.py` analyzes only the active original HL2 Debug/Release batch configurations. Each mode has 61 build steps, with zero directly missing compiled source paths in this archive, but 20 (Debug) and 17 (Release) unresolved include-directory references. This is *not* evidence that the builds compile.
* Expanded Python/native regression tests validate project XML, shader generation rules, linker dependency extraction, source patch markers, multiplayer chat, and generated VMF geometry (as of v3).
* `-renderapi dx9|dx8|vulkan|opengl|dx11|dx12` selects a shader API DLL name through the existing material system loader. Unavailable modules print an explicit warning and fall back to the original DX9 backend; `-renderapi-strict` disables fallback. **Vulkan/OpenGL/DX11/DX12 DLL implementations are not present; selecting their names does not add those renderers.**
* `modernize/gpu_api_probe.cpp` tests whether Windows can create DX11/DX12 hardware devices, queries the Vulkan *loader*, and queries the legacy OpenGL context. It is a standalone diagnostic, not a Source rendering backend and has not been Windows-compiled here.

* XInput gamepad support with a dynamically loaded Windows system DLL and no additional linker `.lib` dependency. Prefers XInput when available, otherwise uses the original WinMM joystick path. Maps analog sticks, face buttons, bumpers, triggers, Start/Back and D-pad. Handles controller slot changes and some hotplug cases. **Not tested on actual controllers.**
* A cheat-protected `npc_allow_unprecached_create` setting, default `0` (old behavior), that allows experimental `npc_create <classname>` / `npc_create_aimed <classname>` for NPC classes that do not already exist in the map. Added class verification and null-player guard. **This does not ensure the NPC's required assets, schedules, precache, sound, animations or physics are valid.**
* `modernize/hl2_2003_cut_npcs.fgd`, Hammer class definitions for 11 source-level candidate prototypes. Load it as an **additional** game-data file in Hammer. `modernize/cut_npc_inventory.json` documents which classes appear registered in this source. Conscript is excluded because its entire implementation is disabled by `#if 0`.
* `modernize/validation_report.json` documents structural validation results and known missing source paths.

### v3: Multiplayer code and experimental maps

* Hardened the actual server chat handler in `dlls/client.cpp`: bounded and sanitized message text and player names, safe logging, indexed player traversal, server-side `mp_chat_cooldown` limiting with connection-id tracking and explicit join/disconnect reset. This patch is **not** yet built with Visual Studio or tested against networked clients.
* Added an opt-in server-side fair-spawn algorithm for existing Deathmatch/Team Deathmatch (`mp_fair_spawns 1`, default; set `0` for legacy). Valid, non-origin DM spawn points are ranked by their nearest living opposing player; team mode ignores teammates. Needs networked gameplay testing.
* Added `modernize/chat_policy.h` with C++98-compatible chat sanitization/rate logic and a native C++ unit test (`modernize/tests/chat_policy_smoke.cpp`), compiled here with GCC and passing.
* Added 11 individual `modernize/maps/cut_npcs/npc_*_test.vmf` Hammer sources containing prototype spawn entities on map load. They are **not** compiled BSP files, verified NPC restorations, or functional evidence of cut enemy AI.
* Added `modernize/maps/multiplayer/mp_modern_test.vmf` with eight DM spawn classes and simple pickups. `modernize/configs/` contains LAN-first deathmatch and teamplay server templates plus a mapcycle file.
* These are implementation and test scaffolding for phase 7; the 2003 source already contains basic multiplayer game rules, but cooperative campaign, matchmaking, Steam authentication, physics replication validation, and public server security remain unimplemented.

### v4: Multiplayer map votes and XInput reliability

* Added an **opt-in map vote** to the actual server game DLL: `callvote map mp_modern_test`, `vote yes`, `vote no`, and `votestatus`. The provided LAN server templates enable it with `mp_mapvote 1`; all other servers default to disabled. Operators configure `mp_mapvote_duration` (10–120 seconds) and `mp_mapvote_cooldown` (0–600 seconds).
* Maps must have an **ASCII letters/digits/underscore-only identifier** and pass `engine->IsMapValid` both on nomination and before executing `changelevel`. Commands are server-authoritative: at least two clients must be connected, 60% of active players must approve, one vote per authenticated engine userid per slot, disconnects invalidate old ballots, level transitions reset the session, and a frame hook expires stalled votes. **This does not add Steam matchmaking or guarantee attack resistance on public servers.**
* Added a dependency-free C++98 vote-policy library with native tests for safe map names, majorities, disconnected clients, cooldowns, time expiry, and user-slot reuse.
* Fixed XInput sticky buttons: the bitmask now resets to zero each sampled frame including disconnects. Added independent left/right **radial deadzones**, preserving the WinMM fallback. Use `joy_xinput_left_deadzone 7849` and `joy_xinput_right_deadzone 8689` (0–30000). This includes a C++98 controller-axis policy and native tests. **Actual pad behavior needs Windows game testing.**

### v5: Steamworks bootstrap, path safety, multiplayer vote correctness

* A **Windows/x86 opt-in Steamworks client bootstrap** in `engine/sys_mainwind.cpp` loads `steam_api.dll` from the executable directory only (no working-directory DLL search), calls `SteamAPI_Init`, runs `SteamAPI_RunCallbacks` from the client message loop, and calls `SteamAPI_Shutdown` once on normal shutdown. Enable with `-steamworks`, or override with `-nosteamworks`. A valid Steam App ID and SDK-supplied DLL **must be supplied by the developer**. With the flag absent or Steam unavailable, the game stays on its original offline path. A dependency-free C++98 test uses fake Win32 exports to verify repeated init, missing exports, failed initialization, callback pumping, resource cleanup and path construction. **This is not Steam Input, Steam auth, or matchmaking.**
* `Sys_mkdir` now rejects null/empty/oversized paths, avoids unsafe `strcpy` and a potential empty-path underread, and normalizes trailing separators without changing drive roots such as `C:\\`. The reusable `modernize/safe_paths.h` is C++98-compatible with native boundary tests. **The complete engine has not been compiled.**
* Fixed the multiplayer map vote's 64th-slot off-by-one error. The policy now supports slots 1 through 64 and rejects votes immediately when the remaining undecided players cannot reach 60% approval. Added native C++98 tests for 64-player voting and early rejection. Steamworks initialization and map votes are still engine-runtime untested.
* All v5 changes are applied by `modernize/apply_modernization.py` when run on a fresh extraction. No Steamworks SDK code, DLLs, credentials, game binaries, or original licensed assets are included in the patch-only overlay.

**Client Steamworks example (development only):**

```bat
hl2.exe -steamworks
hl2.exe -steamworks -nosteamworks
```

**Important:** Do not ship a development `steam_appid.txt`. Get an authorized Steamworks SDK and App ID for any actual Steam release. This bridge is scoped to client bootstrap; dedicated Steam game server APIs still need their own integration.

### v6: Multiplayer spawn protection and Phase 6 asset inventory

* Added a **server-side, optional spawn-protection window** to `dlls/player.cpp`, controlled by `mp_spawn_protection_seconds` (0 by default; range clamped to 0–10 seconds). Only another player's attacks are blocked. Self-inflicted damage and map/world hazards still work. Pressing primary or secondary attack cancels protection *before* the weapon's `ItemPostFrame` update. Deathmatch and Team Deathmatch examples set two seconds. All state is server-owned and bound to `(player slot, engine userid)`; the server resets state on joins/disconnects. This is not client prediction or proven multiplayer stability.
* `modernize/spawn_protection_policy.h` isolates time windows and identity checks in C++98 with native tests for expiration, time rewinds, invalid slots, slot 64, reused user IDs, cancellation, and a 10-second cap. Integrated into the repeatable patch generator and verified against a clean copy of the archive.
* Added `modernize/npc_asset_audit.py` for **Phase 6 asset readiness**. Use `py -3 modernize\npc_asset_audit.py --assets-root C:\path\to\unpacked\hl2 --out modernize\my_npc_asset_report.json` to look for expected `.mdl` paths in developer-supplied unpacked game directories. The report distinguishes missing files, unknown model references, untested assets, and disabled entity registrations; **it does not parse VPK/GCF containers or verify AI, precaching, models, or spawn behavior**. Its static asset checks run in cross-platform tests with temporary fixture directories.
* The original 2003 gameplay remains selectable with `mp_spawn_protection_seconds 0`. The engine has not been built or runtime-tested in this session.

### Console tests, after you successfully build and run a modified game

```
developer 1
sv_cheats 1
joystick 1
npc_allow_unprecached_create 1
npc_create npc_houndeye
npc_create npc_assassin
```

The NPC console patches now use the engine `MyNPCPointer()` check and clean up NPCs if an unaimed spawn trace has no placement surface. These fixes are source-level only, not runtime-tested.

Only run the experimental NPC option in disposable test maps with complete models and required assets. Use `npc_allow_unprecached_create 0` to restore default restrictions. For XInput you can use `joystick 1`, or launch with `-noxinput` to force the older joystick path. With the XInput bridge enabled, the right stick looks without requiring the legacy `+jlook` binding. There is **no Steam Input integration** yet.

Renderer dispatch examples (missing backends fall back, so use strict mode for developer diagnostics):

```bat
hl2.exe -renderapi dx9
hl2.exe -renderapi vulkan -renderapi-strict
modernize\build_gpu_probe.cmd
```

### v7: Legacy lighting fixes and cross-API tone-map shader sources

* **Real changes to the original DX9 lighting paths:** `materialsystem/colorspace.cpp` now checks gamma and overbright inputs before evaluating reciprocal/pow lookup tables, so invalid/NaN/infinite inputs cannot poison generated gamma tables. Ordinary positive inputs within the historical operating range are unchanged. `common_ps_fxc.h` and `lightmappedgeneric_ps20.fxc` now saturate their legacy Schlick Fresnel base to [0,1] before applying the fifth power, avoiding overrange reflections when the interpolated eye/normal vectors depart from unit length. These patches are source-tested, not compiled in the original shader compiler or verified visually.
* `modernize/lighting_parameters.h`, `patch_lighting.py`, and native C++98 tests make the change reproducible on the original archive.
* Added **experimental** full-screen tonemap GLSL 330, Vulkan GLSL 450, and HLSL shaders that provide an ACES-fitted curve, exposure control, and optional manual sRGB output. They share a numeric CPU reference and are accompanied by `modernize/shaders/README.md` and `modernize/build_modern_shaders.py`.
* Run `py -3 modernize\build_modern_shaders.py --plan` for backend shader compile commands. To compile them on an appropriately equipped Windows/Linux host, run `py -3 modernize\build_modern_shaders.py --compile`. D3D11 needs `fxc`, D3D12 needs `dxc`, Vulkan/OpenGL GLSL checking needs `glslangValidator`. These tools are not installed in the current execution environment. **Neither the shader assets nor their build script constitute working Source renderer backends.**
* Modern shaders are intentionally isolated and not forced into the legacy material system until the actual shader API backends, scene render targets, state/presentation, and supported shader compilation exist.

### v8: Safer teamplay and modern OpenGL detection

* `game_shared/teamplay_gamerules.cpp` now caps dynamically discovered and configured teams at `MAX_TEAMS` (32), protecting the fixed `team_names`/`team_scores` arrays from overrun on servers with many arbitrary team names. It also fixes a 32-player assumption in `TeamWithFewestPlayers` that could return no team on larger servers. These are source changes; the game has not been Windows-compiled.
* Added optional server-side `mp_team_balance` (default **0** for legacy behavior). On two-or-more-team servers, a player-requested team change is permitted only if the projected team-size spread stays within one player or strictly improves an existing imbalance. Unrecognized team names are rejected while the option is active, preventing bypass by creating a new name. Existing players are **not** forcibly moved and default-team autoassignment is unchanged. The example team deathmatch configuration sets `mp_team_balance 1`.
* New portable C++98 `team_balance_policy.h` and independent compiled tests, plus patch-generator regression tests. Uses installed server team/player state rather than trusting submitted counts from clients.
* The standalone Windows GPU probe now actually attempts a **WGL OpenGL 3.3 core** context and reports the result separately from a default/compatibility OpenGL context. It still does not implement a Source rendering backend, validate shaders on a GPU, or verify Vulkan GPU availability.

### v9: Animated-light correctness and reproducible Windows build diagnostics

* Fixed `engine/gl_rlight.cpp` so clearing an animated lightstyle also invalidates the affected lightmap cache. The 2003 code reset intensity but forgot to mark the style frame when the pattern disappeared, which could leave old illumination cached.
* Both server-provided lightstyle strings are now copied into the 64-byte client style buffer with `Q_strncpy` instead of `Q_strcpy` in `engine/cl_parse.cpp`. The sampled intensity is bounded to the original `a`–`z` range even if the network data contains unexpected characters.
* Added optional `r_smooth_lightstyles 1` to interpolate animated patterns at the original 10 Hz sample rate, reducing abrupt transitions. **Default 0 retains original stepped animations.** This does not repair other lighting defects or implement modern shadows.
* `modernize/lightstyle_policy.h` contains portable C++98 lightstyle math, with native boundary tests for interpolation, malformed pattern characters, invalid times, and empty styles. No Windows renderer has been run to test these modifications yet.
* `modernize/build_driver.py` and `modernize/build_with_logs.cmd` preserve the original 61-step build ordering and write individual Windows MSBuild logs plus `.modern_build_logs/Debug|Release/build_report.json`. `--dry-run` prints the order on any platform; `--continue-on-error` is diagnostic only because later builds may depend on earlier outputs. A fake MSBuild integration test verifies error propagation and logging, **not a successful engine compilation**.

On Windows, after preparing the source and installing VS2026, use `modernize\build_with_logs.cmd Debug` or `Release` for captured logs. The original `modernize\build_windows.cmd Debug` remains available. The current environment cannot run Windows MSBuild.

### v10: Multiplayer map rotation, MOTD safety, world-kill stability

* Replaced the 2003 `sscanf(..., "%32s", char[32])` mapcycle parser, which could overflow its destination by one byte and assumed that `engine->LoadFileForMe` returned a NUL-terminated buffer. The new `modernize/server_text_policy.h` parses explicitly bounded byte ranges, rejects oversized or unsafe map tokens instead of truncating them, skips comments, preserves map order, and asks the engine to verify each candidate before changing levels. When no usable candidate exists, it retains a valid current map or the original `hldm1` fallback.
* Corrected the legacy MOTD sender so it copies at most 60 bytes per packet, at most 240 bytes total, and never calls `strlen()` on a file buffer without a terminator. Embedded NUL bytes stop transmission cleanly. The underlying user-message protocol and byte limits remain unchanged.
* Prevented a null-inflictor dereference when `CMultiplayRules::DeathNotice()` formats world or environmental kills; it keeps the existing `world` label if the inflictor is absent.
* Added `modernize/patch_server_text.py` and `patch_deathnotice.py`, and portable C++98 smoke tests plus idempotence tests. These are source-verified changes **not tested in a running Windows Source multiplayer server**.

### v11: Crab Synth charge AI restoration (Phase 6, experimental)

* Repaired `dlls/hl2_dll/npc_crabsynth.cpp` so a charge can actually inflict **server-side `DMG_CRUSH`** damage when its registered target is alive and visible. The original code only logged "Head On"/"Hit left"/"Hit Right"; it never damaged the victim, and its right-hand branch selected the *left* animation.
* Added a C++98 charge-geometry policy with explicit forward/lateral distances: the target must be ahead of the Crab Synth and inside the configurable source constants for reach and width. Targets behind the creature or outside charge width cannot trigger a hit. Charge misses stop instead of delivering damage.
* Added `sk_crabsynth_charge_dmg` (default `35`, `0` for animation-only testing, values above `1000` ignored). Damage is applied once at the charge-to-hit transition, with force, and the correct left/right animation schedule is selected. Engine visibility checks prevent damage through an occluding wall.
* Corrected another pathfinding edge case: a missing enemy or a failed navigation goal now fails the task without continuing charge movement or calling `TaskComplete()` afterwards.
* Added `modernize/patch_crabsynth.py`, `crabsynth_charge_policy.h`, and C++98 hit-geometry tests; included the patch in `apply_modernization.py`. The cut-enemy inventory records `charge_hit_source_patch` for Crab Synth but **continues to record `runtime_verified=false` and `assets_verified=false`**.
* **Still missing:** confirmed `models/synth.mdl`, compatible attack animations and sound scripts, navigation gameplay tests, full physics collision tuning, and Windows compilation. This is not a completed NPC restoration.

**In-game test after a successful Windows build and verified assets:**

```text
sv_cheats 1
npc_allow_unprecached_create 1
sk_crabsynth_charge_dmg 35
npc_create npc_crabsynth
```

Use a disposable Hammer map. Do not enable `npc_allow_unprecached_create` on a public server.

### v12: Houndeye and Bullsquid combat restoration (Phase 6, experimental)

* `dlls/hl2_dll/npc_houndeye.cpp`: changed placeholder `sk_Houndeye_health=0`
  and `sk_Houndeye_dmg_blast=0` to tunable defaults of **60 HP** and
  **25 sonic damage**. Sonic cone attenuation is now bounded to [0,1]
  and rejects invalid inputs. The original player-only partial damage through
  walls remains, as do existing animation and sound effects.
* Corrected a missing `return` in Houndeye group-attack strafe selection,
  registered the missing group-rally AI condition, respected the sonic attack
  cooldown at close range, guarded failed `energy_wave` creation and an
  absent hull-trace entity, initialized the attack timer, and precached the
  actual wave class. The new `modernize/houndeye_sonic_policy.h` is C++98.
* `dlls/hl2_dll/npc_bullsquid.cpp`: changed placeholder 0 HP, bite damage,
  and tail-whip damage to tunable defaults of **70 / 20 / 35**. The original
  tail-whip damage code was disabled; added actual server-side trace-hull
  damage and knockback to the existing `BSQUID_AE_WHIP_SND` animation event.
  No new unknown animation event identifiers are assumed. The spit projectile
  allocation is null-checked, the melee check handles a missing enemy, and
  angular tumble RNG limits were corrected.
* Fixed five obsolete `monster_headcrab` classname comparisons in Bullsquid
  AI. The 2003 engine registers `npc_headcrab` and other variants; using
  `CLASS_HEADCRAB` restores behavior for all of them. Added null checks for
  attacking entities and new-enemy schedules.
* The two idempotent source patches are `patch_houndeye.py` and
  `patch_bullsquid.py`. A portable native C++98 smoke test covers sonic
  falloff/damage geometry, with source-wiring, asset-status and idempotence
  regression tests. `cut_npc_inventory.json` records source-level progress
  without asserting that the enemies are playable.
* **Unverified:** these NPC models, activities, `energy_wave` entity effects,
  scripted sounds and maps still require Windows compilation and in-engine
  testing. Their default stats are restoration choices, not verified Valve
  balance data, and can be adjusted using the existing `sk_` console variables.

**Example console values for a development map:**

```text
sv_cheats 1
npc_allow_unprecached_create 1
sk_Houndeye_health 60
sk_Houndeye_dmg_blast 25
sk_bullsquid_health 70
sk_bullsquid_dmg_bite 20
sk_bullsquid_dmg_whip 35
npc_create npc_houndeye
npc_create npc_bullsquid
```

### Known blockers and phase completion state

| Phase | Status | What is still needed |
|---|---|---|
| 1. VS2026 migration | **Structural conversion and shader custom build translation done, compilation unverified** | Repair missing include paths/libraries, compile and fix C++/SDK errors, test Windows x86 builds |
| 2. Engine modernization | **Partial: bounded directory path handling; not compile-verified** | Launch and crash testing, fix startup, dependencies, runtime CRT, modern Windows issues, eventually x64 if feasible |
| 3. Rendering Vulkan / OpenGL / DX11 / DX12 | **Selection hooks + standalone GPU capability probe only** | Implement actual `IShaderAPI` backends, GPU resource/state abstraction, shader translation, presentation, synchronization, caps; existing DX8/DX9 remains |
| 4. Modern lighting/shaders | **DX9 Fresnel and gamma fixes, animated-light cache fix and optional smoothing; experimental cross-API tone mapping sources** | Compile and visually verify DX9 changes, wire HDR scene textures and tone mapping into real render backends, dynamic/static lights, shadow pass, regression maps |
| 5. Steam/controller | **Partial: Steamworks client bootstrap and XInput** | XInput bridge, sticky-button fix, adjustable radial deadzones; runtime verification, Steam Input, official Steamworks SDK + app ID, API startup/shutdown, auth and matchmaking needed |
| 6. Cut enemies | **Partial** | Experimental spawn option, Hammer definitions, and unpacked model-file audit only; missing/disabled AI implementation, asset sourcing, precache, networking and per-NPC tests required |
| 7. Fully featured multiplayer | **Chat hardening, fair spawns, optional spawn protection, 64-slot map voting, optional team balancing, safer map rotation/MOTD/kill notices, and test scaffolding** | Dedicated server builds, map compilation, in-game multiplayer testing, co-op replication, additional modes, anti-cheat, Steam sessions, QA |

### Why the build is not declared finished

Across the entire 153-project archive, there are **606 nonexistent file/path references in 67 projects** according to the original structural scan. The more precise *active-HL2* audit shows **0 missing compiled-source references** among 61 steps per build mode, but it still detects 20/17 missing include-directory references and a number of absent listed headers, resources, and external SDK paths. This environment does **not** have Windows MSBuild, the applicable proprietary SDKs, Steamworks authorization, HL2 game assets, or the runtime test setup. No feature is marked runtime-verified.

Steam platform integration requires the project owner's valid Steamworks credentials and an App ID. Do not use third-party or unauthorized Steam SDK binaries. No licensed Valve source, DLL, or Windows/DirectX SDK binary is bundled with this overlay.

## Useful commands

```bat
py -3 modernize\apply_modernization.py
py -3 modernize\build_audit.py
py -3 modernize\npc_asset_audit.py --assets-root C:\my\unpacked\hl2 --out modernize\my_npc_asset_report.json
py -3 modernize\validate.py
py -3 modernize\build_driver.py Debug --dry-run
modernize\build_with_logs.cmd Debug
py -3 -m unittest discover -s modernize\tests -v
modernize\build_windows.cmd Debug
modernize\build_windows.cmd Release
```

Python 3.9+ recommended. The scripts are idempotent for repeat runs; keep version-control backups because generation **overwrites** generated project files. All migration code is in `modernize/`.


## v13 — branch-separated weapon inventory and Multiplayer model selector

The seven-phase plan has been expanded to **eight** phases: #6 cut-NPC restoration; **#7 weapon completion, `impulse 101` per game branch, and weapon model/material/HUD auditing**; **#8 multiplayer** (including the new player-model chooser and a future live 3D preview). The original phases #1–#5 are unchanged.

### Implemented in this source snapshot

* HL2 now exposes the existing Multiplayer options tab even if old `liblist.gam` metadata says single-player-only. HL1, CS and TF2 keep their original Options-page rules.
* The HL2 model dropdown is populated only with known HL2 models **actually installed** under a finite server/client allowlist. It writes the established archived userinfo `model` setting when Apply is pressed. The HL2 server validates the request and applies a valid installed model at respawn; invalid paths fall back to `models/player.mdl`. No CS/TF2 model is selectable in the HL2-specific list.
* `modernize/hl2_playermodel_policy.h` is shared by the HL2 GameUI and server code. `modernize/tests/playermodel_policy_smoke.cpp` tests the allowlist and branch detection in portable C++98.
* `modernize/weapon_branch_audit.py` creates `weapon_branch_inventory.json` from each branch's source-registered weapon classes. It can optionally check asset files in an authorized local game asset directory with `--game-assets PATH`. It intentionally distinguishes registrations from usable weapons and does not invent view-model paths or icon names.

### Still required

* **Live 3D preview:** the original Options page only has `CBitmapImagePanel` (legacy bitmap remapping). The gameplay renderer is not exposed through the GameUI interface. An off-screen client studio model renderer, render target, animation/rotation input, and UI composition are still needed. This version does **not** represent the existing static image as a live preview.
* **Phase 7:** enumerate actual active per-branch build configurations, repair incomplete weapons, validate each viewmodel/worldmodel, texture scale and HUD icon against real game assets, then implement fully audited per-branch `impulse 101` granting. The inventory is a starting list, not a claim that every weapon already works.
* All changes require a successful Windows build and game/runtime testing. No game assets or proprietary Steam SDK binaries are included.


### v13: implemented branch-specific impulse 101 grants and responsive portraits

* `modernize/patch_impulse101_branches.py` adds a cheat-controlled `sv_impulse101_allweapons 1` switch (default enabled). HL2's override first preserves the generic suit/ammo/standard loadout, then grants every additionally registered HL2 weapon. Counter-Strike and TF2 have separate extra lists in their own `CheatImpulseCommands` override. The preexisting HL1 branch override remains unchanged. Registered abstract weapon bases are deliberately excluded as they are not equipable weapons; see `modernize/weapon_impulse101_manifest.json` for exact per-branch lists and exclusions.
* Branch-specific `LINK_ENTITY_TO_CLASS` registrations are **not proof the weapons are asset-complete**. The extra grants can expose broken prototype weapons and may fail without view models/materials. Run `py -3 modernize/weapon_branch_audit.py --game-assets YOUR_EXTRACTED_GAME_ASSETS` to inspect source-literal model paths and weapon scripts. Final textures, viewmodel selection, accurate scale and HUD art are still to be reconstructed and checked.
* The HL2 Multiplayer options page now uses its existing `model` userinfo cvar with a finite list of installed HL2-compatible model files. Source checks validate selected paths on the HL2 server and apply the model on respawn; TF2/CS are unaffected. `gameui/LabeledCommandComboBox.cpp` notifies its parent on selection so the portrait/status updates immediately.
* Optional portrait materials live at `materials/vgui/playermodels/<portrait>.vmt` (using the IDs in `hl2_playermodel_policy.h`). When a matching VGUI material exists it updates on selection, with aspect-preserving scaling from `gameui/BitmapImagePanel.cpp`. These are **live-updating 2D portraits**. A fully animated, rotating 3D preview is **not yet implemented** because the GameUI build has no studio-model renderer bridge. No missing portraits are fabricated.
* `modernize/tests/test_model_weapons.py` plus portable C++98 tests cover branch isolation, model allowlist safety, source wiring, and repeatable patch application. All of this is **source-tested only**, not compiled inside a Windows Source engine runtime.

### Phase 7: weapon-selection HUD aspect correction

`cl_dll/hl2_hud/hud_weaponselection.cpp` now uses the selected weapon's script-driven active icon (or its inactive icon as a fallback), preserving source aspect ratio and preventing oversized art from overflowing its HUD selection box. `modernize/weapon_hud_layout.h` is a portable C++98 helper with boundary tests. This is an **HL2 HUD rendering fix**, not a replacement for per-weapon missing art or a fix to 3D model material UVs/viewmodel scale. CS/TF2 use separate HUD pipelines and require independent verification. A valid weapon script, model asset, texture and sprite must still exist for each weapon.

## v14: Weapon-script integrity, asset validation and safer HUD metadata

This pass modifies **actual legacy source** in two additional files:

* `cl_dll/hud.cpp`: bounded weapon/HUD sprite names and material identifiers, plus validation of atlas coordinates before creating HUD textures. Oversized, negative and zero-sized records are discarded rather than copied into 64-byte arrays or given malformed rectangles. The HUD continues loading other usable sprites.
* `game_shared/weapon_parse.cpp`: clamp script-configured `bucket` and `bucket_position` to the original inventory-array sizes before the debug and runtime HUD paths use them. Out-of-range cut-weapon script positions cannot index beyond the arrays.

`modernize/patch_weapon_asset_safety.py` reproduces those C++ changes against the untouched original files, and is called by `apply_modernization.py`.

`modernize/weapon_asset_audit.py` reads weapon scripts in **separate branch asset roots** and checks declared view models, world models, optional split-model companion files, HUD VMT materials, `$basetexture` VTF textures, the VTF header dimensions and each atlas region's fit within the actual texture. It performs path validation and does not load arbitrary absolute or traversal paths. Example usage after *legally obtaining and unpacking* the corresponding game assets:

```bat
py -3 modernize\weapon_asset_audit.py --assets-hl2 C:\Games\HL2_extracted\hl2 --assets-cstrike C:\Games\CS_extracted\cstrike --assets-tf2 C:\Games\TF2_extracted\tf2 --output modernize\weapon_asset_report.json
```

The output is per weapon and per game branch, distinguishing **unverified** (no asset directory supplied) from **missing** (asset directory supplied and file absent). This source archive has no unpacked game asset set, so the shipped `modernize/weapon_asset_report.json` records **110 registered weapon candidates as unverified**, not 110 missing weapons. Optional model companion file presence does not establish whether the particular historic MDL format needs them. Asset presence cannot establish material UV scale, animation compatibility, correct first-person holding position, or gameplay readiness.

Six new tests using synthetic asset files cover KeyValues parsing, game-branch isolation, malicious paths, atlas bounds, unprovided assets, and the idempotent source patch. With the existing test suite, **73 regression tests pass under Linux**, and the 153 generated Visual Studio projects still pass XML/structural validation. This does not constitute a successful Windows compile.

**Remaining:** repair each prototype's missing/incorrect actual art with properly authorized content; compile and test each weapon's viewmodel, worldmodel, icons, animations and proportions in-game. Integrate a real studio-model offscreen renderer for the requested animated 3D Multiplayer model preview. Add functioning, tested Vulkan/OpenGL/DX11/DX12 backends, complete Steamworks networking, restore additional cut enemies, and perform full engine/multiplayer Windows QA.

### v15: Build-aware weapon grants and multiplayer model-picker reliability

* The new `modernize/weapon_build_audit.py` compares the *registered weapon classes* with `dlls/hl.vcxproj` **Debug and Release server build inputs**, separately for HL2, HL1, Counter-Strike and TF2. The output is `modernize/weapon_build_report.json`. Matching C++ source file names uses Windows case-insensitive semantics; client-side inclusion of the same source is informational only and is not automatically required for every game. **This is not a successful compiler or runtime check.**
* The audit found **six registered TF2 server weapons whose source files are absent from the active TF2 project configurations**: `weapon_combat_chargeableplasma`, `weapon_infiltrator`, `weapon_laserdesignator`, `weapon_laserrifle`, `weapon_mortar`, `weapon_pistols`. Existing `impulse 101` name lists cannot guarantee those classes are registered in a built server until the files are included and compiled.
* An **opt-in build experiment** is available. After generating the projects, run `py -3 modernize/enable_tf2_prototype_weapon_sources.py --enable` to insert these six files only into the converted **TF2 server project**. No cross-branch references are added. The tool leaves normal builds unchanged until deliberately enabled; the underlying project converter will overwrite this optional change if you regenerate the `.vcxproj`. The experimental files may fail to compile because their dependencies and network classes remain unverified. This does **not** repair their missing assets, animations, textures or HUD icons.
* The 2003 `CLabeledCommandComboBox::SetInitialItem` accidentally passed a VGUI ComboBox ID where its own item index was required. This is corrected, along with invalid-index checking, reset state, bounded model names and console commands, handling of empty model selections, and stale portrait texture cleanup. The implementation is reproducible through `modernize/patch_multiplayer_model_ui_safety.py`.
* A **true live animated 3D model preview** remains unfinished: the existing Options menu currently displays portrait materials when installed. Rendering a 3D MDL in GameUI requires additional client renderer integration.
* The package remains a **source-code milestone**. Windows compilation, actual weapon asset validation and in-game testing have not been performed.

### v16: Two-phase focused pass — weapon reliability (7) and multiplayer model choices (8)

**Build work deliberately deferred per request.** No VS project inputs were expanded, no missing TF2 prototype sources were force-enabled, and no Windows compilation was attempted.

* Phase 7: `modernize/patch_grants_buildaware.py` now restricts the experimental `impulse 101` extension to weapons whose implementing source is included in that branch's existing Debug **and** Release server projects. It excludes HL2's registered-but-uncompiled `weapon_brickbat` and six TF2 prototype weapons from the emitted cheat arrays. The base `impulse 101` logic is unchanged. The regeneration process updates its per-branch weapon manifest with explicit excluded names. This does not fix the excluded weapons or their art.
* Phase 7: HL2's weapon HUD now displays a neutral `WPN` label if **both** weapon-specific sprite options are missing or have unusable dimensions. It never borrows another weapon's sprite. Existing valid HUD art keeps aspect-preserving sizing and the actual weapon's existing name. View/world models and textures are **not** repaired or verified in-game.
* Phase 8: Enlarged the HL2-only, finite allowlist to 46 potential humanoid, Combine, and Civil Protection model paths. The client only lists models whose MDL files are actually installed; the server only accepts the allowlisted installed paths. No HL1/CS/TF2 model can be selected through this HL2 menu.
* Phase 8: The Options page now explicitly reports when a selected model lacks an installed portrait, instead of silently presenting an empty image as if it were a 3D renderer. Portrait choice updates immediately; application continues through the original `model` USERINFO cvar. **An animated engine-rendered 3D preview is still not implemented.** The available archive has no matching model/media assets for a visual test.
* `modernize/tests/test_phase7_phase8_v16.py` adds eight focused tests, including a native C++98 policy compile, per-branch grant coverage, icon fallback source checks, and script idempotence. Existing regression tests were adjusted to reflect intentionally skipped noncompiled registered weapons.

**Completion caveat:** These are source-level Phase 7/8 improvements, not two completed and verified product phases. To finish Phase 7 requires the actual weapon scripts, textures, models, animations, and live testing for each branch. To finish Phase 8 requires the requested engine-rendered 3D preview and multiplayer session validation. No credible "phase complete" claim is possible before those requirements are met.

### v17: Model selection integrity and weapon-grant failure reporting

* Fixed a remaining out-of-bounds command lookup when the legacy VGUI dropdown selection index is stale; unknown labels now invalidate the selection instead of retaining a previous model command.
* The Multiplayer model portrait is hidden as soon as the selected model is invalid, rather than displaying a portrait from an earlier selection. The preview remains **2D**, not an animated 3D model.
* HL2/HL2MP branch detection now accepts case-insensitive directory names and trailing forward or backslashes. TF2/HL1/Counter-Strike model paths remain excluded.
* The existing, branch-specific `impulse 101` prototype-grant loops now count and report successful, already-owned, and failed weapon acquisitions using player inventory checks. This does not complete missing weapon scripts, textures, scaling, viewmodels, or game-specific restrictions.
* Added portable C++98 and source regression tests. The project was **not** compiled with Visual Studio or launched in-game. No build project changes were made.

### v20: 3D preview state isolation and branch-specific weapon geometry preflight

* Added a scoped restore of projection/view/model matrices and fog mode around the animated Multiplayer preview's studio renderer, reducing risk that the preview modifies subsequent Options-menu drawing.
* The preview, HL2 player model precache, and cheat weapon-grant validation share a bounded validator for the actual 2003 studio vertex companion paths (`.dx80.vtx`, `.dx7_3bone.vtx`, `.dx7_2bone.vtx`). A mere MDL filename or header is no longer enough. No conversion from retail v37 models is claimed.
* The new validation cannot prove meshes, UVs, materials, animations, or runtime behavior; it intentionally rejects unported prototype assets. Ordinary weapon grants were not changed.
* Host regression suite: 153 tests run, 152 passed, one skipped; Visual Studio project structure: 153 projects and 348 configurations; full Windows compilation is deferred.
* The v20 overlay applies only to the exact v19 source ZIP. See `modernize/reports/RELEASE_NOTES_V20.md`.

## v21: guarded studio MDL 36/37 compatibility and cross-branch asset preflight

The **original 2003 v36 loader is unchanged** for files reporting studio version 36.
For files reporting version 37, `engine/l_studio.cpp` now validates the entire
MDL's v36-compatible bone, animation, sequence, mesh and material tables before
normalizing the *in-memory copy* for this engine's legacy consumers. The
original bytes on disk retain version 37. The preflight additionally requires
an existing 2003 `OptimizedModel` **VTX version 6** companion with the same MDL
checksum and a bounded bodypart table. It will reject later VTX version 7,
unsupported changes to the v37 animation/layout structures, and missing vertex
sidecars. These restrictions prevent version-number hacks that could crash the
old model renderer. This is not support for every later v37 variant.

Model policy is shared with the multiplayer 3D preview, HL2 server player model
selection, and experimental `impulse 101` weapon grants. Static-prop models
with no animation/bones can pass the studio validation stage (the multiplayer
portrait is still for animated character models only).

### Offline model inventory (phases 6, 7 and 8)

Run against separate, legally supplied game-content directories:

```text
python modernize/scan_studio_v21.py --root hl2=/path/to/hl2 --root hl1=/path/to/hl1 --root tf2=/path/to/tf2 --output studio_asset_scan_v21.json
```

The scanner examines v36/v37 MDL metadata, companion VTX v6 versions, matching
checksums and table boundaries. Its `2003_loader_candidate` state means **only
that these file-header checks passed**. It cannot validate an entire studio
layout, material appearance, animation rig or runtime drawing. Game-content
archives are not included. A full Windows/Win32 build and representative real
v36/v37 models are required to certify runtime behavior.

### Build and verification boundary

The test suite runs portable C++98 MDL/VTX header fixtures, Python scanner
fixtures and existing source-policy checks. The complete 2003 `studio.h`
validator is integrated into the Windows engine source, but cannot be natively
compiled by the Linux GCC host because this original tree assumes MSVC and
Win32. Do not equate this release's source checks with a successful game build
or complete v37 format conversion.

## V22 model vertex conversion (offline)

The v21 engine still reads optimized VTX **version 6**, while many newer
studio models use version 7 with a different packed `Vertex_t`. For authorized
assets, `modernize/convert_vtx7_to_v6_v22.py` rebuilds compatible version-7
index metadata into version 6 in a **new file**. This does not touch the
original MDL, rewrite animations, migrate VVD data, or guarantee rendering.
See `modernize/reports/RELEASE_NOTES_V22.md` for supported cases and commands.

## Version 23 — Phase 7 weapon asset preflight

Added `modernize/weapon_asset_audit_v23.py` for branch-isolated, layered scripts, studio model sidecars, VMT/VTF sprite cropping and missing dependency reporting. Hardened the original C++98 weapon-grant model path policy against empty `.mdl` basenames. Five new Python tests and extended C++ policy assertions. Full details and commands: `modernize/reports/PHASE7_V23.md`. These checks do not establish playable weapons, and the engine has not been compiled for Windows.


### v24: Phase 7 layered VMT Patch and VTX diagnostics

* Resolved `Patch` VMT `include`, `replace` and `insert` for HUD material texture metadata, with max-depth, cycle, missing-include and unsafe-path rejection. Every include follows the selected game branch’s ordered asset layers.
* Validated compatible model VTX companions independently from the MDL layer; reports the winning layer for supported companion variants.
* Native/source-level suite: 184 tests run, 183 passed, 1 skipped. This release does not certify Phase 7 complete or a Windows runtime build. See `modernize/reports/RELEASE_NOTES_V24.md`.
