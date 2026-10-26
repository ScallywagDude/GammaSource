# GammaSource Core Runtime Stability Pass

This patch targets the existing Win32 runtime without changing the game's HUD, weapons, menus, content, displacement simulation, or on-disk network format.

## Changes

- `public/bitbuf.cpp` and `public/bitbuf.h`: validate bit-buffer lengths and offsets, guard negative/oversized reads, prevent signed-overflow checks, initialize diagnostic names, and use a bounded byte-wise read when an optimized dword read would cross a short packet's allocation.
- `bf_read::ExciseBits()`: remove an erroneous bytes-versus-bits allocation size, copy remaining packet bits within the existing buffer bounds, and reject invalid removal intervals without mutating the message.
- `engine/net_chan.cpp`: validate fragment offsets and lengths before sequence/ack state changes; limit fragment copies to the 2 KB stack destination; abort on failed buffer copies.
- `modernize/tests/test_bitbuf_stability.py`: compile and execute actual method bodies with C++98 stubs and undefined-behavior instrumentation.
- `modernize/tests/test_network_packet_limits.py`: verify source guard ordering and fragment-length boundary cases.

## Preservation

The content overlay remains mounted by default. The shipped `hud1.vtf`, HUD sprite manifest, first-person beta viewmodels, original weapon source, and terrain-deformation code are unchanged. Existing 2003-era file formats and interfaces remain in place.

## Verification scope

- 13 focused tests pass (bit-buffer, fragment bounds, content integration).
- The project structure validator passes, but reports remaining source references in the broader auxiliary solution.
- The full Windows MSVC build and in-game stability are **not** verified in this environment.
- Three legacy regression checks still fail: an old Round 8 repair script refuses an already-customized `Directory.Build.targets`, and two old weapon-HUD checks expect the superseded earlier HUD implementation. These checks were not suppressed.

The changes are an incremental hardening patch, not a complete replacement or proof of runtime parity with a later engine.
