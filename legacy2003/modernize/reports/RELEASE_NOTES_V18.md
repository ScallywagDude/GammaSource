# Half-Life 2 (2003) Modernization v18

Prepared from the exact supplied `HL2_2003_Modernization_Source_v17.zip`. The full source and patch-only overlay preserve every original archive member. The release completes feasible source integration and supplied-content repairs; Phase 7 is **not certified complete or playable** because compatible original assets, a successful full build and runtime verification remain unavailable.

## Implemented

- Branch-specific inventory covers 113 registrations: HL2/HL2MP 32, HL1 15, Counter-Strike 28, TF2 38. Shared physics tools, abstract/internal classes, project-excluded prototypes and commented/unregistered brickbat are explicitly distinguished. No foreign weapon inventory is imported.
- `impulse 101` uses installed-script/model/header/ammunition checks, duplicate and inventory-capacity protection, cleanup of rejected grants, actual branch ammo refill, and original cheat restrictions. CS expanded grants retain the available arsenal with deterministic same-slot ordering and branch armor/helmet state. Incomplete/internal weapons remain excluded.
- Incendiary rifle, tripwire, thumper and immolator source behavior repaired. TF two-handed support update logic repaired, while its uninitialized standalone container remains excluded from grants. HL2 HUD safety is preserved and CS/TF icon fitting now preserves aspect ratio and source resolution.
- Real animated HL2/HL2MP Options -> Multiplayer preview implemented using the existing engine studio renderer, model geometry/materials and shared bone animation. Includes mouse drag, idle rotation, framing, status updates, installed-format checks and the existing save/apply path. It requires compatible assets and rebuilt engine/GameUI; no static portrait is called a 3D result.
- Twenty-one cut/experimental NPCs inventoried; twenty active Hammer definitions and source test rooms generated. Attachment/effect/null/projectile-reference safety repaired in Assassin, Bullsquid, Mortar Synth, Combine Guard, Light Stalk and Launcher. Conscript remains behind its obsolete disabled implementation. Availability is kept separate from AI correctness.
- Existing DX9 cheap-water shader normalization and Fresnel bounds repaired. A real FXC/DXC run exposed and fixed an invalid reserved HLSL identifier; all four Direct3D 11/12 stages now compile, and their project-owned bytecode/provenance is included under `modernize/shaders/compiled_v18`. Compiler discovery now finds the installed SDK beyond PATH. Original lighting safeguards are preserved. OpenGL/Vulkan validation lacks glslangValidator; no functioning modern rendering backend is claimed.
- Content-aware offline validation covers exact case/search order, scripts, MDL metadata, supported companion variants, VMT/VTF dependencies, real HUD rectangles, sound manifests, shader source/includes, player allowlists, branch mismatches, and unreferenced candidates. It never classifies those candidates as safe to delete.
- Deterministic packaging and a hash-checked release-overlay applier verify that the exact v17 ZIP plus v18 overlay reproduces every full-source archive member. Every added/modified file is listed in `CHANGELOG_V18.md` and the machine manifest.
- Project regeneration now uses portable filter IDs and solution ordering, exact CRLF output and the original DSP staging metadata. This preserves v17's unrelated generated project settings across Windows replay; the new preview project entries remain included.

## Actual asset work and its boundary

The v17 source archive has no game MDL/VVD/VTX/PHY/QC/SMD/VMT/VTF, weapon scripts or sound payloads. Utility textures and application icons are not adequate authoring sources for original weapon/NPC geometry, hands, skeletons, UVs or animations. No substitute artwork or unrelated model is represented as restored original content.

The local workspace also contains user-supplied game content. It was inspected read only, with explicit branch roots: `hl2_2003`, common `hl2`, and `hl1`. The audit scans every unpacked material definition in those roots. CS/TF2 game-content roots were not supplied; their references remain unverified rather than being filled from another branch.

Exact-case, redundant-separator, existing legacy HUD prefix, conservative VMT syntax and same-weapon reticle fallback repairs are composed in `ASSET_RESTORATION_PLAN_V18.json`; corresponding unified diffs are included. There are 6,382 edits across 5,281 hash-verified local files. Generated local edited scripts/materials are kept outside the source archives in `asset_restoration_v18`, alongside the source tree, so the archives contain no newly redistributed proprietary game asset payloads. All 276 raster HUD crops are nonblank in the post-repair pixel audit; three HL1 autoaim regions explicitly reuse their weapon's own regular crosshair rather than claiming reconstructed original art. See `ASSET_REPAIRS_V18.md` for branch/layer mapping and reproduction commands. Seven ambiguous malformed VMTs remain untouched.

Installed model samples report studio version 37; this source declares version 36. The metadata reader and new preview do not guess a version-37 layout or infer compatibility from the engine's old version-field overwrite. Correctly matching filenames therefore do not establish a usable restoration. The missing report lists exact paths and unsupported-version results. Legally supplied compatible compiled models and matching materials/animation/mesh dependencies, or sufficient authorized authoring sources, are required for the remaining model work.

Controller input has no referenced glyph assets in the supplied XInput bridge. Existing pickup artwork/audio dependencies are checked through weapon HUD data, `HudAnimations.txt`, sound manifests and the pickup sound events; present working content is not unnecessarily replaced.

## Deliverables and reports

| Requirement | Delivered file |
|---|---|
| Complete source archive | `HL2_2003_Modernization_Source_v18.zip` beside the source folder |
| Patch-only overlay | `HL2_2003_Modernization_Overlay_v18.zip` beside the source folder |
| Weapon audit | `WEAPON_ASSET_AUDIT_V18.md/.csv`, `WEAPON_IMPLEMENTATION_V18.md`, asset dependency JSON, branch/build inventories |
| NPC audit | `NPC_MATERIAL_IMPLEMENTATION_V18.md`, `NPC_ASSET_AUDIT_V18.json`, `NPC_EXTERNAL_ASSET_AUDIT_V18.json` |
| Multiplayer models | `MULTIPLAYER_MODEL_AUDIT_V18.md`, `MULTIPLAYER_IMPLEMENTATION_V18.md`, `MULTIPLAYER_PREVIEW_VALIDATION_V18.json` |
| Remaining assets | `repaired_assets/MISSING_ASSETS_V18.md/.json`; baseline `MISSING_ASSETS_V18.*` and `SOURCE_ONLY_ASSETS_V18.json` retained |
| Change log | `CHANGELOG_V18.md`, `modernize/source_change_manifest.json`, `modernize/source_changes.patch` |
| Actual validation | `VALIDATION_V18.md`, `VALIDATION_RESULTS_V18.json`, raw `validation/*.log`, `weapon_syntax_v18.json` |

Report filenames above are under `modernize/reports/` unless the row names `modernize/` explicitly or refers to the older top-level modernization inventories. `VALIDATION_V18.md` records the final whole-tree results and limitations.

## Reproduction

Extract the overlay's `modernize/apply_release_overlay.py` and `modernize/package_release_v18.py`, then run with Python 3:

```text
python modernize/apply_release_overlay.py --original HL2_2003_Modernization_Source_v17.zip --overlay HL2_2003_Modernization_Overlay_v18.zip --destination new_v18_source
```

The destination must not exist. The exact base ZIP hash is required. The full archive may instead be extracted directly. Run offline checks from its source root:

```text
python modernize/run_validation_v18.py --verify-replay --compiler-bin "<directory containing clang++>"
python modernize/asset_validation_v18.py --source-only
python modernize/asset_validation_v18.py --assets-hl2 "<hl2_2003>" --assets-hl2 "<common hl2>" --assets-hl1 "<hl1>" --all-materials
```

For local reference repairs, keep the same root order used to generate the plan, then write a new overlay:

```text
python modernize/asset_case_patch_v18.py --apply --plan modernize/reports/ASSET_RESTORATION_PLAN_V18.json --assets-hl2 "<hl2_2003>" --assets-hl2 "<common hl2>" --assets-hl1 "<hl1>" --output-dir "<new local overlay directory>"
```

`hl2/root0` overlays the first HL2 content root; `hl2/root1` overlays the second; `hl1/root0` overlays the HL1 root. These are separate branch/layer directories, not a combined game directory. Do not merge foreign branches. The tools do not download assets, SDKs or compiler binaries.

## Remaining verification

Full Windows linking, gameplay/weapon behavior, model UVs and hands, material appearance, animation quality, NPC AI, multiplayer replication, GPU preview output, display-resolution layouts and device-loss behavior remain runtime-unverified. The targeted compiler checks and synthetic format fixtures establish narrower properties only. Original modernizations are preserved, but earlier release claims do not override these explicit v18 limitations.
