# GammaSource Win32 Tracker build repairs - round 6 (2026-10-23)

Baseline: `GammaSource_Win32_Build_Repairs_Round5_Full.zip`.

Scope: The remaining errors reported for VGUI accessors (`ivgui`, `filesystem`, `surface`, `system`), an abstract `CFavoriteGames`, and associated legacy Tracker declarations. No AMD64 configuration or Phase 7 weapon/gameplay files are changed.

## Changes

- `Tracker/AdminServer/BaseGamesPage.h` and `.cpp`: implement the pure `IGameList::GetInvalidServerListID()` interface using the AdminServer list's own invalid-row sentinel. This resolves one concrete reason `CFavoriteGames` is abstract.
- `Tracker/ServerBrowser/{BaseGamesPage,DialogGameInfo,InternetGames,ServerContextMenu}.cpp`: include `<vgui_controls/Controls.h>` to provide declarations for VGUI free accessor functions. Merely including `IVGui.h`, `ISystem.h` or `ISurface.h` defines interface types, not the accessor functions.
- `modernize/repair_tracker_win32_accessors_round6.py`: idempotent repair script, wired into `modernize/apply_modernization.py` after existing scripts.
- `modernize/tests/test_tracker_win32_accessors_round6.py`: regression checks covering interface contract, headers, regeneration and repeatability.

## Tests run

- `python -m unittest discover -s modernize/tests -p 'test_tracker*.py' -v`: 22 tests passed.
- `python modernize/validate.py`: structural project validation passed, 153 projects, 348 configurations.
- Running `repair_tracker_win32_accessors_round6.py` twice makes no further changes on the second run.

## Not yet verified

- This Linux execution environment cannot perform the required Windows MSVC, DirectX SDK and dependency-library builds. The 605 missing references reported by the full-tree structural auditor span inactive and unrelated historical projects and remain unresolved.
- The compiler diagnostics supplied without filenames, lines and numeric error codes cannot be mapped reliably to every source line. Other missing-type or API errors may remain.
- Deprecated CRT warnings are not the same as build-stopping errors. These are not globally suppressed or changed without checking bounds and behavior.

Build the Win32 Release configuration on Windows, then share complete MSBuild output with file paths, line numbers and diagnostic codes for any remaining failures.
