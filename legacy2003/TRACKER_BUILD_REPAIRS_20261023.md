# GammaSource Tracker build repairs — 2026-10-23

This repair targets the follow-up error list containing missing `TokenLine.cpp`, `Info.cpp`, `AdminServer/IAdminServer.h`, legacy `VGUI_*.h`, `tokenline.h`, `info.h` and `assert` diagnostics. **Windows compilation and in-game integration remain unverified.** AMD64 configuration and Phase 7 were not modified.

## Source and project fixes

- Added `shared/TokenLine.{h,cpp}`, a quoted-field tokenizer exposing the methods used by the archived AdminServer (`SetLine`, `CountToken`, `GetToken`, `GetRestOfLine`). Fields and rest-of-line data are writable and have instance-owned lifetimes. Input parsing stops at a newline and bounds token memory to the supplied line. Includes portable C++98 regression coverage.
- Added `common/Info.{h,cpp}` to supply `Info_ValueForKey` required by Tracker's network info readers. It handles malformed inputs and truncates 512-byte return buffers safely. This is a source-level reconstruction of the function required by the archived projects; unrelated engine-only Info functions have not been invented.
- Added `common/AdminServer/IAdminServer.h` using the API contract visible in the existing `common/ServerBrowser/IServerBrowser.h` and `CAdminServer` implementation. The `AdminServer001` interface identifier is **reconstructed, not externally ABI-verified**. Confirm with a real original module consumer before using binary plugins.
- Added 47 forwarding headers under `modernize/tracker_vgui_compat`. Each includes a **real** header from `public/vgui`, `public/vgui_controls`, or `public/`. No placeholder GUI types or fake binary interfaces were inserted. Updated Tracker's 12 `.vcxproj` include paths to find them.
- Repointed generated Tracker project compilation references for `common/Random.cpp` to `common/GameUI/Random.cpp`, `common/MemPool.cpp` to `public/mempool.cpp`, and `public/UtlBuffer.cpp` to the existing `public/utlbuffer.cpp`. All Tracker `ClCompile` entries now resolve under Windows-style case-insensitive lookup. Historical `.dsp` files remain unchanged.
- Added explicit `#include <assert.h>` to 10 Tracker source files using `assert` rather than relying on transitive headers.
- Added `modernize/repair_tracker_source_paths.py` to the idempotent `modernize/apply_modernization.py` regeneration sequence; it reapplies generated-project include-path and source redirects after `convert_dsp.py`.

## Tests performed (not a Windows build)

- `python modernize/repair_tracker_source_paths.py` run twice: second run reported **0** new writes.
- `g++ -std=c++98 -Wall -Wextra -Werror -pedantic` compilation of the two new CPP files succeeds.
- `python -m unittest modernize.tests.test_tracker_missing_sources -v`: **4/4 passed**, including a compiled C++ behavioral executable.
- `python -m unittest modernize.tests.test_build_error_source_repairs -v`: **5/5 passed** for the earlier source patches.
- `python modernize/validate.py`: **153 projects / 348 configurations passed structural validation**. This is not evidence of successful MSVC compilation.

## Still outstanding

1. This error list omitted MSBuild project paths, filenames, line numbers and codes, so errors such as "missing type specifier" and `syntax error: missing ';' before '<'` cannot yet be uniquely traced. VGUI include forwarding can expose further API mismatches between Tracker and Source VGUI2.
2. `ARRAYSIZE` macro redefinition and deprecated CRT/`/Zc:forScope-` notices are warnings. Do not silence warnings en masse and call the build repaired.
3. Both old and new build logs include dependency/library/toolchain blockers. No Visual Studio/MSVC or Windows SDK runtime test was available for this patch. Tracker UI is **not** declared buildable or playable.
4. For the next failure, export a **binlog** (`msbuild project.vcxproj /p:Configuration=Release /p:Platform=Win32 /bl:tracker.binlog`) or compiler output with project, source path, line, and Cxxxx/LNKxxxx code.

This is a focused set of verifiable source repairs, not a representation that all user-reported errors have been eliminated.
