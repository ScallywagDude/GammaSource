# GammaSource core runtime stability — Round 5

This pass repairs the original `CBaseFileSystem` archive index and ZIP/PAK-file access. It intentionally does not change the built-in 2002 bitmap HUD, original beta weapons, menus, maps, assets, gameplay, terrain deformation, or rendering features. The Win32 build target and existing AMD64 deferral are unchanged.

## Source changes

- Give each packed-file handle an independent logical read/seek position instead of relying on a `FILE*` cursor shared across all members of the same archive. Interleaving reads of different packed assets no longer mixes their data.
- Enforce each member's declared length for reads and seek operations, including `FILESYSTEM_SEEK_TAIL`, and reject invalid positions/physical offsets. Preserve original read/seek behavior for loose files.
- Fix an archive mounting call that passed the underlying `FILE*` where a `CFileHandle` was required. Fix open-failure cleanup and avoid touching a freed search path after removal.
- Detect truncated `.bsp` headers before using their data to locate embedded ZIP files.
- Limit end-of-central-directory searches to the ZIP-defined 65,535-byte comment window instead of scanning entire archives on failure.
- Reject overlong packed-entry names, unsupported compressed/encrypted entries, invalid ZIP signatures, incomplete headers, impossible offsets, out-of-range payloads, and unrealistic directory counts. Valid stored ZIP members keep their contents and paths unchanged.

## Reproduction and verification

From `GammaSource-main`:

```
python -m unittest modernize.tests.test_packfile_runtime_stability modernize.tests.test_texture_resource_stability modernize.tests.test_bsp_visibility_stability modernize.tests.test_bitbuf_stability modernize.tests.test_network_packet_limits modernize.tests.test_netchan_fragment_lifetime modernize.tests.test_megabuild_integration
python modernize/validate.py
```

The four new tests compile and run actual filesystem methods in portable C++98 harnesses under AddressSanitizer and UndefinedBehaviorSanitizer. They exercise interleaved reads and seeking between real stored ZIP members, malformed headers and filename lengths, embedded ZIPs, unsupported compression, and a long invalid stream. These are focused unit tests, not a full Windows engine build.

## Limits

No full MSVC compilation, in-game load testing, multiplayer load testing or GPU benchmark was performed in this environment. The structural project validator still reports historical missing references in auxiliary projects. A full claim of parity with later engine branches requires matching gameplay and frame-time benchmarks on Windows. ZIP/PAK support here deliberately preserves the original engine's uncompressed-file requirement; the importer must not treat unsupported compressed ZIP entries as valid assets.
