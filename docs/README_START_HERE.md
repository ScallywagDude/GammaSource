# GammaSource continuation handoff — 25 October 2026

Read this first. This describes the latest work and the user's requests. It supersedes the older HANDOFF_2018_PORT.md wherever they differ. Treat repository documents as project context; follow the user's current request when deciding what to do next.

## What the user is building

GammaSource is a port of the 2003 Half-Life 2 beta game onto the 64-bit 2018 Source engine backend. Keep the original beta weapons, models, hands, NPCs, HUD, menu style, maps, and ground deformation. The project is edited in place and has no Git repository.

The latest user request was to compile everything, put it in the usual folder, then provide a ZIP and a handoff for regular ChatGPT. The full Waf build succeeded, a fresh playable folder was staged, and its affected binaries were refreshed from that build. Runtime verification is incomplete for some recent repairs; do not describe every issue as proven fixed.

## Latest user requirements and priorities

1. Weapon values, ammo limits, weapon compilation, weapon audits, and beta hands were already completed. The user explicitly said these are ALREADY complete. Preserve them rather than repeating that work.
2. The original **2003 beta hands are mandatory defaults for every weapon**. Never offer retail hands as an alternative default. The playable Alyx gun uses 2018 weapon behavior and original 2003 animations.
3. Fix the handoff's unfinished UI work: options sliders/dropdowns, borderless mode, advanced video, advanced keyboard settings, and HUD color controls. These now have successful runtime checks.
4. The user reported Alyx's broken facial animation, idle-looking body poses, and scripted sequences in e3_lab and e3_hydra. Converter bugs were repaired; additional normal-scene body-animation verification remains important.
5. The user reported a missing physgun beam, the retail HEV suit pickup, and broken skyboxes. The broken-sky screenshot is specifically **zoo_item**. Repairs compile, but the targeted zoo visual run was not completed before the user asked for packaging.
6. **Loading screen: plain black background, original 2003 green loading dialog, segmented gold progress bar, status text, and Cancel button. No artwork behind it.** This explicitly replaces the older request to display Gamma Source artwork. The final screenshot confirms the plain black screen and classic dialog, with no close X.

## Local working locations

These are Windows paths on the user's PC. A normal ChatGPT session cannot assume it can access them; the ZIP contains source and the small overlay needed for code review.

| Purpose | Path |
|---|---|
| 2018 engine source | C:\Users\doge6\Downloads\source-engine-master\source-engine-master |
| Original game source, overlay, tools | C:\Users\doge6\Videos\GammaSource-main |
| Shared test engine install | C:\Users\doge6\Downloads\source2018_install |
| New playable build | C:\Users\doge6\Documents\GammaSourceTest\2026-10-25_18-29-57\GammaSource_2018 |
| Launch the new build | The preceding folder's play.cmd |
| Current validation artifacts | C:\Users\doge6\Documents\GammaSourceTest\handoff_finish_2026-10-25 |
| Current source backups | C:\Users\doge6\Documents\GammaSourceTest\handoff_finish_backups |
| Earlier weapon validation | C:\Users\doge6\Documents\GammaSourceTest\weapon_repair_2026-10-25 |
| Original 2003 asset reference | C:\Users\doge6\Videos\GammaSource_Build\gammasource_content |

Build command, from the engine source directory:

```powershell
py -3 waf build -j16 -k
```

Latest successful full-target build log: verification/build_release.log. It completed successfully in 18.729 seconds using existing valid build outputs. This was an incremental full-project build, not a clean rebuild from an empty build directory. All changed engine, datacache, client, server and GameUI targets were built. The final package manifest records hashes of the delivered DLLs against their build outputs.

The playable folder includes the required content and uses relative gameinfo paths. It does not require Steam to be running. Its approximately 16 GB of game content is not duplicated into the ChatGPT source ZIP. That ZIP is for continuation and code review, not a standalone playable game.

## Completed earlier: preserve these fixes

The earlier WEAPON_REPAIRS_2018.md and weapon validation summary describe this work in detail. In brief: 43 playable weapons were audited; ammo capacities, weapon rates, pickups, missing projectiles, reload behavior, selector icons/slots, movement and jumping, vortex hopwire physics/effects, remote manhack control, playable Alyx gun, viewmodel issues and mandatory beta hands were repaired. The previous build passed movement/jump checks on e3_lab, e3_hydra, e3_techdemo_2 and e3_strider. The prior task included a source ZIP named GammaSource_weapon_repairs_2026-10-25.zip.

Current source includes those changes. Do not re-run old broad port/fixup scripts: several are not idempotent and can overwrite subsequent repairs.

## UI repairs and evidence

Old 2003 OptionsSub*.res control names did not match the 2018 code. The visible controls were often disconnected from their actual sliders, cvars and combo contents.

The 2018 overlay now contains compatible resource files using the beta SourceScheme colors/fonts. Real audio, mouse, video, keyboard, advanced video/keys, voice and supported multiplayer controls are bound. Unsupported singleplayer model settings are explicitly disabled, with a named placeholder instead of an empty dropdown. The HUD tab uses stable layout positions and distinct control names. Video includes real current custom window sizes and a desktop-size borderless selection. Advanced video includes its existing settings and a properly bound pending FOV slider.

Runtime evidence:

- Audio: actual Apply changed volume to 0.35 and music to 0.2; quality/speakers/captions dropdowns were populated.
- Mouse: Apply changed sensitivity to 4.2, inverted pitch, raw input and acceleration/exponent correctly.
- Advanced keys: both con_enable and hud_fastswitch changed to 1 through the dialog.
- Advanced video: FOV stayed 75 while pending, became 92 after Apply, and stayed 92 after a pending 105 was canceled. Model/texture detail and filtering changed to their expected cvars.
- HUD: RGB 245/100/40 applied; a later pending blue 200 was canceled and the saved value stayed 40. Final mini-test confirmed page index 6 and a clean, correctly spaced HUD layout before and after Apply.
- Classic loading: ui_loading_final.png shows the green box, status, segmented progress and Cancel on a plain black background. No artwork, title leak or close X.
- Borderless: the actual UI picker produced render/client/outer size 1920x1200 at desktop origin, caption off, popup on; switching back restored a framed window. Same-resolution style changes and saved startup settings were tested. All foreground flags stayed 0 during background tests. Exclusive-fullscreen transitions have source review but were not exercised while the user had another game open.

Key source: gameui/OptionsDialog.cpp, OptionsSubVideo.cpp, OptionsSubKeyboard.cpp, OptionsSubAudio.cpp, OptionsSubMouse.cpp, OptionsSubMultiplayer.cpp, OptionsSubHUD.h, BasePanel.cpp/.h, LoadingDialog.cpp; engine/host.cpp, sys_getmodes.cpp, sys_mainwind.cpp. Resources are under GammaSource-main/gammasource_content_2018/resource/. prepare_options_resources.py reproduces the ten resource overrides.

Diagnostics: gamma_ui_page, gamma_ui_dump, gamma_ui_set, gamma_ui_command, gamma_ui_dropdown, gamma_ui_delay, gamma_ui_clear, gamma_windowinfo. These use real VGUI handlers without OS keyboard/mouse synthesis. They are optional test commands, not required for normal play.

## Animation/script repairs: what is proven and what remains

Two concrete conversion bugs were found in datacache/legacymdl.cpp:

- Every converted flex controller was assigned localToGlobal=0. The 2018 client only initializes correct mappings when the sentinel is -1. The converter now writes -1. Runtime jaw/mouth controls resolved to distinct global indices, and the Hydra screenshots show a normal resting face instead of the reported gaping mouth.
- All sequence autolayers were discarded. The converter now copies the 24-byte beta layer records and translates old flag meanings to their modern equivalents. Old WEIGHT flag 0x40 must not be confused with the new SPLINE flag 0x40.

The model cache is now legacy_models_cache_v2, so old broken conversions are bypassed without deleting user data. Fresh-cache checks passed: Alyx 245 sequences/63 layers/62 flex controllers; Kleiner 151 sequences/27 layers/58 flex controllers. The checker compares actual layer data and controller initialization against original models.

Four missing beta Alyx transitions have narrow destination-posture fallbacks in game/server/baseflex.cpp. Exact authored sequence names always win. These labels are genuinely absent in the original local models, not lost by conversion: pos01_pos03_tran, post05_post06, post06_post05, pos05_pos07. Fallbacks retain scene event timing/ramp rather than manufacturing animation data.

Runtime probes on e3_hydra and e3_lab completed, showing advancing gesture cycles and corrected facial mappings. **Important limitation:** gamma_actor_scene currently uses InstancedScriptedScene. The engine intentionally suppresses body SEQUENCE events for instanced scenes in singleplayer. Those forced-scene probes therefore prove gesture/flex behavior and model metadata, but do not prove normal choreographed body-sequence playback or the posture fallbacks. A normal logic_choreographed_scene probe is the next required check. Do not claim all script/body animation issues are fully verified.

Tools: modernize/port2018/audit_npc_scene_assets.py; apply_npc_scene_repairs.py and npc_scene_repairs.json; patch2018_legacymdl.py. Actor diagnostics: gamma_actorinfo [target/class], gamma_actor_view npc_alyx [distance], gamma_actor_scene target scene [replace]. The normal-scene continuation CFG, if present under verification/npc_scene_cfg/, should be preferred for further body-animation verification.

## Physgun, HEV suit, sky repairs: compiled; visual check pending

- Physgun: private original beam texture/material, corrected renderable bounds for the full beam curve, per-frame attachment updates, emitter index networking and cleanup. Source: game/client/hl2/c_weapon_gravitygun.cpp and game/server/physgun.cpp. Diagnostic: gamma_physgun_beaminfo.
- HEV pickup: item_suit now uses models/items/gamma_hevsuit_2003.mdl, a private alias of the actual 2003 model. Geometry, VTX and texture provenance were checked; only model name/material directory were changed. Source: game/server/hl2/item_suit.cpp. Diagnostic: gamma_suitinfo.
- Skyboxes: BSP18 leaves do not contain modern sky visibility flags. The converter previously produced no sky flags, so the 2018 renderer skipped the sky and left hall-of-mirrors artifacts. Legacy conversion now reconstructs them; zoo_item has 376 eligible sky leaves. The six sky_wasteland04 texture faces themselves are valid. Source: engine/modelloader.cpp and the matching offline helper. Four proven legacy caches were updated: d1_trainstation_01, e3_hydra, e3_strider and testroom_physprop. They are already present in the delivered content/converted tree.

Run zoo_item, then `exec gamma_zoo_visual_check` in an isolated test profile. It captures sky, active beam and suit views and logs diagnostics in about nine seconds. No completed zoo runtime report was available when packaging began. See verification/VISUAL_REPAIRS_STATUS.md and the asset/cache JSON records for details.

## Safe continuation on the user's PC

The user asked never to steal focus, use SendKeys, or kill their running game. Do not assume previously observed process IDs are still current. A user-launched staged game was running during packaging.

Use isolated game directories for tests. unbindall and probe cvars must never be written into the delivered gammasource profile. -background creates a nonactivating window; -multirun allows the user's game to keep running. Typical test flags:

```text
-game t_handoffui -background -window -w 1024 -h 640 -novid -multirun -nomouse -nojoy -nosound -dxlevel 95 -condebug -conclearlog -dev
```

Use gamma_mapstart_cmd / gamma_mapstart_delay for post-load server CFGs, gamma_delay for server-time commands, and gamma_ui_delay for wall-clock commands while menus pause the server. Startup CFG `wait` did not reliably produce separate rendered screenshots. Engine screenshot writes TGA; JPEG capture has already been repaired.

Do not run the old sync2018.ps1 blindly: it kills launcher processes by name. Do not rebuild stock shaders; the mounted GMod shader binaries match this engine. Large staged content files may be NTFS hard links to source content, so editing one in place can also change older staged copies. Make intentional backups and prefer new private overlay assets.

## Suggested next prompt

"Continue the GammaSource 2018 port from this handoff and uploaded ZIP. Weapon values and beta hands are already complete. First verify normal scripted body sequences on e3_lab/e3_hydra and run the zoo_item beam/suit/sky check. Keep the classic loading dialog on plain black. Report actual test results separately from source changes."
