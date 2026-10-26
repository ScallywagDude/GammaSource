# GammaSource engine port: handoff (2026-10-25)

This is for the next agent picking up the work. Read `PORT_2018_BACKEND.md` in this repo as well: it has the full plan, the milestone notes and the gotchas.

## What this project is

- GammaSource is the 2003 Half-Life 2 beta engine and game, made buildable.
  - Repo: `C:\Users\doge6\Videos\GammaSource-main`. It is not a git repo.
- The user wants the game moved onto the 2018 "nillerusr" source-engine backend (64-bit, newer renderer), keeping:
  - the 2003 frontend: HUD, menus, static menu background;
  - every 2003 weapon, NPC and entity;
  - ground (displacement) deformation;
  - the four E3 maps working.
- 2018 tree: `C:\Users\doge6\Downloads\source-engine-master\source-engine-master`. Also not git; edited in place.
- Rules from the user:
  - Never push, merge or open a PR without asking.
  - Never steal focus or use SendKeys.
  - Never kill the user's running game (`Videos\GammaSource_Build\hl2.exe`, or a staged build).
  - Keep working without stopping unless told.
  - Put every build in `C:\Users\doge6\Documents\GammaSourceTest\<yyyy-MM-dd_HH-mm-ss>`.

## Build, run and test loop

1. Build: in the 2018 tree, run `py -3 waf build -j16` (add `-k` to keep going on errors).
2. Copy the binaries into the test install: run `sync2018.ps1` from the scratchpad (included in the zip under `tools/`).
   - Test install: `C:\Users\doge6\Downloads\source2018_install`. The test game dir is `t_legacy`; its `gameinfo.txt` mounts the content layers.
3. Run and screenshot without stealing focus: `tools\shot2018.ps1 -Game t_legacy -Extra '-multirun -dxlevel 95 -condebug +exec <cfg>' -Out x.png -Wait 45 -RestoreAt 12`.
   - `-multirun` is required while the user's game is open (single-instance check).
   - `-Install <dir>` points the script at a staged build instead.
4. Drive a test from a cfg:
   - `gamma_mapstart_cmd "exec gamma_tune.cfg"` runs a cfg after the map starts.
   - `gamma_delay <sec> <cmd>` runs a command as the local player later.
   - The `screenshot` console command saves TGAs to `t_legacy\screenshots`; `tools\sheet.ps1` makes contact sheets.
5. Debug a crash: `tools\minidbg.py <cwd> <exe> args` gives a stack.
   - Symbolize with the VS llvm-symbolizer: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe`.
   - Copy the matching `.pdb` from `build\...` next to the DLL first.
6. Stage a release: `modernize\port2018\make_build.ps1` (`-Include2003` also copies the 2003 build).
   - It is Steam-free: the retail HL2/TF2/Portal VPKs are hard-linked under `content\` and `gameinfo.txt` uses relative paths only.
   - Last good staged build: `Documents\GammaSourceTest\2026-10-25_15-11-31`. It predates the portal gun and the fixes below.
   - A second staging (`2026-10-25_16-2x`) ran after; check whether it completed (its `gammasource\gameinfo.txt` exists) before relying on it.

## Content layers (mount order, `t_legacy\gameinfo.txt`)

1. `source2018_assets\gammashaders`: our shaders (Predator/JellyFish).
   - Do NOT rebuild stock shaders. GMod's `.vcs` files match this engine, and my sprite rebuild broke glows; it was reverted.
2. `source2018_assets\gmodshaders`: GMod's stock shaders.
3. `hl2\converted`
4. `GammaSource-main\gammasource_overlay`: everything new for the 2018 build. Scripts, merged sound manifest, particles manifest, beta-hands rules, icons, nodraw overrides.
5. `source2018_assets\gamma`
6. `GammaSource-main\gammasource_content`: the 2003 game content.
7. `Documents\HL2BetaMegaCuck\hl2_2003`
8. `source2018_assets` (root)
9. TF2 `hl2_misc.vpk`, then the HL2 VPKs, then the HL2 loose folder, then Portal's `portal_pak.vpk`.

## Done (all verified in game unless noted)

- 64-bit boot.
- BSP v18 loading.
- MDL v36/v37 converted at load time (`datacache/legacymdl.cpp`). Converted models are cached in `<mod>/legacy_models_cache`.
  - The queued loader now skips unconverted models, so they take the sync path.
- Ground deformation: engine, game, collision and save/load.
- 2003 weapons, NPCs and entities ported into `game/server|client/gamma`.
- 2003 HUD.
- Main menu matches 2003:
  - static `console/console_background`, items bottom-left, `resource/game_menu` title;
  - the CONSOLE item removed;
  - the `ValvePlatformUIMutex` wait removed, because it hid the menu while the 2003 build ran.
- Options dialog: the 2003 tabs, including Lock (content lock) and the HUD color tab.
  - "Borderless windowed" is in the video combo, backed by the engine convar `vid_borderless`.
  - The user says borderless is still broken (see TODO).
- `impulse 101` gives every `weapon_*` that has a script, plus maximum ammo.
- `weapon_vortexhopwire`: the Aftermath/episodic vortex hopwire with our models.
- `weapon_proto1`: a reconstruction, since Aftermath ships no source (`gamma/weapon_proto1.cpp`).
- Beta hands on retail viewmodels: `game/client/gamma/c_betahands.cpp` plus `scripts/gamma_betahands.txt`. The revolver alignment is tuned; the portal gun uses bone mapping.
- Portal gun: Portal-Base merged into the HL2 DLLs (`PORTAL;GAMMA_PORTALGUN`).
  - Patch scripts: `modernize/port2018/patch2018_portal.py` and `patch2018_portal_core.py`.
  - The player is now `CPortal_Player`. Both portals fire by default.
  - The HL2 ammo table is kept (`portal_gamerules` GetAmmoDef is `#ifndef GAMMA_PORTALGUN`).
  - Glow sprites use `sprites/portalgun_fx/*`.
- Selector icons: `make_icons.ps1` draws `sprites/w_icons5b`; `set_icons.py` points the scripts at it.
- `toolsnodraw` is `$no_draw` in the 2018 layer. It fixes the yellow NODRAW panels on brush entities. Not yet in a staged build.
- Scenes (just fixed, not yet staged), which caused the "animations and audio broken" report:
  - The 2018 scene cache only read `scenes.image`, so every 2003 `.vcd` was missing: Alyx/Kleiner didn't act or talk.
  - `scenefilecache/SceneFileCache.cpp` now reads loose `.vcd` first.
  - `game/server/sceneentity.cpp` parses text VCDs, precaches their speech (`PrecacheLooseScene`), and measures their duration.
  - Verified on e3_lab: speech plays.
  - Still logged: `unable to find sequence pos05_pos07 / pos01_pos03_tran / post05_post06` on Alyx. Our beta `models/Alyx.mdl` lacks them, so this is probably the same in the 2003 build; unverified.

## In progress when stopped: movement is broken (user report)

- Measured on e3_lab, e3_hydra and e3_techdemo_2 with `gamma_delay` driving `+forward` / `+jump`:
  - the server sees `IN_FORWARD` in `m_nButtons`;
  - velocity stays 0 and position never changes;
  - `MaxSpeed()` is 150, flags are normal (on ground, client), movetype is 2 (WALK).
- Next step I was about to run: print `GetLastUserCommand()->forwardmove/sidemove` in the dev command `gamma_playerinfo` (`game/server/gamma/gamma_devtools.cpp`).
  - If forwardmove is 0, the problem is client-side input: `CInput::CreateMove` / `ComputeForwardMove` in `game/client/in_main.cpp`, `cl_forwardspeed`, or the touch input code `in_touch.cpp`.
  - If forwardmove is non-zero, check `CHL2GameMovement` / `CGameMovement::WalkMove`, `sv_accelerate`, `MaxSpeed`, and the 2003 player changes.
- Note: the minimized test window may matter for client input; confirm with `cl_showusercmd`-style logging.
- The user's report predates the portal merge, so the cause is in the base port.

## TODO the user asked for (in their order of priority, before more weapon work)

1. Fix borderless window mode (`engine/sys_getmodes.cpp`, `sys_mainwind.cpp`, `gameui/OptionsSubVideo.cpp`). It probably needs the window sized to the desktop and the style re-applied on apply.
2. Fix the main menu's dialogs: "all sliders n things are broken and all dropdowns are empty".
   - The 2018 GameUI pages load `Resource/OptionsSub*.res` from our 2003 content, whose control names and layouts don't match the 2018 code.
   - Either ship 2018-compatible `.res` files styled like 2003, or port the 2003 pages.
3. Bring the 2018 Advanced video options (`COptionsSubVideoAdvancedDlg`) in and make them work with the UI.
4. Add "Advanced keys" in the keyboard tab: a dialog with two checkboxes, "Enable developer console" (`con_enable`) and "Fast weapon switch" (`hud_fastswitch`).
5. Use the user's uploaded "powered by Gamma Source" image as the loading screen.
   - The image is attached in the thread and included in this zip as `loading_screen.png`.
   - The 2018 `gameui/BasePanel.cpp` draws `console/loading` during loads; make a VTF and draw it full-screen there.
6. Movement (above). Then stage a new dated build and verify it on all four E3 maps.

## Known issues / notes

- `e3_techdemo_2` and `e3_hydra`: once the portal gun placed portals with no input. It hasn't reproduced since the ammo fix.
- Test runs wrote `portal_pak.vpk.sound.cache` into the Steam Portal folder (harmless; it gets regenerated).
- Fixup scripts `fixups2003*.py` are NOT idempotent; re-port a file before re-running its fixup.
- vpc `$File` lines can't be commented out (the parser still sees them); delete them instead.

## What's in the zip

- `source-engine-master/`: the full modified 2018 tree, without `build/`.
- `GammaSource-main/`: the repo, without the 3.9 GB `gammasource_content` and build outputs. It includes `gammasource_overlay`, `modernize/port2018/*` and the docs.
- `tools/`: the scratchpad scripts (`sync2018.ps1`, `shot2018.ps1`, `sheet.ps1`, `minidbg.py`, `vpkgrep.py`, `mdlbones.py`, and others).
- `loading_screen.png`: the user's loading-screen image.
- Not included (too large; they live on the user's machine):
  - `gammasource_content`
  - `source2018_assets`
  - `source2018_install`
  - the Steam VPKs
