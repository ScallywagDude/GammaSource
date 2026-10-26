# GammaSource core runtime stability — Round 4

This pass improves the original materials/resource-loader and shared serialization code. The built-in 2002 bitmap HUD, 2003 weapons, terrain deformation, beta content, menu/UI and wire formats are unchanged. Win32 remains the target; AMD64 and deferred weapon restoration were not changed.

## Changes

- Require a complete VTF header before precaching or loading a material. Close handles on every early-error path.
- Check VTF file lengths against the actual filesystem entry, before allocating the input buffer or consuming a purportedly complete image. Reject short file reads instead of decoding stale/uninitialized memory.
- Keep the `CUtlBuffer` write cursor consistent with the entire successfully loaded VTF, not just its header. Do not access an end-of-buffer pointer for empty payloads.
- In `CUtlBuffer`, validate read/write spans with non-overflowing arithmetic. The raw `Get()` method now marks invalid reads as failures instead of relying on a debug-only assert, while retaining the original external-buffer behavior and wire representation for valid data.
- Reject structurally invalid VTF frame counts, dimension values and image-format IDs before later decoder operations.
- Correct the on-disk six-face size for VTF 7.0 cubemaps. VTF 7.1 cubemaps retain their existing seven-face layout.
- Avoid a debug assertion when asked to skip all available mip levels.

## Verification

Run from the `GammaSource-main` directory:

```
python -m unittest modernize.tests.test_texture_resource_stability modernize.tests.test_bsp_visibility_stability modernize.tests.test_bitbuf_stability modernize.tests.test_network_packet_limits modernize.tests.test_netchan_fragment_lifetime modernize.tests.test_megabuild_integration
python modernize/validate.py
```

The four new regression tests compile/run the actual `CUtlBuffer` methods and the actual `CTexture::LoadTextureBitsFromFile()` method in C++98 harnesses, using AddressSanitizer/UndefinedBehaviorSanitizer and simulated complete, short and truncated texture reads. The protected legacy components are checked byte-for-byte against the previous archive. The Round 3 preservation test now permits changes to the engine's `materialsystem` because material/resource stabilization is the explicit scope of this round; protected HUD, weapons, content and displacement files are still covered.

**Not certified:** Windows/MSVC builds, rendering correctness on an actual GPU, map and asset-loading with the complete Mega Build, benchmarks, or full stability/performance parity with a later Source release. Those require an instrumented Win32 build and matched runtime tests. The structural project audit is not a compiler run and continues to report historically absent auxiliary sources.
