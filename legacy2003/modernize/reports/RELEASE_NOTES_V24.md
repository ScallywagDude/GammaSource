# Source modernization v24 — Phase 7 weapon asset integrity

This pass updates the local v23 source tree, not a playable game build.

## Changes

- The branch-layer-aware weapon auditor now follows Source VMT `Patch` materials. It resolves `include` references using the *same ordered HL2 / HL1 / CS / TF2 search path* supplied for the relevant game branch, recording the selected material layers.
- The resolver supports `replace` (only existing inherited keys) and `insert` (only absent inherited keys). It rejects directory traversal, malformed inheritance documents, missing includes, cyclic references, and chains over the configured depth limit.
- HUD sprite checks can validate the inherited `$basetexture` VTF rectangle rather than falsely reporting a valid Patch material as having no texture.
- Model checks now resolve each VTX sidecar from the caller-provided game search layers independently of the MDL layer and record the winning VTX layer. Compatible vertex metadata is *not* proof of functioning v36/v37 animation/geometry.

## Verification

- The focused weapon auditor suite passes 9 tests, including newly added mixed-layer Patch and depth-bound fixtures. All fixtures use synthetic game assets.
- Whole-tree regression suite: 184 tests, 183 passed, 1 skipped, 0 failed.
- Existing Visual Studio project-structure checks and earlier source tests are included, but the Windows engine has NOT been compiled or run in this pass.

## Remaining Phase 7 requirements

113 registered classes still need actual branch-specific viewmodels, world models, compatible materials, textures, weapon attachments, HUD imagery, proportions, animations, ammo/slot behavior and playtesting. The locally supplied source ZIP does not contain installed commercial game assets. This report does not mark Phase 7 as finished or assert a graphical renderer is working.
