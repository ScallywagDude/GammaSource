# GammaSource

Modernization of the historical 2003 Source-engine tree. The uploaded source, generated Visual Studio projects, restoration tools, and existing Git history are present on `main`. Game assets and a successful rebuilt runtime are separate verification requirements.

## Current milestone and work order

**Source milestone: v26 (Phase 7 restoration and native Windows builds). Phase 7 is not complete.** Finish **Phase 7 — Weapon restoration**, then work on **Phase 3 — Rendering API modernization, shaders and lighting**, then continue the remaining phases. **Phase 3 includes Vulkan/OpenGL/DX11/DX12 modernization.**

| Work | Status | Required verification |
| --- | --- | --- |
| Phase 7: weapon restoration and HUD | **Active; must finish first** | Windows builds, per-branch assets, weapon behavior, 2002 HUD, HL1 weapon selection/icons, HUD color options, numeric Energy, animations, pickups, ammo and `impulse 101` |
| Phase 3: Vulkan/OpenGL/DX11/DX12, shaders and lighting | **Next, after Phase 7** | Working backends, shader builds, GPU/runtime and in-game visual checks |
| Remaining engine/gameplay, Steam/controller, cut-enemy and multiplayer work | Partial; follows the active priorities | Branch-specific build, runtime, network and asset tests |

Phase labels follow the current project roadmap; older documents retain historical numbering. No phase is complete solely because a source or metadata check passes.

## Published code milestones

- **v26 — Weapon restoration and native toolchain:** restored hopwire combat and the original remote-controlled manhack; reinstated six omitted TF2 weapons and their guidance/mortar dependencies; restored brickbat throwing and recovery. Fixed modern compiler compatibility, original assembly builds, static CRT/allocator linkage, dropped library dependencies, shader tooling and shared object-directory collisions. Added HL1-style weapon buckets with consistent icon sizing/tint, a HUD color tab in Options and an Energy counter beside Health and HEV Suit, keeping the 2002 amber style. All four legacy shader DLLs also build with the documented toolchain. **265 tests run: 262 passed, 3 skipped, 0 failures; replay preserves 7,118 files.** All four Release clients and servers and the Release engine have built successfully during this work; final checkpoint results and remaining runtime/art failures are recorded in [PHASE7_STATUS.md](PHASE7_STATUS.md) and [check results](modernize/reports/phase7_checks.json). New builds expose a startup crash and incompatible installed model layouts; no weapon is claimed gameplay-verified.
- **v25 — Phase 7 weapon and build repairs:** sticky launcher and hopwire preserve ammunition on failed projectile creation; hopwire guards invalid owner/empty-ammo events; TF2 repair gun prioritizes injured passengers; internal rocket projectiles are excluded from weapon grants. Fixed legacy HUD sprite auditing, contradictory completion evidence, Windows test replay, and initial CRT/build linkage blockers. **217 tests run: 216 passed, 1 skipped, 0 failures.** Overlay replay preserves 7,072 protected files; all 153 projects pass structural checks. At v25, the first five Release build steps passed; shader tools, debug linkage and server compiler errors still blocked a rebuilt game. The v26 repairs and current blockers are recorded below. See [PHASE7_STATUS.md](PHASE7_STATUS.md) and [recorded check results](modernize/reports/phase7_checks.json).
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

The source inventory now contains **114 registered entries and 102 gameplay candidates**. Native model-layout checks reject the installed HL2/HL1/CS weapon models that previously passed header checks; TF2 content also uses replacement HL2 weapon models. **0 weapons are claimed gameplay-verified.** The single [Phase 7 status](PHASE7_STATUS.md) records current build, art and runtime evidence. Phase 3's rendering APIs, shaders and lighting start after Phase 7 is completed and tested.
