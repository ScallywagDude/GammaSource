# Visual repairs status — 2026-10-25

The integrated engine/client/server/GameUI build passed. The three visual repairs below are implemented, but their zoo_item runtime test had **not started** when the user requested packaging and handoff. Do not describe them as visually verified in game.

Latest stage being refreshed by the coordinating agent:
`C:\Users\doge6\Documents\GammaSourceTest\2026-10-25_18-29-57\GammaSource_2018`

## Physgun beam

The retained 2003 client effect used the old Sprite shader with beam geometry, bounds covering only its endpoint, no per-frame leaf-system refresh, and incomplete removal cleanup. The server also sent a viewmodel entity index as a model-precache index. These are repaired; the precise contribution of each defect awaits runtime confirmation.

Changed live engine files, relative to `C:\Users\doge6\Downloads\source-engine-master\source-engine-master`:
- `game/client/hl2/c_weapon_gravitygun.cpp`: compatible retained material, whole-curve bounds, attachment fallback, continuous updates, cleanup and diagnostics.
- `game/server/physgun.cpp`: private beam material and entity-index networking.

Private overlay assets under `C:\Users\doge6\Videos\GammaSource-main\gammasource_content_2018`:
- `materials/sprites/gamma_physbeam.vmt`
- `materials/sprites/gamma_physbeam.vtf`

The VTF is byte-identical to the original 2003 `materials/sprites/physbeam.vtf`. Only the shader wrapper changes to UnlitGeneric with additive vertex-color/alpha rendering.

## Original 2003 HEV suit pickup

`item_suit` used the shared `models/items/hevsuit.mdl` path, allowing mounted retail content or old converted assets to replace it. `game/server/hl2/item_suit.cpp` now precaches and uses the private `models/items/gamma_hevsuit_2003.mdl` path.

Overlay files:
- `models/items/gamma_hevsuit_2003.mdl`
- `models/items/gamma_hevsuit_2003.dx80.vtx`
- `models/items/gamma_hevsuit_2003.dx7_2bone.vtx`
- `materials/models/gs_suit/hev_sheet.vmt`
- `materials/models/gs_suit/hev_sheet.vtf`

The MDL mesh/animation data, VTX data and suit texture are original 2003 assets. MDL byte changes are limited to its internal name and private material-directory string. Source assets in `C:\Users\doge6\Videos\GammaSource-main\gammasource_content` were checked against `C:\Users\doge6\Videos\GammaSource_Build\gammasource_content` and match exactly. The original `models/Items/hevsuit.jpg` thumbnail shows the expected beta vest and black arms, unlike the user's silver-shouldered retail pickup screenshot.

## zoo_item sky

`zoo_item.bsp` is version 18 and requests `sky_wasteland04`. All six original 512×512 DXT5 sky faces exist and have valid VTF headers. The conversion bug was absent per-leaf sky flags: BSP18 has a plain area field, while the 2018 client requires sky visibility flags before submitting its sky pass. This caused the hall-of-mirrors ceiling.

`engine/modelloader.cpp` now conservatively enables the sky pass for non-solid leaves in BSP18 maps containing sky surfaces. Existing surface visibility/depth still determines visible sky. Native modern BSP behavior is unchanged. zoo_item conversion changes eligible sky leaves from 0 to 376.

Both reproduction routes were updated:
- `C:\Users\doge6\Videos\GammaSource-main\modernize\bsp18_to_19.py`
- `C:\Users\doge6\Videos\GammaSource-main\modernize\port2018\patch2018_bsp18.py`

The generator helper matches the live engine exactly. Existing high-priority caches under `C:\Users\doge6\Downloads\source2018_install\hl2\converted\maps` were backed up and repaired:
- `d1_trainstation_01.bsp` — 1,674 eligible sky leaves
- `e3_hydra.bsp` — 273
- `e3_strider.bsp` — 1,057
- `testroom_physprop.bsp` — 83

Before changing these caches, seven original lump payloads (texinfo, leaves, planes, vertices, visibility, nodes and leaf faces) were proven byte-identical to their BSP18 sources. Only leaf sky-bit bytes changed. A `gamma_legacy_sky_revision2.json` record is beside those maps. **Packaging must include these updated caches or remove stale equivalents from a package's higher-priority converted mount.** The original zoo_item BSP was not changed; the engine converts it in memory.

## Reproduction and pending runtime QA

Regenerate the seven private assets with Python:
`C:\Users\doge6\Videos\GammaSource-main\modernize\port2018\repair_visual_assets.py`

Launch only an isolated validation instance, using the refreshed matching client/server/engine binaries and current overlay. On `zoo_item`, run:
```text
exec gamma_zoo_visual_check
```

The cfg is `gammasource_content_2018/cfg/gamma_zoo_visual_check.cfg`. It finishes after about nine seconds and captures:
- `jpeg zoo_sky_fixed`: sky above the spawn.
- `jpeg zoo_physgun_beam_fixed`: held primary beam.
- `jpeg zoo_suit_2003_fixed`: original suit pickups.

Diagnostics:
- Client `gamma_physgun_beaminfo`: active/registered state, emitter attachment, material/error, draw age and curve bounds. Verify an active visible beam draws recently, then unregisters on release/holster.
- Server `gamma_suitinfo`: actual model path and positions of each suit pickup. Expect `models/items/gamma_hevsuit_2003.mdl`.
- Engine conversion developer output: `GammaSource BSP18: restored sky visibility in ... leaves`.

Still required: inspect all three screenshots, confirm the sky replaces the stale ceiling, confirm beam appearance and release/holster cleanup, and confirm beta suit appearance/materials. No successful runtime result for these three repairs is claimed yet. Completed weapon values, beta hands and prior audits were not changed by this work.

## Evidence and backups

Reports in this folder:
- `visual_assets_provenance.json`
- `visual_source_review.json`
- `visual_bsp_conversion_audit.json` — initial inspection; its old cache `updated:false` entries predate the subsequent repair.
- `visual_existing_bsp_cache_repair.json` — authoritative completed cache repair record.

Pre-change source and converted-map backups:
`C:\Users\doge6\Documents\GammaSourceTest\handoff_finish_backups\visual_followup`

A separate small follow-up makes `-safe` bypass desktop borderless even if config reload sets `vid_borderless 1`: `engine/sys_getmodes.cpp` and `engine/sys_mainwind.cpp`. Its backups are under `handoff_finish_backups/safe_followup`. The newest -safe runtime recheck is also pending.
