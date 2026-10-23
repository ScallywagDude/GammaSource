# GammaSource

Modernization of the historical 2003 Source-engine tree. The uploaded source, generated Visual Studio projects, restoration tools, and existing Git history are present on `main`. Game assets and a successful rebuilt runtime are separate verification requirements.

## Current milestone and work order

**Source milestone: v25 (published Phase 7 source fixes). Phase 7 is not complete.** Finish **Phase 7 — Weapon restoration**, then work on **Phase 3 — Shaders and lighting**, then continue the remaining phases. **Vulkan/OpenGL/DX11/DX12 modernization stays the final phase.**

| Work | Status | Required verification |
| --- | --- | --- |
| Phase 7: weapon restoration | **Active; must finish first** | Windows builds, per-branch assets, weapon behavior, models, HUD, animations, pickups, ammo and `impulse 101` |
| Phase 3: shaders and lighting | **Next, after Phase 7** | Shader builds and in-game visual checks |
| Remaining engine/gameplay, Steam/controller, cut-enemy and multiplayer work | Partial; follows the active priorities | Branch-specific build, runtime, network and asset tests |
| Vulkan/OpenGL/DX11/DX12 | **Final phase**; infrastructure only | Working backends and GPU/runtime verification |

Phase labels follow the current project roadmap; older documents retain historical numbering. No phase is complete solely because a source or metadata check passes.

## Published code milestones

- **v25 — Phase 7 weapon and build repairs:** sticky launcher and hopwire preserve ammunition on failed projectile creation; hopwire guards invalid owner/empty-ammo events; TF2 repair gun prioritizes injured passengers; internal rocket projectiles are excluded from weapon grants. Fixed legacy HUD sprite auditing, contradictory completion evidence, Windows test replay, and initial CRT/build linkage blockers. **217 tests run: 216 passed, 1 skipped, 0 failures.** Overlay replay preserves 7,072 protected files; all 153 projects pass structural checks. The first five Release build steps pass, but shader tools, debug linkage and server compiler errors still block a rebuilt game. See [PHASE7_STATUS.md](PHASE7_STATUS.md) and [recorded check results](modernize/reports/phase7_checks.json).
- **v24 — Layered VMT Patch/VTX diagnostics:** inherited material textures and independently located model companions; historical validation was 184 tests run, 183 passed, 1 skipped. The source is now uploaded; that earlier result does not certify the current checkout or gameplay.

## Publication and completion requirements

- Update **this README with every published code milestone**, including actual validation results and unresolved blockers.
- Keep **one living [PHASE7_STATUS.md](PHASE7_STATUS.md)**. Update it in place; Git commits retain history. Do not create version-numbered Phase 7 status files.
- Preserve the existing Git history and the **AI DISCLOSURE** and **Redistribution** sections verbatim.
- Source registration, project membership, model headers and HUD atlas bounds are useful checks. Weapon completion still requires successful builds and actual visual/gameplay tests.
- Keep local game assets and generated build outputs out of source milestones.

## AI DISCLOSURE
Look, I'm ngl to you fam, ts is superrrr AI. I basically took the og source leak from 2003 and shoved that shit into Astra/Sol and were gonna see what happens with that, together.

## Redistribution
Yeah so like don't sell this. This is Forcefully Open-Sourced (LEAKED PROPRIETARY SOFTWARE). The only reason I put it up is because come on, it's a 20+ year old version of a game engine that's HORRIBLY unfinished, I don't think they care about little passion projects like this. Do whatever with it, just make sure it's NOT FOR MONETARY VALUE.

## Current Phase 7 verification

The local four-branch asset audit reports 96/113 metadata candidates; **0 weapons are claimed gameplay-verified**. TF2 alpha content uses replacement HL2 weapon models, and several prototypes remain incomplete or uncompiled. The single [Phase 7 status](PHASE7_STATUS.md) records the source, art, build and runtime blockers. Phase 3 starts after Phase 7 is completed and tested.
