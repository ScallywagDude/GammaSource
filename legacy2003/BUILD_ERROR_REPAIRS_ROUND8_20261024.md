# GammaSource build-error repairs — Round 8

Base: `GammaSource_Beta_Core_Stability_Round7_Full_Source.zip`.
Target: Win32 Visual Studio C++ builds. Existing gameplay and beta assets are unchanged.

## Resolved source diagnostics

- `public/bitbuf.cpp`: `bf_write::WriteBitsFromBuffer` no longer calls the protected `bf_read::CheckForOverflow` method. It checks the public remaining-bit count and overflow flag, marks a truncated reader overflowed, and preserves the writer's failure behavior.
- `public/bitbuf.cpp`: `bf_read::ExciseBits` now explicitly casts its message storage to writable memory only within this in-place mutating method. Ordinary reads still use a `const unsigned char*`, preserving reader constness. Callers of `ExciseBits` must provide writable backing storage.
- `engine/net_chan.cpp`: replaces undefined `Q_strchr` with the standard `strchr` path validation function.

## Project build-order repairs

- 87 existing `.vcxproj` files gained 190 explicit build-only project references to local libraries they link: `tier0.lib`, `vstdlib.lib`, `vtf.lib`, `vgui_controls.lib`, and `mxtk.lib`.
- These references do not automatically inject libraries into the linker: `<LinkLibraryDependencies>false</LinkLibraryDependencies>`. Legacy linker arguments remain unchanged.
- `materialsystem/shaderdx8/shaderdx8_ihvtest.vcxproj` includes the bundled `dx9sdk/include` directory required by `d3dx9.h` (both Win32 configurations).
- `modernize/repair_project_dependencies_round8.py` reapplies those fixes when Visual Studio projects are regenerated. It is idempotent and is registered in `modernize/apply_modernization.py` after the other conversion repairs.

## Validation

- 27 focused regression tests passed, including C++98 bit-buffer compilation, memory-pool, fragment, VTF, ZIP, BSP visibility and project-dependency tests.
- `modernize/validate.py` passed structural project checks across 153 projects / 348 configurations.
- The original ordered Win32 Release plan (`modernize/build_win32_round9.py Release --dry-run`) contains 61 steps and passes planning validation.
- Project repair script repeated without additional modifications on its second run.
- Original beta HUD texture, gameplay and terrain files were not changed.

## Remaining limitations

- **No full Windows/MSVC compile was run in this environment.** Dependency references correct MSBuild ordering but do not prove that upstream projects compile and produce libraries.
- The full converted solution still contains 605 historical missing-source references across 66 projects. Structural validation reports those and is not a full build.
- Old Win32 objects without SAFESEH metadata may still require their replacement with rebuilt objects or the existing explicit opt-in `/SAFESEH:NO` compatibility setting. This repair does not silently weaken linker security.
- CRT deprecation warnings have not been globally suppressed or counted as resolved.

To build on Windows, from the source root, run `py -3 modernize/build_win32_round9.py Release --rebuild --keep-going` with the required Visual Studio components and shader tools available. The build driver saves per-project detailed logs for any additional source errors.
