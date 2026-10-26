# Phase 7 weapon restoration — v23

**Status: incomplete, with a new asset preflight and one real source-policy fix.**

## Changes

- `modernize/weapon_asset_audit_v23.py`: a read-only, ordered-layer audit of installed branch-local weapon scripts, first/third-person model MDL+VTX metadata, and HUD VMT/VTF sprites.
- Reuses the engine-era KeyValues parser; selects direct sprite data before the legacy `640` and `320` layouts and verifies actual VTF crop bounds, not guessed icon sizes.
- Keeps HL2, HL1, Counter-Strike, and TF2 content roots separate. Reports which overlay layer supplied the script, material and texture.
- Rejects traversal paths and symlink escapes when locating assets. Neither this audit nor v22's VTX metadata tool proves that model geometry, animation or UV scaling renders correctly.
- `modernize/weapon_grant_policy.h`: rejects `models/.mdl` and nested paths ending with `/.mdl` before `impulse 101` attempts a developer grant.
- `modernize/apply_modernization.py`: invokes the v23 audit as a repeatable source-only report generator.
- Five new synthetic Python tests plus extended C++98 smoke coverage.

## Current branch counts from v22 inventory

| Branch | Registered | Verified runtime/model/HUD/scale | No supplied content roots in this session |
| --- | ---: | ---: | --- |
| HL2 | 32 | 0 | Yes |
| HL1 | 15 | 0 | Yes |
| Counter-Strike | 28 | 0 | Yes |
| TF2 | 38 | 0 | Yes |
| **Total** | **113** | **0** | **All four** |

**Zero fully verified does not imply zero working weapons.** The missing game-content roots prevent identifying exact repairs in this environment. The 2003 v36/v37 model pipeline also still needs real Windows and GPU testing.

## Commands to run with your legally available unpacked game assets

```sh
python modernize/weapon_asset_audit_v23.py --assets-hl2 /path/hl2_2003 --assets-hl2 /path/hl2 --assets-hl1 /path/hl1 --assets-cstrike /path/cstrike --assets-tf2 /path/tf2 --output modernize/reports/WEAPON_PREFLIGHT_V23.json
python -m unittest modernize.tests.test_weapon_asset_audit_v23 -v
```

Optional repeated `--assets-BRANCH` roots are searched in the order supplied. For restored overlays, put the matching branch's repaired root before its original content root. Do not place TF2 or Counter-Strike assets in an HL2 search layer.

## Limitations before Phase 7 can be certified complete

1. Supply branch-specific, compatible weapon scripts, v36/v37 models with the required sidecars, materials, and HUD textures from authorized copies.
2. Resolve each weapon's missing script/model/vertex/HUD dependency from the generated report; reconcile v37 vertex candidates with 2003 loader limitations.
3. Validate animations, sound and attachment paths, UV texture scale, world and first-person models and inventory slot layout in the running game.
4. Build and test each branch on Windows. In particular check `impulse 101` with weapons already owned, empty/full inventories and multiplayer.
5. Keep abstract classes, uncompiled prototypes and invalid registrations excluded until their real dependencies are repaired.

No commercial artwork, binary assets or leaked engine code is needed to publish this independent diagnostic.
