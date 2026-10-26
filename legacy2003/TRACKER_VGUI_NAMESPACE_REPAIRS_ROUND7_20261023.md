# GammaSource Win32 / Tracker VGUI2 namespace repair — round 7 (2026-10-23)

Baseline: `GammaSource_Win32_Build_Repairs_Round6_Full.zip`.

## Reproduced root cause

`public/vgui/VGUI.h` maps `vgui` to `vgui2` with a preprocessor macro. The legacy Tracker forwarding header `modernize/tracker_vgui_compat/VGUI_Controls.h` formerly included `public/vgui_controls/Controls.h` **before** that macro was guaranteed to exist. Consequently `Controls.h` could declare `system()`, `ivgui()`, `filesystem()`, `surface()` and related accessors in `namespace vgui`, while callers later compiled `vgui::system()` as `vgui2::system()`.

A C++98 Clang syntax check that included the actual legacy wrapper before the actual VGUI header reproduced four missing `vgui2` accessor declarations. The same check passes after the patch. Only unrelated engine platform headers were bypassed by test-level include guards, not the VGUI declarations.

## Code changes

- `modernize/tracker_vgui_compat/VGUI_Controls.h` now includes `<vgui/VGUI.h>`, the complete VGUI key/mouse enums, then `<vgui_controls/Controls.h>` in the correct order.
- 59 Tracker `.cpp` caller files directly include `<vgui/VGUI.h>` before their other headers. This ensures the namespace alias is active even where an indirectly included control header runs earlier than the old forwarding shim. Engine/VGUI2 public headers, game code, and non-Tracker projects were not edited.
- `modernize/repair_tracker_vgui_namespace_round7.py` is idempotent and called by `modernize/apply_modernization.py` after the previous Tracker Win32 repairs.
- `modernize/tests/test_tracker_vgui_namespace_round7.py` adds namespace ordering, syntax-only C++98 header compilation, and rerun regression checks. The legacy wrapper test now explicitly recognizes the necessary multi-header include sequence.

## Validation

- 27 Tracker tests passed, including five new namespace tests.
- Portable Clang `-std=c++98 -fsyntax-only` test of real VGUI headers passed.
- Prior Tracker repair scripts made zero further changes on rerun; round 7 script also made zero changes.
- Visual Studio structural audit: 153 project files and 348 converted configurations validated; **not Windows compilation**.

## Remaining limitations

Full Win32 MSVC compilation, link/runtime behavior, and the user's unspecific `missing type specifier` errors are **not certified resolved** without a build log including `file(line): error C####: ...` details. Historical CRT deprecation messages are warnings, not proof of a failed build, and have not been hidden. AMD64 configurations and Phase 7 are unchanged.
