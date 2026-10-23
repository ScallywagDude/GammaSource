# Phase 7 — Weapon restoration v23 progress

**Current phase: active, not complete.** This update is derived from the local v22 source inventory and v23 source-only diagnostics. Windows compilation and game/runtime verification have not been performed.

## v23 changes

- Introduced `modernize/weapon_asset_audit_v23.py` in the local v23 source ZIP. It scans caller-supplied **ordered layers** per game branch, identifies script/model/HUD dependencies and returns the source layer for each resolution.
- Handles legacy HUD raster layouts: direct sprite data takes precedence over `640` and then `320`. Validates VMT/VTF image dimensions and sprite rectangle bounds.
- Uses the v22 studio model sidecar scanner to classify v36/v37 model/VTX candidates. This is a file-format preflight, not evidence of working geometry or animation.
- Hardened the independently written C++98 model-path validator to reject empty `/.mdl` basenames. The reusable `modernize/weapon_grant_policy.h` and its `modernize/tests/weapon_grant_policy_smoke.cpp` tests are published in this repository.
- Local v23 source and overlay are reproducible against the exact v22 archive: 8,264 files; five updated files, five additions.

## Validation

**180 tests ran: 179 passed, 1 skipped, 0 failures.** Also ran the new five synthetic layered-asset tests. SHA-256 verified the v22 + v23 overlay reproduces all v23 source contents.

| Game branch | Registered weapon classes | Fully verified visually and in-game |
| --- | ---: | ---: |
| HL2 / HL2MP | 32 | 0 |
| HL1 | 15 | 0 |
| Counter-Strike | 28 | 0 |
| TF2 | 38 | 0 |
| **Total** | **113** | **0** |

Zero verified means verification **has not happened**, not that all the weapons are broken. No real game asset content directories are attached to this public GitHub release.

## Next blockers

To finish Phase 7, provide authorized, unpacked asset roots for each branch and run the local v23 audit. Repair script, MDL/VVD/VTX, VMT/VTF, HUD mapping, ammunition and animations on a branch-by-branch basis. Verify correct UV scaling, hand attachment, slot selection and `impulse 101` within a successfully compiled Windows game. Uncompiled prototype classes must stay excluded until their actual dependencies are implemented.

The original model/texture art, proprietary 2003 source, and SDK binaries are deliberately not included in the public GitHub repository.
