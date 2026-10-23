# GammaSource Win32 build-error repairs — 2026-10-23

**Scope:** C++ source compilation defects in the user's pasted build log. AMD64 configuration and Phase 7 development are deliberately untouched.

## Changed source files

* `Tracker/AdminServer/BaseGamesPage.h`: expose the message-map declaration to derived pages rather than leaving it private.
* `utils/hlfaceposer/{choreoview.cpp,phonemeeditor.cpp,PhonemeEditor_CloseCaption.cpp,mxexpressiontray.cpp}`: qualify callback member pointers with `&Class::Method` instead of the non-standard implicit VC6 syntax.
* `utils/hlfaceposer/phonemeeditor.cpp`, `utils/hlfaceposer/mdlviewer.cpp`, `master/hlmasterdlg.cpp`, `master/hlmodmaster/hlmodmasterdlg.cpp`: correct tokens such as `"*"WORD_DATA_EXTENSION` and `"__DATE__` that MSVC treats as invalid user-defined literal suffixes.
* `utils/NormalMapper/{NmFileIO.cpp,ArgFileIO.cpp,TGAIO.cpp}`, `utils/mxtk/src/win32/mxwindow.cpp`, `utils/vrad/vraddll.cpp`: replace unavailable standard-library headers (`fstream.h`, `iostream.h`, `ostream.h`) and remove an unused `strstrea.h` include; keep `std::cerr` behavior.
* `utils/hlfaceposer/addsoundentry.cpp`: correct two invalid `va()` diagnostic formats.

The idempotent script `modernize/fix_legacy_compiler_errors.py` re-applies verified repairs to a *fresh copy* of the exact uploaded source and refuses unexpected input. New smoke tests live at `modernize/tests/test_build_error_source_repairs.py`.

## What this does NOT certify

The original pasted list is 9,726 lines of errors/warnings without file/line/project fields. It contains hundreds of repetitions of the same problems. These edits repair specific source patterns, not the entire build, and the machine used for the edits does not have MSVC, proprietary third-party libraries or a running game.

Unresolved Windows build blockers identifiable in the supplied log:

* **Perl missing (269 repeats).** Install a current Perl and ensure `perl` is on `PATH`, or point `HL2_MODERN_PERL` to a valid `perl.exe`. Verify using `py -3 modernize\run_shader_tool.py --probe`. The shader tool already supports this setting. This is an environment prerequisite, not source syntax.
* **Missing built libraries** (`vstdlib.lib`, `vgui_controls.lib`, `game_controls.lib`, `vtf.lib`). Build producers first, using matching Release/Debug and architecture, and inspect their first compiler/linker failure before building dependents. Do not copy unrelated prebuilt libraries to satisfy a linker by name. All-project solution builds are not a substitute for an ordered dependency build.
* **Missing `d3dx9.h`.** This is a legacy DirectX SDK dependency. Install an authorized compatible SDK and configure its include/lib paths; do not replace missing declarations with dummy headers.
* **Windows pack-alignment assertion.** Identify which project/source is compiling with a nondefault packing option or leaking a `#pragma pack`. Do not suppress `WINDOWS_IGNORE_PACKING_MISMATCH` globally; the ABI warning concerns actual memory safety.
* **Other legacy compatibility errors:** obsolete Speech SDK conversions; unnamed missing type specifiers; `STAT_CHUNK`/`CHUNKSTATE` syntax cascades; iterator-return and const-correctness failures. The text-only error list lacks locations needed to patch these safely.
* **Deprecated CRT APIs, deprecation notices, and narrowing warnings:** not evidence of a failed build by themselves. Do not globally disable them and claim a successful compilation.

## Tests executed in the editing environment

- Source checks: `python -m unittest modernize.tests.test_build_error_source_repairs -v`
- Structural Visual Studio workspace validation: `python modernize/validate.py`
- Build order plan: `python modernize/build_driver.py Release --dry-run`
- Full cross-platform suite: 265 tests, 10 errors, 6 skipped. The observed errors are from UTF-8 decoding of legacy 8-bit text, a git-history-dependent test running on a downloaded ZIP without `.git`, Windows path monkeypatching on Linux, and hard-coded CRLF text. They do not demonstrate a clean Windows build and have not been relabeled as passing.

For an actionable next build log, open a VS2026 **x86 Native Tools** prompt, build an individual failed project and capture its full `/v:diag` output or a binlog (`/bl:build.binlog`), preserving filename, line number, project and error code. Start with foundation libraries and the earliest *compiler* failure. Use the ordered `modernize/build_driver.py` for the main 61-step HL2 Win32 build. Shader compile tools and installed licensed SDKs must be available for relevant stages.
