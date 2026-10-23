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

## October 22, 2026 — Phase 7 weapon audit publication

**Published to this repository:** [Phase 7 evidence checker](phase7_manifest_check.py) and [Phase 7 status report](PHASE7_STATUS.md). Both are regular, uncompressed project-original files. The private/local v22 engine source ZIP was not uploaded.

**Inventory gate:** 113 registered weapon classes (HL2/HL2MP 32, HL1 15, Counter-Strike 28, TF2 38); 86 compiled additional `impulse 101` grants; 7 excluded uncompiled candidate classes. Branch-grant consistency passed. **0/113 registrations have all four required runtime/model/HUD/texture-scale evidence flags**, so Phase 7 remains blocked, not finished. This indicates missing verification, not necessarily broken weapons.

**Tests performed:** the validator ran against the local v22 ZIP and returned the expected **INCOMPLETE** status (exit code 1) after passing inventory integrity checks. Windows compilation and in-game tests were not performed.

**Reproduce locally:**

```sh
python phase7_manifest_check.py /path/to/HL2_2003_Modernization_Source_v22.zip
```

**Remaining blockers:** verified playable models, materials, viewmodel animations, HUD artwork/scaling, complete cross-branch `impulse 101` and ammo behavior, and Windows/gameplay testing. Alternative rendering backends remain scheduled last.
