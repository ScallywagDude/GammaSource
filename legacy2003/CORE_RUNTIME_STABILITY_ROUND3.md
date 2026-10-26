# GammaSource engine stability — Round 3 (Win32)

This increment protects the original 2003 BSP/map-loading and visibility code. It does **not** replace gameplay, weapons, rendering, ground deformation, the 2002 bitmap HUD, or legacy resource formats.

## Fixed

- Validate the size and cluster-count of visibility tables before reading variable-length `bitofs` entries. Reject truncated tables during map loading, instead of indexing beyond the allocation.
- Verify compressed visibility offsets and all run-length reads. For absent/corrupt rows, conservatively report all surfaces as visible instead of crashing or culling valid geometry. Preserve original output for valid rows, including trailing zero-run truncation.
- Remove unconditional debug breakpoints on invalid visibility cluster inputs; guard output rows against out-of-range cluster counts and unsupported PVS/PAS selectors.
- Validate BSP file headers, lump offsets, lump sizes, element/subrange arithmetic, and read completion before using loaded bytes. Offset checks also cover the original displacement-lump retrieval paths, without changing displacement behavior.
- Zero-terminate the spare byte already allocated for BSP lumps. This prevents accidental reads into uninitialized memory when text lumps lack a terminator.

## Validation

Run from the repository root:

```text
python -m unittest modernize.tests.test_bsp_visibility_stability modernize.tests.test_bitbuf_stability modernize.tests.test_network_packet_limits modernize.tests.test_netchan_fragment_lifetime modernize.tests.test_megabuild_integration -v
python modernize/validate.py
```

The new portable C++98 harness compiles and runs under AddressSanitizer and UndefinedBehaviorSanitizer when Clang is available. It exercises thousands of valid compressed visibility rows against a reference and 30,000 corrupted-data cases. Source-level tests check the guard ordering; a bytewise comparison with the Round 2 source verifies that the beta HUD, gameplay, materials, viewmodels and displacement implementation are unchanged.

Structural project validation is not an MSVC build. The larger solution still contains 605 historically missing source references. Full Windows compilation, map loading with a broad set of original beta BSPs, performance benchmarks, long-session leak testing and modern Source parity are **not certified**. Reach performance and stability parity only after repeatable measurements and in-game regression runs on both builds.

## Scope

Only the BSP visibility/map-loading source, two small header-only helpers, this report, and a regression test are changed relative to Round 2. There are no switches that disable beta features and no changes to wire formats or version signatures. AMD64 and the paused weapon-restoration phase are still out of scope.
