# Half-Life 2 (2003) Modernization — v19 source continuation

**Base:** `HL2_2003_Modernization_Source_v18.zip` (exact SHA-256 checked). No original installed game MDL/VMT/VTF/audio data is included. This is an incremental source update, not a finished playable build.

## Changes implemented

### Multiplayer player-model compatibility

- HL2 server model precache and spawn now inspect actual studio headers before sending them to the source-v36 loader. Matching names alone no longer permit version-37 assets (or malformed/truncated headers) to be loaded.
- The original `models/player.mdl` remains first fallback, followed by the other HL2 allowlisted models that pass the same check. If no compatible model exists, the server logs the condition and leaves the player without a model. This is intentionally fail-closed, **not** a workaround for missing original assets or confirmation of playability.
- The 3D menu preview gains right-click to reset yaw and animation, double-left-click to reset, and middle-click to pause/resume animation. Left-drag and slow idle rotation remain supported. The existing `GameUIModelPreview001` ABI is unchanged.

### Asset validation and release reproducibility

- Added C++98 studio-header regression fixtures covering valid v36 headers, unsupported versions, truncated files, truncated buffers, inconsistent declared lengths and invalid signature.
- Existing JSON generator scripts now preserve published CRLF/LF formatting and avoid rewriting unchanged reports. This fixes cross-platform replay drift without altering the weapon grant inventories or game branches.
- The older filename-only regression assertion now checks the stronger compatibility gate.
- Added a v18-base SHA-256-checked v19 overlay applier. This overlay applies to **v18**, unlike v1–v17 overlays that were designed for other baselines.

## Validation

- Linux host: `python -m unittest discover -s modernize/tests -p 'test_*.py'` → 148 tests run, 147 passed, one skipped, zero failures/errors.
- Native policy smoke compiled and executed under C++98.
- v19 patch generator repeated with identical source output.
- The full game was not compiled, linked or launched. These tests do not establish in-game gameplay, rendering or missing asset compatibility.

## Outstanding work

1. Licensed version-36 player/weapon models with matching 2003 vertex, animation and material dependencies (the examined game content is version 37).
2. Full weapon model/animation/texture/HUD and cut-NPC runtime verification.
3. Windows build/toolchain integration; Vulkan/OpenGL/DX11/DX12 real backends; Steamworks networking; multiplayer gameplay testing.
4. Full server/client/GPU validation of the new player-model rejection and interactive preview behavior.

Earlier v18 asset repairs (6,382 edits in the external local overlay) are represented by diff/plans in the source archive and **not** embedded as redistributed game assets.
