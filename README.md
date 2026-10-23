# GammaSource

Modernization tooling and progress tracker for a historical Source-engine codebase. This public repository contains project-original documentation, validators, and test tools; it intentionally does not redistribute proprietary leaked engine or game source, compiled game assets, or SDK binaries.

## Current milestone

**Source milestone:** v24 (local source archive, not uploaded). **Active phase:** Phase 7 — Weapon Restoration. **Next phase after weapon restoration:** remaining engine/gameplay phases, with the alternative rendering APIs work scheduled last.

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

## Current Phase 7 progress

Phase 7 remains active. The local v24 source adds layered weapon-asset auditing, safe VMT `Patch` inheritance resolution, and model VTX companion checks across ordered content layers. It passed **183 tests, with 1 skipped and 0 failures**; source replay verified **8,267 files**. Windows compilation, game assets, and weapon-by-weapon runtime testing remain outstanding.

See [the single, continuously updated Phase 7 status](PHASE7_STATUS.md) for implementation history and completion blockers. Future progress belongs in this README and that status file, **not** standalone `PHASE7_Vxx.md` reports. Git commits retain the history.
