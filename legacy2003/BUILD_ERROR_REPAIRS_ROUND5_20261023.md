# GammaSource Win32 compiler repairs — October 23, 2026 (Round 5)

Baseline: `GammaSource_Tracker_VGUI2_Followup_Full.zip`. This round responds to
`Pasted text(20261023-053704).txt` (~14,952 diagnostic lines, including repeated warnings).

## Implemented in this source snapshot

- QHTM `DebugHlp.h`: removed old typedefs for `LPCSTR`, `LPCWSTR`, and `BOOL` that redefined Windows SDK types, including the incompatible `unsigned short*` interpretation of `LPCWSTR`.
- QHTM `Map.h`/`Map.inl`: fixed malformed `BucketEntry` copy assignment, made the dependent `BucketEntry` return type explicit, and moved the method implementation below helper declarations for standards-conforming name lookup.
- QHTM `MapIter.h`/`MapIter.inl`: fixed template dependent-type declarations and initialize `m_pNext` when resetting an empty map.
- QHTM `WinHelper.h`: allow normal copy construction of `WinHelper::CSize`, including return-by-value paths.
- All nine QHTM `.vcxproj` files: inserted project-root and QHTM implementation include paths into compiler configuration groups. This addresses a broad subset of missing `QHTM_Includes.h`, `DebugHlp.h`, `WinHelper.h`, `ImgLib.h`, `TextABC.h`, and `Map.h` errors while retaining the original project include paths.
- TFStats `timeindexedlist.h`: added `<map>` and repaired dependent nested `iterator`/`accumulator` type declarations for C++98 compilers.
- Added VGUI2 controls accessor declarations to three Tracker ServerBrowser compilation units that use `vgui::system()`, `vgui::ivgui()`, etc., without including `vgui_controls/Controls.h`.
- Added idempotent repair scripts to `modernize/apply_modernization.py` so source overlay application reapplies these fixes.

## Checks performed

- Portable C++98 QHTM hash-map and iterator compile-and-run test passed under GNU C++. It uses a small Win32 shim and `-fpermissive` for an unrelated legacy pointer-width cast on the Linux host, NOT MSVC.
- Portable C++98 TFStats time-indexed list compile-and-run test passed under GNU C++, with the TFStats application dependency stubbed.
- New six-test regression suite passed, including repeat-run idempotence and XML project configuration checks.
- Existing 17 Tracker regression checks passed.
- `modernize/validate.py` passed: 153 generated projects / 348 configurations; **structural only**.

## Unresolved / not claimed

- **No Windows MSVC x86 compilation or application/runtime test was performed.** These changes do not imply all compile errors in the report are fixed.
- The 345 repeated `Perl was not found` diagnostics require a supported Perl executable or a functioning replacement shader generation toolchain. This environment cannot provide the Windows executable.
- `STAT_CHUNK` / `CHUNKSTATE` identifiers are absent from the checked-in C++ source as searched; they appear to originate from unavailable/mismatched declarations or an external dependency. Needs diagnostics with file, line, project and command line to identify definitively.
- Older VGUI2 API differences (`TextEntry`, `HTML`, `IPanel`), `CFavoriteGames` abstract-class errors, missing `vgui_controls.lib`, and other errors need additional source-specific fixes and a Windows build.
- `validate.py` reports 605 missing source references across 66 projects in the full archive. These have not been fabricated or marked resolved.
- Generic CRT deprecation diagnostics are warnings and have not been globally suppressed.
- AMD64 configurations and Phase 7 are unchanged by this round.

## Reapply and check

```bat
py -3 modernize\repair_qhtm_win32.py
py -3 modernize\repair_tfstats_templates.py
py -3 modernize\repair_tracker_controls_accessors.py
py -3 -m unittest discover -s modernize\tests -p test_qhtm_tfstats_win32.py -v
py -3 modernize\validate.py
```

For a precise follow-up MSVC error-fix pass, export **Build Output** (not just Visual Studio's Error List), including project name, filename, line number, error code, command line, and the earliest errors for each project.
