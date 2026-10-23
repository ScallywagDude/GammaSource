# Phase 7 — Weapon restoration

**Latest source milestone: v26. Status: active, not complete or gameplay-verified.** This is the single living Phase 7 status file. Finish Phase 7 first, then Phase 3 (Vulkan/OpenGL/DX11/DX12 rendering API modernization, shaders and lighting), then continue the remaining phases.

## Current branch inventory

| Branch | Registered entries | Extra `impulse 101` grants | Uncompiled grant candidates excluded | Fully verified |
| --- | ---: | ---: | ---: | ---: |
| HL2 | 33 | 23 | 0 | 0 |
| HL1 | 15 | 15 | 0 | 0 |
| Counter-Strike | 28 | 25 | 0 | 0 |
| TF2 | 38 | 30 | 0 | 0 |
| **Total** | **114** | **93** | **0** | **0** |

These are source/project inventory counts. The 114 entries include 12 explicit non-gameplay classifications: bases, internal class-loadout helpers, an internal rocket projectile and the cubemap debugging tool. There are 102 gameplay candidates (HL2 32, HL1 15, CS 25, TF2 30), including the restored manhack, brickbat and six TF2 prototypes. The completion gate keeps unfinished concrete weapons in scope even if a grant exclusion is added. Project membership alone is not proof of compilation or gameplay. Extra grants exclude each branch's original cheat loadout.

## Latest source changes

**v26**

- The requested HUD work is part of Phase 7: HL1-style numbered weapon buckets, expanded active-slot icons and scrolling for the restored arsenal; consistent aspect-preserving icon canvases and palette tint; an Options HUD tab with RGB controls, preview, Apply/Cancel and restored 2002 amber; a numeric **Energy** display beside Health and HEV Suit. The networked sprint value is rounded/clamped to 0�100. Existing 2002 font resources are preferred when installed; stock fonts remain the fallback. Runtime appearance and complete replacement art still require gameplay verification.
- Hopwire now deploys a bounded eight-hook tether charge, enters its combat stage, detonates with the branch's grenade damage, and cleans up hooks, springs and effects. Failed allocations have a deployment deadline; ownership and saved stages are guarded.
- Manhack restores the source's remote blade-control design with an owner-bound camera, stationary vulnerable player body, movement/time/distance limits, blade damage and secondary exit. Holster, drop, death, owner loss and save restoration clean up control. It is included in HL2 grants.
- Six omitted TF2 weapons and their client/network counterparts are included by default for TF2. Restored guided plasma uses the existing powered projectile; mortar connects its existing firing UI to bounded owner-validated commands and the deployed object. Laser weapons use the supplied APIs and clean up targeting state. Reconstructed defaults still require gameplay/balance testing.
- Brickbat reactivates the original bottle, rock and cremator-head throwing/recovery design with typed pickup accounting, limits, deferred throws and failed-spawn ammo preservation.
- Repaired callback pointers, network offsets, dependent templates, registration access, formatting and Debug follower handles. The follower manager uses stable list-node identities instead of copying a modern checked iterator into a pointer-sized handle.
- Project conversion restores library source inputs, uses matching freshly built foundation/GUI libraries, preserves static CRT defaults and per-file PCH exclusions, retains VC6 loop scope, and isolates intermediate objects by project. Legacy staging is opt-in with `HL2ModernStageLegacy=true`, preserving shipped archives by default. The supplied S3 codec archive retains its historical x86 exception-table compatibility setting.
- Native Debug/Release allocator checks exercise the rebuilt DLL boundary, growth zeroing, overflow rejection and diagnostics. Modern UCRT entry points use the same engine allocator; no forced links or dummy allocator were added.
- Shader builds use a modern Perl interpreter, installed SDK FXC and D3DAssemble, preserving legacy shader profiles and DWORD byte packing. Original engine assembly is translated with the checked-in translator rebuilt from source, assembled with current MASM, and linked as real objects.
- A read-only native model probe uses the same full `StudioDataValidator` as the engine. Passing a version/header check is explicitly insufficient.

**Final checkpoint: 265 tests run, 262 passed, 3 skipped, 0 failures.** Separate native x86 assembly tests and the optional legacy-compiler shader checks passed. Reapplying all overlays leaves 7,118 protected files unchanged. Structural checks pass for 153 projects / 348 configurations.

Final checkpoint test/build/replay results are recorded in [phase7_checks.json](modernize/reports/phase7_checks.json). Gameplay verification remains false.

**v25**

- Sticky launcher: a failed grenade factory no longer dereferences null, spends ammunition, or produces successful firing effects. Player and NPC paths share the success result.
- Hopwire: failed grenade creation is safe; throw/roll/lob animation events require a player owner and positive ammunition, and consume ammunition only after successful creation. Successful trajectories, timer, redraw and final-ammo removal are retained. Its unfinished combat behavior remains a blocker below.
- TF2 repair gun: injured vehicle passengers are prioritized over healthy passengers; both passenger orders are tested using the actual source method.
- TF2 grants: `weapon_grenade_rocket` is a `CBaseAnimating` projectile, not an equipable weapon. It remains registered for its launcher but is excluded from `impulse 101`. Both historical grant generators use the authoritative exclusions so replay cannot add it back.
- Legacy HUD auditing: bare material names in `640`/`320` sprite blocks resolve under `sprites/`, matching the client. Direct/explicit paths retain their meaning and traversal stays rejected.
- Completion gate: checks individual Debug/Release build rows, aggregates, source paths, exclusions, grants, asset status/issues and explicit boolean evidence. Missing/contradictory reports cannot establish completion; a source inventory alone cannot establish a Windows build.
- Windows prerequisites: repaired legacy string/CRT compilation, preserved vstdlib's static runtime choice, linked the matching fresh tier0 import library, and added the modern `_msize_base` entry through the same allocator. Existing string API signatures are retained. Explicit VC6 `/ML` switches map to the supported static CRT, including the generated engine configurations.
- Every source repair has a repeatable overlay; targeted C++98 tests execute actual weapon methods with controlled factories and targets. The Windows studio replay test uses its running Python interpreter.

**Validation: 217 tests run, 216 passed, 1 skipped, 0 failures.** All portable C++98 checks ran with the installed compiler. Reapplying the complete overlay left 7,072 protected source/project/tool files byte-for-byte unchanged. Structural validation passes for 153 projects and 348 configurations. The [recorded check results](modernize/reports/phase7_checks.json) distinguish passing tests from the failed full builds. No runtime verification flags are set by this milestone.

**v24 (historical result)**

- Layered weapon auditor resolves Source `Patch` materials through `include`, `replace`, and `insert`, recording the search layer for inherited textures.
- Detects malformed/cyclic/unsafe material includes and excessive inheritance depth.
- Locates VTX companions independently across ordered asset layers.
- Historical validation: 184 tests run, 183 passed, 1 skipped, 0 failed; overlay reapplication matched 8,267 source files from v23.

**v23 (historical result)**

- Introduced per-branch layered asset auditing for models, scripts, HUD materials, VTF dimensions and direct/640/320 legacy sprite layouts.
- Added safe weapon model-path checks rejecting `/.mdl` as an empty basename.
- Historical validation: 180 tests run, 179 passed, 1 skipped, 0 failed; replay reproduced 8,264 local source files from v22.

## Build and asset evidence

The Release foundation projects, all four Release game clients and servers, and the Release engine have built successfully during v26 work. Debug foundation/GUI projects and all four Debug clients and servers have also built during this work. All four updated clients and GameUI build in Debug/Release. The final localized selector also relinks in both HL2 configurations. The recorded results identify exactly which updated configurations passed. Native HL2/CS server module loading and HL2 client module loading passed, including interface creation and unloading; those are not gameplay tests.

Legacy Perl/compiler process-launch failures are resolved with configurable modern tools. All four legacy shader DLLs (DX6, DX7, debug and DX9) build. The DX9 register-allocation problem is resolved using an optional earlier signed Microsoft compiler, retaining exact profiles and full precision; all 352 teeth shader permutations compile. See [toolchain instructions](modernize/LEGACY_SHADER_TOOLS.md). Full ordered Debug/Release builds remain incomplete. VPhysics compilation reaches linking, where supplied legacy Havok archives still require compatible runtime/exception metadata.

An isolated runtime was launched with freshly rebuilt engine/client/server and copied original BSP18 test maps. The filesystem and launcher were rebuilt to remove proven mixed-binary ABI failures. The current failure is the old shaderapidx9.dll during ShaderSystem::Init. The matching shader API compiles, but its legacy d3dx9.lib archive lacks SafeSEH metadata and blocks linking. A prior mixed-engine run also faulted in the new client. Runtime crashes are being diagnosed, and successful gameplay is not claimed. Installed game binaries/assets were not overwritten.

Matching asset directories were found locally for all four branches. A read-only layered audit reports:

| Branch | Metadata candidates | Registered entries | Gameplay verified |
| --- | ---: | ---: | ---: |
| HL2 | 32 | 32 | 0 |
| HL1 | 15 | 15 | 0 |
| Counter-Strike | 24 | 28 | 0 |
| TF2 | 25 | 38 | 0 |

These are the earlier 113-entry metadata audit totals, including internal entries; they do not cover the newly registered brickbat. CS flags include three excluded bases and Scout's absent ordinary crosshair/autoaim entries, with zoom reticles present. TF2 lacks prototype/harpoon scripts and correct weapon identities: its included README explicitly says weapon models were replaced with HL2 models.

The deeper native probe rejects all 55 HL2, 28 HL1 and 50 CS unique referenced models against this source tree's complete layout validator. All are MDL37; matching version/VTX headers did not establish compatible pointer tables. Of 23 unique TF2 referenced models, 22 match the loader's layout and one MDL37 fails. A separate original-leak weapon-model sample contains 62 rejected MDL37, four rejected MDL32, seven accepted through the real 35→36 path and one accepted MDL36. These are parser results, not visual/animation/gameplay certification. Local paths/assets are not published.

## Remaining completion blockers

- Finish the complete ordered Windows builds, remaining shader/register and legacy dependency issues, and runtime startup crash diagnosis.
- Build and test the final restored hopwire, manhack, brickbat and six TF2 prototypes together; complete prediction/network, class/loadout, save/load and balance checks in actual gameplay.
- Implement genuine support/conversion for the installed incompatible model layouts and supply correct missing TF2 scripts/art; rewriting a version field or substituting an unrelated model cannot establish restoration.
- Verify each actual gameplay weapon's script, viewmodel, world model, companion layout, attachment points, animations, hand position, materials, UV/texture scale, HUD art identity and crosshair behavior using correct branch content.
- Test pickups, both attacks, reload, ammunition, inventory limits, repeated/full-inventory `impulse 101`, save/load, death/drop and multiplayer behavior in rebuilt games. Record failures and rerun affected tests after fixes.

## Reproducing checks

With Python and a usable C++ compiler in the developer environment:

```sh
python -m unittest discover -s modernize/tests -v
python modernize/validate.py
python phase7_manifest_check.py .
```

The gate currently reports valid but incomplete evidence and exits **1**, with **0/102 required gameplay candidates verified**, while retaining all 114 registered entries in its inventory. Exit 0 requires complete reported build/asset/runtime evidence; exit 2 denotes corrupt or inconsistent input. A passing report still requires trustworthy gameplay evidence; the validator does not launch a game.

For actual Windows builds, use an x86 Visual Studio developer environment:

```sh
python modernize/build_driver.py Release
python modernize/build_driver.py Debug
python modernize/run_allocator_smoke.py Both
python modernize/run_game_module_smoke.py HL2
```

Shader projects accept `HL2_MODERN_PERL` (or `HL2ModernPerl`) pointing to a modern Perl executable. SDK tools are discovered automatically; `HL2_MODERN_FXC` and `HL2_MODERN_D3DCOMPILER` can select explicit installed tools.

Keep this status file and the main README updated in place with every published code milestone. Git commits preserve history; do not create `PHASE7_Vxx.md` reports.
