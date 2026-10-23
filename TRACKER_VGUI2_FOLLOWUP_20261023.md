# GammaSource Win32 / Tracker VGUI2 follow-up (2026-10-23)

## Source

Base: `GammaSource_Tracker_VGUI2_Repairs_Full.zip` (prior revision). New diagnostic list: `Pasted text(20261023-051936).txt`, compared with older `Pasted text(20261023-045930).txt`. The diagnostic lists contain messages **without filename, line number, MSVC diagnostic code, or project name**, so this patch addresses independently confirmed mismatches. Warnings are not proof of build failure.

## Repairs made

- Added the **complete `VGUI_ListPanel.h` declaration** to `Tracker/AdminServer/BaseGamesPage.h` before it declares an `OurListPanel` subclass. This removes the root cause of the cascading `ListPanel base undefined`, `AddColumnHeader`, `SetSortFunc`, and other inherited-member errors from that translation unit.
- Removed bogus `class KeyValues;` declarations **inside the VGUI namespace** in AdminServer's `PlayerPanel.h`, `RawLogPanel.h`, and `ServerConfigPanel.h`. The real `KeyValues` is a global class; local duplicate declarations caused ambiguous types, bad overload resolution, and mismatched `CUtlVector<KeyValues*>` types.
- Added explicit `UtlVector.h` imports to `rcon.h` and `ServerList.h` for the `requests`, `m_Servers`, and `m_RefreshList` member containers.
- Changed `Button::SetCommand` in `BaseGamesPage.cpp` to the existing one-parameter `KeyValues*` overload. The button already calls `AddActionSignalTarget(this)` separately.
- Corrected the `serverpage.cpp` include to its actual local class declaration, `serverpage.h`, rather than a nonexistent `vinternetdlg.h` in the AdminServer directory.
- Corrected `GamePanelInfo.cpp` to use the `CChatPanel::DoInsertString` and `CMOTDPanel::DoInsertString` methods rather than nonexistent `InsertString` methods of these wrapper classes.
- Replaced obsolete `IPanel::Client()` uses in `AdminServer.cpp` and `BuddyButton.cpp` with VGUI2 `IPanel::GetPanel(..., GetControlsModuleName())` or the direct `IPanel::RequestInfo()` interface. The new `GetPanel` path is null-checked for cross-module panels.
- Updated `modernize/repair_tracker_vgui2_api.py` so the previous script no longer reverses the `GamePanelInfo` fix; added an idempotent follow-up script integrated after it in `apply_modernization.py`, plus regression tests.

## Verification

- **17 Tracker regression tests passed** with Python 3 (includes the original 11 and 6 new follow-up tests).
- `modernize/validate.py` passes its **project XML and configuration structural checks** (153 projects, 348 converted configurations).
- Both VGUI2 repair scripts are repeatable and leave the patched files unchanged on a second invocation.
- Source-level checks and ZIP verification do **not** substitute for a successful Win32 MSVC compilation, linking, or UI testing; none was available in this environment.
- The broader archive still reports **605 missing source references** in 66 projects under the validator. Some are outside the active build and need separate review.

## Still outstanding

- The compilation list includes legacy API and header issues without filename/line context. A full MSBuild `.binlog` or text output with file paths and MSVC error codes is required to accurately attribute every remaining problem.
- CRT deprecations and `/Zc:forScope-` are mostly warnings and were not blanket-suppressed; a format-string warning requires inspection of its exact callsite.
- Tracker's reconstructed interfaces are **not ABI-certified** against original historic binaries.
- Full Win32 run-time and game UI tests are not certified. Neither AMD64 settings nor Phase 7 gameplay code has been changed.

## Reapply

From extracted source root:

```bat
py -3 modernize\repair_tracker_vgui2_api.py
py -3 modernize\repair_tracker_vgui2_followup.py
py -3 -m unittest discover -s modernize\tests -p "test_tracker*.py" -v
```
