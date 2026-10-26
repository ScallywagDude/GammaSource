# GammaSource / Half-Life 2 Beta Mega Build content integration

This source revision makes the original 2002 beta HUD part of the default Win32 engine, **not a toggle or a launch-time option**. `engine/common.cpp` always mounts the physical `gammasource_content` folder at the top of the `GAME` search path. `cl_dll/hud_numericdisplay.cpp` already draws the original HUD bitmap glyphs; the supplied `hud1.vtf`, material and sprite coordinates are bundled under `gammasource_content`.

The user-supplied `hl2_2000.zip` provided real **v_crowbar, v_pistol and v_smg1** model files. Their internal MDL37 animation/sequence tables were converted to native MDL36 structures, without just editing the version byte. All three converted files and their matching DX80 and DX7-2bone VTX6 geometry were validated offline and bundled. Original source files remain untouched; see `gammasource_content/MODEL_CONVERSION_MANIFEST.json`.

## Importing the larger Mega Build already installed on Windows

The full Project Beta Mega Build (several gigabytes) is **not contained in this chat's uploads**. The program `modernize/integrate_beta_megabuild.py` copies its compatible content from a physical installation or ZIP archive into the default overlay. It merges `materials`, `models`, `maps`, `sound`, `scripts`, `resource`, `particles`, `scenes`, etc., while selecting higher-resolution VTFs when conflicting textures exist. Mega-specific mods are not silently mixed into vanilla `hl2`. The bundled HUD atlas and sprite definitions are always preserved.

If your Mega Build is at `%USERPROFILE%\Documents\HL2BetaMegaCuck`, and your compiled game is at `C:\Games\GammaSource`, execute in Command Prompt from this source directory:

```bat
IMPORT_MEGABUILD_AND_INSTALL.cmd "C:\Games\GammaSource" "%USERPROFILE%\Documents\HL2BetaMegaCuck"
```

Or prepare the source overlay without installing:

```bat
py -3 modernize\integrate_beta_megabuild.py --mega-root "%USERPROFILE%\Documents\HL2BetaMegaCuck" --destination gammasource_content
```

Then run `INSTALL_GAMMASOURCE_CONTENT.ps1 -GameRoot 'C:\Games\GammaSource'` to copy the content overlay to the game's physical parent directory, next to the `hl2` folder. **Rebuild the Win32 engine (`engine.dll`)** to pick up the now-always-enabled search path. It does not modify files inside an external Mega Build installation.

## Content and code boundaries

The Mega Patch authors describe their release as **content fixes** (textures/models/scripts), not original C++ engine changes: https://hl2-beta.ru/index.php?topic=28043.0 . This update therefore modifies GammaSource's real engine filesystem path and integrates genuine available HUD/viewmodel assets; it does **not** claim to transplant nonexistent Mega Patch C++ source or to reverse-engineer an unprovided engine executable. Binary `.exe`, `.dll`, libraries and incompatible model versions are not installed into GammaSource.

The asset importer writes `MEGA_BUILD_ASSET_MANIFEST.json` with all installed paths and rejected models. It makes `.before-mega.bak` backups for overwritten files and never follows original game junctions. HUD resources are protected against a Mega import replacing the approved 2002 sprite art. For large game patches, rerun it after updating the Mega Build.

## Validation / remaining work

- 2002 HUD assets: included and engine search path enabled by default. Actual HUD layout/source was implemented in the preceding version.
- Three real first-person weapon MDL/VTX pairs: converted and structurally verified; **not yet runtime-rendered**.
- Bulk Mega Build assets: importable from your installed Mega Build but **not bundled here**, since the actual installation archive was not uploaded.
- The source checks include VTF resolution precedence, ZIP path traversal, original HUD preservation, model version rejection, and idempotence.
- Full Windows build, in-game visual comparison, other beta weapon viewmodels, and compatibility with all Mega Build campaign packs have not been established here.

AMD64 and Phase 7 unrelated behavior have not been modified by this package.
