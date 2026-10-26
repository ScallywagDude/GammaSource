# GammaSource Core Stability — Round 6 (2026-10-24)

## Baseline

`GammaSource_Beta_Core_Stability_Round5_Full_Source.zip`. This update touches only shared memory-pool allocation, model-name handling, BSP game-lump dictionary validation, and one new regression test module. It preserves the existing 2002 HUD, all beta weapons, UI, materials, model assets, terrain deformation, and earlier runtime repairs.

## Changes

- `public/mempool.h`: pointer-align free-list storage in the blob header on both 32-bit and 64-bit layouts. No public pool API changes.
- `public/mempool.cpp`: round per-block stride to pointer alignment; make `GROW_SLOW` actually allocate the specified constant-size batches rather than inheriting the increasing `GROW_FAST` multiplier; reject invalid, overflowing and failed blob allocations without dereferencing a null allocation; calculate allocation size using the actual `m_Data` offset; prevent the allocation count from becoming negative on an empty free operation.
- `engine/modelloader.cpp`: reject model paths that cannot fit `model_t::name` before inserting in the name dictionary; handle a failed pool allocation without populating an invalid cache entry; validate game-lump dictionary header, count and payload bounds before enumerating entries.
- `modernize/tests/test_memory_pool_and_model_cache_stability.py`: compile and run extracted **actual C++98 implementations** with AddressSanitizer/UndefinedBehaviorSanitizer and minimal platform mocks. Tests cover all growth modes, alignment, exhaustion and invalid sizes, model names/cache failures, and valid, truncated and malformed BSP game-lump dictionaries.

## Behavior and compatibility

The pool's `GROW_SLOW` mode now matches its documented semantics. The free-list block stride is rounded up to the pointer size, so it can allocate slightly more memory than before for nonaligned requested sizes. This does not change the size or representation of objects stored by the pool. `GROW_FAST` continues to increase the number of blocks with each new blob. Invalid model paths and corrupt game lumps are rejected rather than copied beyond fixed buffers.

## Scope of verification

Executable C++98 test harnesses compile the relevant functions from source and run under sanitizers. The Win32 project-structure validator checks project metadata, not compilability. No full Visual Studio/MSVC build, playtest, performance-profile result, or long-session crash test was run. The 2003 beta systems were byte-comparison checked against the Round 5 release during packaging; this does not prove all in-game functionality works. Comparable stability and frame-time performance to a later Source engine remain unverified.
