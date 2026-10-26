# v18 animated multiplayer model preview

The HL2/HL2MP Options -> Multiplayer selector now has an actual studio-model rendering implementation. This is completed source integration with targeted compiler and offline tests; it is not a claim that a runnable engine build or the supplied player assets have passed visual testing.

## Implementation

- `public/IGameUIModelPreview.h` defines optional, separately versioned `GameUIModelPreview001`. Existing `IVEngineClient` and `IGameUIFuncs` vtables are preserved. GameUI requests this provider and the existing `IMatSystemSurface` extension during initialization.
- `gameui/PlayerModelPreviewPanel.cpp` paints inside the material surface's existing `Begin3DPaint`/`End3DPaint` boundary. It submits the actual selected MDL to the engine provider. No portrait texture is used in the HL2 3D path.
- `engine/gameui_modelpreview.cpp` loads the selected studio model through the engine model loader, evaluates its compiled idle sequence with the shared `InitPose`, `CalcPose`, and `CalcAutoplaySequences` implementation, builds the animated bone matrices, and calls the existing `IStudioRender::DrawModel` using that model's real hardware data and original materials. It supplies ambient and directional lighting, neutral flex weights, and no forced replacement material.
- Idle selection prefers `ACT_IDLE`, then a looping idle/stand label. An asset without an idle uses its bind pose and explicitly reports the missing idle; it does not substitute an attack/death animation or another model. Missing geometry produces an error status. The Options page refreshes this status every visible frame, including failures found during first rendering.
- Left mouse dragging rotates the model. Mouse capture is released on button-up and when the selection is cleared. Automatic rotation resumes after two seconds without dragging, at 18 degrees/second. Frame deltas are bounded after returning from a hidden tab.
- A bounding sphere containing the model/idle bounds determines camera distance. An aspect-correct perspective projection preserves proportions in tall and wide panels. The 2003 backend's viewport-scoped depth clear is used; the VGUI surface restores the model/view/projection matrices and viewport afterward. Partially clipped preview panels are skipped because this surface's 3D drawing bypasses normal VGUI clipping.
- The page updates the provider immediately on model-selection messages. Invalid or empty selections clear prior geometry. Apply continues through `CLabeledCommandComboBox::ApplyChanges`; the existing `model` cvar is archived and sent as userinfo, preserving the existing server-side policy.
- `IModelLoader::FMODELLOADER_GAMEUI` is a separate reference bit, included in the loader reference mask. Clearing the preview releases only its references. The provider keeps a model path rather than a potentially stale pointer across map changes. GameUI shutdown clears its references.
- Both original `.dsp` and migrated `.vcxproj`/`.filters` projects contain the new source/header inputs. The engine preview source and shared `bone_setup.cpp` are excluded from both dedicated configurations. The three Options message-map entries use explicit member-function addresses so that this changed unit passes the installed modern compiler.

## Availability and asset limits

The existing finite HL2/HL2MP list remains the authority: 46 potential models. HL1, Counter-Strike, and TF2 retain their existing branch-specific selection behavior and are not populated with the HL2 list. This v18 animated panel is scoped to HL2/HL2MP.

The updated engine filters the list using an actual `IDST` header, this tree's supported MDL version 36, declared file length, bone/animation counts, and the 2003 vertex-file naming rules used by `Mod_LoadStudioModelVtxFileIntoTempBuffer`. These are `.dx80.vtx`, `.dx7_3bone.vtx`, and the `.dx7_2bone.vtx` fallback. Retail `.vvd`/`.dx90.vtx` assumptions are not substituted for this source format.

Before loading, the provider reads the declared MDL bytes and checks ranges for the principal studio tables, relative bone/sequence/animation/material strings, bone parent ordering, animation channel/RLE spans, sequence animation indices/weights/layers, pose parameter tables, and model/mesh vertex windows. File size is capped at 64 MiB for this UI. This preflight is defensive format checking, not a comprehensive validator of all MDL/VTX semantic fields or a guarantee against every malformed local asset; the existing studio loader remains responsible for loading vertex data and materials.

The source archive itself contains no compiled player MDLs, VTX files, or player texture sets. The separately inspected local asset roots expose three matching candidates (`player.mdl`, `combine_soldier.mdl`, `police.mdl`), all MDL version 37. Their layout compatibility with this version-36 source is not established, and the v18 preview deliberately does not pass them to the loader. Other allowlisted paths are missing from the inspected roots. No proprietary player model, skeleton, material, texture, or SDK binary has been copied into the deliverable. See `MULTIPLAYER_MODEL_AUDIT_V18.md`, `ASSET_VALIDATION_V18.json`, and `MISSING_ASSETS_V18.md` for per-file content results.

The implementation therefore has no compatible original player model on which to verify appearance, UVs, skin weights, animation quality, material textures, networking, or in-game model replacement. Supplying a legally usable, compiled version-36 player model plus its matching 2003 VTX/material/texture dependencies remains necessary for visual runtime verification. The bind-pose error path is not presented as completed idle animation.

## Actual validation results (2026-10-22)

1. **PASS:** five new preview architecture tests and two updated prior preview regressions (`7 tests`). They check provider/consumer interface matching, preservation of existing ABI surfaces, project inputs and dedicated exclusions, actual studio draw/animation calls, branch and file gates, archived selection apply, and reference ownership/stale-geometry clearing.
2. **PASS:** native `model_preview_smoke.cpp` compiled and executed with the installed MSVC x86 toolchain. Assertions cover negative/stalled frame time, large and negative yaw, idle-cycle wrapping, invalid dimensions, sphere visibility at four viewport aspect ratios, and proportional camera distance when model size doubles.
3. **PASS:** native `model_preview_studio_smoke.cpp` compiled and executed with the same toolchain. A synthetic in-memory format fixture is accepted; out-of-range bone/animation/sequence/pose/body-model references, invalid parent/animation indices, and a zero-length RLE span are rejected. This fixture is test code, not replacement model content.
4. **PASS:** targeted MSVC `/Zs` syntax checks for `engine/gameui_modelpreview.cpp`, `game_shared/bone_setup.cpp`, `gameui/PlayerModelPreviewPanel.cpp`, `gameui/OptionsSubMultiplayer.cpp`, and `gameui/GameUI_Interface.cpp` under Debug preprocessor settings. Warnings remain in legacy code (CRT names, `ARRAYSIZE` redefinition, and `KeyValues` allocation/delete pairing). No binaries were produced by `/Zs`.
5. **PASS:** replayed all seven existing model/UI generators with SHA-256 comparisons of their source targets. `patch_player_model_selection.py`, `expand_hl2_models.py`, `patch_preview_status.py`, `patch_model_dropdown_integrity.py`, `patch_model_path_policy.py`, `patch_preview_bitmap.py`, and `patch_multiplayer_model_ui_safety.py` produced no source drift. The two portrait transforms explicitly recognize the superseding animated-preview marker.
6. **NOT VERIFIED:** full GameUI/engine linking, Release compilation, GPU draw results, tab geometry at each installed resolution/resource layout, device-loss recovery, and selection propagation in an actual multiplayer session. An initial Release-like syntax attempt exposed existing const-return incompatibilities in `public/vstdlib/strtools.h`; the successful checks use `_DEBUG`, matching that header's alternate branch. No full Windows project build or game launch is claimed.

The two native smoke-test executables and object files were deleted after execution. Only their source and the validation record are delivered.

## Reproduction commands

Activate the installed VS x86 compiler environment with `VC/Auxiliary/Build/vcvars32.bat`, then run from `engine/`:

```text
cl /nologo /Zs /W3 /EHsc /I..\public /I..\common /I.\audio\public /D_DEBUG /DWIN32 /D_WIN32 /DENGINE_DLL /DUSE_CONVARS /DBUMPMAP /D_CRT_SECURE_NO_WARNINGS gameui_modelpreview.cpp ..\game_shared\bone_setup.cpp
```

From `gameui/`:

```text
cl /nologo /Zs /W3 /EHsc /I..\public /I..\common /D_DEBUG /DWIN32 /D_WIN32 /DGAMEUI_EXPORTS /D_CRT_SECURE_NO_WARNINGS PlayerModelPreviewPanel.cpp OptionsSubMultiplayer.cpp GameUI_Interface.cpp
```

From the source root:

```text
python -m unittest modernize.tests.test_model_preview_v18 modernize.tests.test_phase7_phase8_v16.ScopedPhases.test_preview_status_not_false_3d modernize.tests.test_phase7_phase8_v17.Phase7Phase8V17.test_unselected_model_clears_previous_geometry -v
```

From `modernize/tests/`, place compiler outputs outside the final source package:

```text
cl /nologo /W4 /EHsc /Fo<temporary-directory>\preview_math.obj /Fe<temporary-directory>\preview_math.exe model_preview_smoke.cpp
<temporary-directory>\preview_math.exe
cl /nologo /W4 /EHsc /MDd /D_DEBUG /D_CRT_SECURE_NO_WARNINGS /I..\..\public /Fo<temporary-directory>\preview_studio.obj /Fe<temporary-directory>\preview_studio.exe model_preview_studio_smoke.cpp
<temporary-directory>\preview_studio.exe
```
