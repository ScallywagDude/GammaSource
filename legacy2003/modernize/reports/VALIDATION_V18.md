# v18 validation results

The final offline suite passed: **144 tests run, 143 passed, 1 skipped, 0 failures/errors**. The complete modernization replay also passed with **zero protected-source hash changes**. This establishes source/configuration and narrowly tested native-policy properties. It does not establish a successful full Windows game build or a playable game.

## Actual checks

| Check | Actual result | Evidence |
|---|---|---|
| Full regression suite | PASS: 144 run, 143 passed, 1 skipped | `VALIDATION_RESULTS_V18.json`, `validation/regression_tests.log` |
| All modernization scripts replayed | PASS; zero protected-source drift | `validation/replay.log`, final `source_drift: []` |
| Converter preservation | PASS: portable filter IDs, canonical solution ordering, exact CRLF output, original DSP staging metadata | Three added conversion regression tests; both solutions, build command and mastertest project match v17 bytes |
| VS project/solution structure | PASS: 153 projects, 348 configurations, 19 shader custom-build entries | `validation/structure.log`, `modernize/validation_report.json` |
| Branch registrations/build inclusion | PASS as inventory: HL2 32, HL1 15, CS 28, TF2 38 | Branch/build inventories and `validation/weapon_*.log` |
| Native policy tests | PASS with actual compiled executables and strict C++98 flags | Regression log; test-only C-runtime bridges avoid the new MSVC STL's newer-language requirement |
| 3D preview native fixtures | PASS: framing/animation and defensive studio-byte validation executables | `MULTIPLAYER_PREVIEW_VALIDATION_V18.json` |
| Preview translation-unit syntax | PASS: five Debug MSVC `/Zs` units | `MULTIPLAYER_IMPLEMENTATION_V18.md` exact commands |
| Server/weapon syntax probes | BLOCKED: 11 probes, 0 successful units | Fresh `weapon_syntax_v18.json` with exact compiler commands and diagnostics |
| HLSL experimental shader assets | PASS: D3D11 VS/PS and D3D12 VS/PS, actual FXC/DXC outputs | `SHADER_VALIDATION_V18.*`, `modernize/shaders/compiled_v18/provenance.json` |
| OpenGL/Vulkan shader validation | BLOCKED: four glslang jobs lack the compiler | Shader evidence and final `validation/shader_plan.log` |
| Shader source includes | PASS: all 129 inspected includes resolve | `ASSET_VALIDATION_V18.json` shader section |
| HUD pixel decoding | PASS: all 22 referenced atlases, 107 mip/frame images | `HUD_PIXEL_AUDIT_V18.*`; all top frames independently compared to Pillow DDS decoding |
| Local asset repairs | PASS: 5,281 edited files, 6,382 edits; originals and output hashes verified | `ASSET_REPAIRS_VALIDATION_V18.json`, combined minimal plan and patch |
| Archive CRC and overlay reproduction | Verified by the release packager for actual delivered archives | External `HL2_2003_Modernization_Checksums_v18.json`, embedded release/change manifests |

The sole skipped regression uses a Unix-only fake compiler and is not executable on this Windows host. No failed native policy check was relabeled as a skip: fixtures were adapted to the real C runtime while retaining C++98 flags and runtime assertions. The spawn-protection header's observed `MAX_CLIENTS` macro collision was fixed without changing its 64-player policy and is covered in the native fixture.

The project-structure pass still records 605 missing references across 66 projects in the all-project conversion inventory; many belong to unavailable optional/original project inputs. The primary HL2 build-order audit reports no missing compilation inputs, but that does not resolve legacy compiler incompatibilities. The eleven real server probes remain blocked by legacy headers/compiler assumptions, including const-return wrappers, x86 assembly and existing datamap/API usage. No linking pass, engine build or game launch occurred.

## Asset content and limits

The v17 ZIP contains no game MDL/VVD/VTX/PHY/QC/SMD/VMT/VTF, weapon-script or sound payload. Original game geometry, rigs, materials, hands, UVs, animations and artwork cannot be reconstructed faithfully from classnames and C++ alone. No placeholder is represented as finished original content.

Original local game directories were audited read only. `ASSET_VALIDATION_V18.json` and the top-level `MISSING_ASSETS_V18.*` are the **before-repair baseline**. The final composed local overlay is `asset_restoration_v18` beside the source folder. Its re-audit and the actual **remaining** dependency findings are under `repaired_assets/`. The unchanged originals were verified by SHA-256; the edited overlay is deliberately not installed automatically or distributed as proprietary payloads in the source archives. Minimal token plans reproduce it from the exact local originals.

All primary cut-NPC models inspected locally report studio version 37; the supplied source declares version 36. The audit deliberately does not guess newer table layouts. The new preview rejects unvalidated model versions before loading. Three matching player candidates were present but unvalidated; most of the 46 allowlisted paths were absent. Version-field overwriting in the old loader does not establish compatibility. Materials/skeletons/sequences inside those newer models remain unverified even when filenames exist.

The original pixel audit found three blank HL1 autoaim regions. The final overlay uses each weapon's existing nonblank regular reticle in the same material and dimensions, with six explicit coordinate-token changes. This is a documented fallback, not restored original autoaim artwork. Hopwire's existing nested 640 metadata is now consumed correctly; its three raster crops contain nonblank pixels. Font-only script entries remain source-unsupported and explicitly unverified; script-only weapons are not promoted into the active branch inventory.

The actual post-repair HUD audit reports 212 nonblank HL2 raster crops and 64 nonblank HL1 crops, with no blank raster crops. All 21 font-only rows belong to source-unregistered script entries and remain explicitly unsupported. The post-repair full dependency audit retains 684 distinct branch/path/state findings: 275 missing references, 138 case differences (including embedded source paths), 121 unsupported model-version paths, 91 references in unprovided branches, 53 invalid paths and 6 VMT parse failures. Counts describe these supplied search roots, not a complete game installation. The stricter material-syntax checker separately lists seven ambiguous unrepaired VMTs, including a structure the general KeyValues parser can accept without proving its material meaning.

VTF header/payload and HUD decoding checks do not certify artwork identity or GPU appearance. VMT syntax repairs do not certify shader/proxy behavior or transparency. Seven ambiguous malformed VMTs were left untouched; their exact paths are recorded. Unreferenced files are candidates outside this dependency closure, never classified safe to delete. Packed archives, map contents and dynamic event/path construction are outside the complete static closure.

## Runtime verification still required

- Full Windows engine/GameUI/game-DLL compilation, linking and launch.
- Weapon spawning, selection, ammunition, firing, reload/holster behavior and replication in every branch.
- Correct compatible view/world models, UV/hand positioning, proportions, origins/FOV, materials, attachment transforms and animation quality.
- NPC spawning, combat/AI, sounds, hitboxes and complex aquatic/experimental behavior.
- Actual 3D viewport output, idle animation appearance, input/rotation, display layouts, device loss and multiplayer selection propagation.
- Original DX9 shader permutations and GPU appearance; complete modern render backends remain unavailable.

Run the suite from the source root:

```text
python modernize/run_validation_v18.py --verify-replay --compiler-bin "<directory containing clang++>"
```

Raw logs, per-file content states, compiler diagnostics and artifact hashes are included so a pass can be assessed at its actual scope.
