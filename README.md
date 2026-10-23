# GammaSource

Modernization tooling and progress tracker for a historical Source-engine codebase. This public repository contains project-original documentation, validators, and test tools; it intentionally does not redistribute proprietary leaked engine or game source, compiled game assets, or SDK binaries.

## Current milestone

**Source milestone:** v22 (local source archive, not uploaded). **Active phase:** Phase 7 — Weapon Restoration. **Next phase after weapon restoration:** remaining engine/gameplay phases, with the alternative rendering APIs work scheduled last.

| Phase | Status | Verification needed |
| --- | --- | --- |
| 1. Visual Studio migration | Generated projects; not build-verified | Windows compile/link |
| 2. Engine modernization | Partial | Windows/runtime tests |
| 3. Shaders and lighting | Partial | Visual tests |
| 4. Steam and controller support | Partial | Steam/runtime tests |
| 5. Cut enemies and AI | Partial | In-game behavior/assets |
| 6. Weapon restoration (formerly Phase 7) | **Active** | Branch-specific inventory, models, HUD, animations, impulse 101, game tests |
| 7. Multiplayer and 3D preview | Partial | Runtime/network tests |
| 8. Vulkan/OpenGL/DX11/DX12 (moved last) | Infrastructure only | Complete backend implementation |

*Original phase IDs are retained in release notes. The display above uses intended execution order, not historical numbers.*

## Publication and verification policy

- Update **this README on every published milestone**, including version, exact changes, test counts and remaining blockers.
- Original tools, independent validation code, synthetic fixtures and documentation can be published as regular, uncompressed files.
- Do not upload or claim ownership of third-party proprietary 2003 game source or installed game assets in this public repository.
- Do not mark a phase completed without source integration and appropriate runtime/asset verification.
- Model-format acceptance is not proof that the rendered model, animations, or HUD appearance are correct.

## Release strategy

Complete Phase 7's weapon inventory, safe `impulse 101` handling, correct per-branch model and HUD references, and integrity checks first. Remaining rendering-API overhaul is the final phase.

## AI DISCLOSURE
Look, I'm ngl to you fam, ts is superrrr AI. I basically took the og source leak from 2003 and shoved that shit into Astra/Sol and were gonna see what happens with that, together.

## Redistribution
Yeah so like don't sell this. This is Forcefully Open-Sourced (LEAKED PROPRIETARY SOFTWARE). The only reason I put it up is because come on, it's a 20+ year old version of a game engine that's HORRIBLY unfinished, I don't think they care about little passion projects like this. Do whatever with it, just make sure it's NOT FOR MONETARY VALUE.

## October 22, 2026 — v23 weapon audit (Phase 7 active)

**Latest local source milestone:** v23 (complete source ZIP kept in the chat, not uploaded to public GitHub). **Publication:** [C++98 grant policy](modernize/weapon_grant_policy.h), [native test](modernize/tests/weapon_grant_policy_smoke.cpp) and [Phase 7 v23 report](PHASE7_V23.md). The full v23 ZIP contains the richer layered scanner `modernize/weapon_asset_audit_v23.py`.

The v23 changes check ordered per-branch weapon asset layers, legacy direct/640/320 HUD layouts, actual VTF rectangles, and v36/v37 VTX candidates. The model grant validator rejects empty `/.mdl` names. All branches remain isolated, and the Vulkan/OpenGL/DX11/DX12 overhaul remains last.

**Tests:** 180 run; 179 passed, 1 skipped, 0 failed. Overlay replay reproduced all 8,264 local v23 source files from v22, with five changed files and five additions. No successful Windows game build or real gameplay test was claimed.

**Completion gate:** 113 registered weapon classes, **0 fully runtime/model/HUD/scale-verified** because the required branch-specific game assets and runtime tests were not available here. This means unverified, not inherently broken. Phase 7 stays active, and no other phase is newly certified complete.

**Next:** Audit legally available unpacked models/materials/weapon scripts for each branch, repair actual dependencies, confirm viewmodel/worldmodel placement and HUD identity/scaling, and test `impulse 101` in each running branch.

## October 22, 2026 — Phase 7 v24 code update

**Local milestone:** v24 (8,267 files; 5 modified and 3 added relative to v23). **Active phase:** Phase 7, weapon restoration. **Publishing progress:** [Layered weapon asset auditor source](modernize/weapon_asset_audit_v23.py) and [v24 implementation and validation report](PHASE7_V24.md) are in this repository as normal source and Markdown files.

**New implementation:** VMT `Patch` materials now resolve inherited textures through safe, bounded `include`, `replace` and `insert` processing. Model VTX companions can be resolved independently across ordered content layers within a branch. The validator remains read-only and keeps HL2, HL1, Counter-Strike and TF2 assets isolated.

**Validation:** 184 regression tests run, 183 passed, 1 skipped, 0 failed. The v23-to-v24 overlay replay matched all 8,267 full-source files. **No Windows compile or in-game rendering test** has been completed.

**Remaining weapon work:** all 113 registered weapon classes need actual installed assets, correct texture/UV/HUD/viewmodel presentation and live `impulse 101` gameplay validation before Phase 7 can be certified. The full proprietary source tree is not yet bulk-published to GitHub. Renderer API modernization remains the final phase.
