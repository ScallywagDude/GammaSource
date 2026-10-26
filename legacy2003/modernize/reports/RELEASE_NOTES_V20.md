# Half-Life 2 (2003) Modernization — v20

**Baseline:** the exact user-supplied `HL2_2003_Modernization_Source_v19.zip`. This release preserves the v18 asset plans, Phase 7 weapon integration, and the v18-v19 animated GameUI model-preview work. No original game art, proprietary SDK, or new binaries are added.

## Phase 8 — animated 3D multiplayer preview

- Fixed engine projection/view/model matrix state leaking out of the GameUI 3D preview into other rendering passes. A scoped helper now pushes and pops all three matrices and restores the prior fog mode even on future early returns. Note: the studio renderer also maintains separate global lighting/view state; that still needs in-game verification.
- Removed an ad hoc vertex-filename concatenation operation from the preview's installed-model check. The new C++98 shared helper derives only the actual Source 2003 `.dx80.vtx`, `.dx7_3bone.vtx` and `.dx7_2bone.vtx` companions from bounded, relative, null-terminated internal MDL names. The preview retains the renderer's normal variant selection and dx7 fallback.
- The HL2 server now requires at least one supported vertex companion in addition to a compatible v36 studio header before precaching or spawning a selected model. This does not prove animation/geometry compatibility and does **not** convert the examined installed v37 assets.

## Phase 7 — experimental `impulse 101` visual safety

- The existing branch-specific cheat weapon granting path now rejects weapons whose viewmodel or world model has no supported Source 2003 vertex companion. This check is applied in the existing `GiveCheatWeapon` preflight, not to ordinary weapon pickup/spawn behavior. It does not add unrelated branches' weapons.
- Missing, unported or newer-format assets continue to block those grants. This is intentional. The required geometry, material, UV, animation, sound, and original HUD artwork have **not** been fabricated or certified as fixed.

## Validation and limitations

- 153 Python regression tests run: 152 passed, 1 skipped, 0 failed.
- Five new v20 tests cover native C++98 companion-name validation, cross-module integration, rendering-state code paths, weapon-grant path gating, and patch-script idempotence.
- Structural Visual Studio project validation passes for 153 generated projects and 348 configurations; existing unverified/missing reference warnings remain.
- Windows C++ linking, multiplayer gameplay, the GPU preview, studio model rendering, weapon effects, and alternate graphics APIs were not exercised. **This is a source-tested incremental update, not a playable build or completion of Phases 7/8.**

Apply the v20 overlay only to the exact v19 source ZIP using `python modernize/apply_v20_overlay.py --base ... --overlay ... --output ...` (extract the applier from the overlay first) or use the full v20 source archive.
