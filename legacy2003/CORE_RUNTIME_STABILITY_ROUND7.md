# GammaSource Core Runtime Stability Round 7

This round continues engine stability work while preserving the existing beta gameplay, HUD, weapons, and terrain behavior.

## Changes

### 1. Branded startup loading screen
- Replaced the plain gray Win32 startup "Loading..." screen with a centered GammaSource logo.
- Added a BMP asset at `gammasource_content/resource/gammasource_loading.bmp`.
- Updated both the launcher-side and engine-side Win32 loading paths so the branded loading screen appears whether the window is being configured by the launcher or directly by the engine.
- If the bitmap cannot be found, the legacy text fallback still appears.

### 2. Studio model mesh allocation hardening
- Added overflow-safe byte-count helpers in `studiorender/r_studio.cpp`.
- Prevented invalid or corrupted strip-group counts from producing unsafe allocations for:
  - index arrays
  - unique-triangle arrays
  - strip-data buffers
  - group-index-to-mesh-index tables
- Added graceful early-outs for invalid strip counts and negative bone-state-change counts.
- Prevented corrupted VTX data from overrunning strip-data size calculations.

## Files changed
- `launcher/getmodes.cpp`
- `engine/sys_getmodes.cpp`
- `studiorender/r_studio.cpp`
- `gammasource_content/resource/gammasource_loading.bmp`
- `launcher/res/gammasource_loading.bmp`

## Preservation notes
- The built-in 2002 HUD remains enabled.
- No beta weapons, menus, first-person models, gameplay systems, or deformable terrain features were removed.
- Loading-screen branding change is additive and keeps a fallback path to the original text renderer.

## Validation performed
- Verified the new bitmap assets are present in the source tree.
- Verified that only the intended source files and new loading-screen assets were added relative to Round 6.
- Performed source-level sanity review of the new startup-loading and strip-allocation code paths.

Full Windows compilation and runtime gameplay verification are still pending.
