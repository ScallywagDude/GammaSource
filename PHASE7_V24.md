# v24 — Phase 7 weapon asset-integrity update

**Local source baseline:** v23. **Complete local source:** v24 (8,267 files). **Phase 7:** active, not complete.

## Changes
- The branch-layer-aware weapon auditor now resolves VMT `Patch` inheritance for weapon HUD images, with `include`, `replace`, and `insert` behavior.
- Every included VMT is located using the selected game's ordered asset layers. The audit records each material's source layer and the winning VTF layer.
- Material resolution rejects malformed documents, missing includes, cyclic includes, chains deeper than 12, and unsafe traversal paths.
- The studio model preflight now locates VTX companions independently across asset layers and records where compatible candidate metadata is found.
- Valid material inheritance is no longer falsely reported as a missing HUD texture. File-format eligibility is still **not** a runtime rendering guarantee.

## Validation

The focused weapon-asset tests pass 9/9 checks. Whole-tree Python/native suite: **184 run, 183 passed, 1 skipped, 0 failed**. The overlay re-created all **8,267** source files from the exact v23 archive and passed bytewise content comparisons.

The complete local ZIP contains **five changed** and **three added** files relative to v23. The uncompiled engine is not claimed to be playable.

## Still blocked

113 registered weapon classes still lack complete runtime, viewmodel/worldmodel, animation, HUD-image, texture-scaling, and branch-specific `impulse 101` verification with the actual game assets. This describes unavailable test evidence, not 113 broken weapons.

The public repository contains original tooling and the audit source. The full current working-source snapshot remains downloadable in the corresponding conversation; the full multi-thousand-file tree has not yet been successfully bulk-committed to GitHub.

The modernization roadmap still places Vulkan, OpenGL, Direct3D 11 and Direct3D 12 last.
