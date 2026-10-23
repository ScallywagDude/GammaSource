# GammaSource Win32 Build Repair — Round 8 (2026-10-23)

Base: Round 7 full source. AMD64 and Phase 7 are unchanged.

## Source repairs

- `Tracker/ServerBrowser/BaseGamesPage.h`: `DECLARE_PANELMAP()` is protected so `IMPLEMENT_PANELMAP` can legally take the address of the inherited static map. This directly addresses the `CBaseGamesPage::m_PanelMap` private-access failure.
- `vguimatsurface/vguimatsurface.vcxproj`: removed duplicate `vgui_surfacelib.lib` staged-archive dependency. The configuration retains its direct build-output archive. The DSP-to-MSBuild converter applies the same rule on regeneration. This addresses the duplicate `CFontManager` link symbols in the supplied log where that project was responsible.
- Modernized VC6 `<fstream.h>` references in Worldcraft and `public/tokenreader.h` through a small C++98 compatibility header with normal `std::fstream`, `std::ifstream`, `std::ofstream` and `std::ios` aliases. Removed `ios::nocreate` when input mode already prevents creation, including implicit `ifstream` input mode. Updated the legacy `class fstream` declaration to a real type include.
- `modernize/build_windows.cmd` probes the existing Perl tool before shader builds so a missing Perl interpreter produces a single actionable failure rather than hundreds of repeated per-shader failures. Original `.fxc`, `.vsh` and `.psh` profiles remain unchanged.
- `Directory.Build.targets` adds an **opt-in only** Win32 flag `HL2ModernLegacyUnsafeSEH=true`, to support external x86 object files lacking SAFESEH metadata. This is *not* enabled by default, because `/SAFESEH:NO` reduces security hardening. Prefer rebuilding third-party objects with compatible SAFESEH metadata if possible.
- Added repeat-safe `modernize/repair_win32_round8.py` and regression tests, integrated after previous source repair passes.

## Build prerequisites and remaining blockers

- Install or select **Perl 5** (e.g. point `HL2_MODERN_PERL` at a local `perl.exe`) before invoking Windows shader builds. The script does not bundle Perl or generate placeholder shaders.
- The DirectX June 2010 SDK is needed for `d3dx9.h` in legacy source. It is a separate dependency, not part of current Windows SDK headers.
- An opt-in Win32 build can use `msbuild PROJECT.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:HL2ModernLegacyUnsafeSEH=true`, only if the corresponding objects cannot be rebuilt with SAFESEH support. Treat this as reduced mitigation and verify affected third-party libraries.
- `libci.lib` and `libcp.lib` are retired VC6 runtime libraries. Their provenance (input object or prebuilt library) must be identified from a verbose MSVC link log before changing the CRT linkage. Do not fabricate replacement libraries.
- `STAT_CHUNK`, `CHUNKSTATE`, `CSpDynamicString`/`SPPHONEID`, strict Windows structure packing and residual missing-type errors need compilation filenames/line numbers. They have not been certified fixed.
- Source/project checks can run on Linux, but **no full Visual Studio/MSVC build or runtime execution was performed**. Structural checks do not imply playability.

## Test commands

```bat
py -3 modernize\repair_win32_round8.py
py -3 -m unittest modernize.tests.test_win32_round8 -v
py -3 modernize\validate.py
py -3 modernize\run_shader_tool.py --probe
modernize\build_windows.cmd Release
```
