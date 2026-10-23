# Phase 7 — Weapon restoration

**Latest local source milestone:** v24. **Status:** in progress, not playable or runtime-verified. This is the **single living Phase 7 status file**. Update it in place for future releases rather than creating `PHASE7_V25.md` and similar files.

## Branch-specific weapon verification

| Branch | Registered classes | Additional compiled `impulse 101` grants | Uncompiled candidates excluded | Fully verified |
| --- | ---: | ---: | ---: | ---: |
| HL2 / HL2MP | 32 | 21 | 1 | 0 |
| HL1 | 15 | 15 | 0 | 0 |
| Counter-Strike | 28 | 25 | 0 | 0 |
| TF2 | 38 | 25 | 6 | 0 |
| **Total** | **113** | **86** | **7** | **0** |

These values come from the v22 baseline weapon inventory; later audits have not established that any of the 113 weapons is fully visually and gameplay-verified. “0 verified” means incomplete evidence, **not** that every weapon is broken. Extra grants do not include weapons granted by each branch's original `impulse 101` logic.

## Latest source changes

**v24**
- Layered weapon auditor resolves Source `Patch` materials through `include`, `replace`, and `insert`, recording the search layer for inherited textures.
- Detects malformed/cyclic/unsafe material includes and excessive inheritance depth.
- Locates VTX companions independently across ordered asset layers instead of assuming the model and its sidecars share a directory.
- Validation: **184 tests run, 183 passed, 1 skipped, 0 failed**. Overlay reapplication matched **8,267** source files from v23. Five changed files and three added files.

**v23**
- Introduced per-branch layered asset auditing for models, scripts, HUD materials, VTF dimensions and direct/640/320 legacy sprite layouts.
- Added safe weapon model-path checks, rejecting `/.mdl` as an empty basename.
- Validation: **180 tests run, 179 passed, 1 skipped, 0 failed**. Replay reproduced **8,264** local source files from v22.

## Remaining Phase 7 work

- Verify actual branch-specific weapon scripts and compiled registrations; exclude abstract, internal, or uncompiled classes.
- Validate first-person viewmodels, world models, model companion versions, attachment points, animations, hand positioning, and weapon behavior.
- Verify VMT/VTF materials, correct texture and UV scaling, HUD icon identity, rectangles, and crosshairs against real installed game assets.
- Test `impulse 101`, ammunition, inventory limits, pickups and multiplayer behavior inside working Windows builds for each branch.

Registration and format checks are not proof of correct models or gameplay. A full Windows build and in-game asset/runtime verification have not been completed.

## Reproducing the inventory gate

With your local v22 or later source archive:

```sh
python phase7_manifest_check.py /path/to/HL2_2003_Modernization_Source_v22.zip
```

Exit status 0 denotes complete reported evidence, 1 denotes valid but incomplete evidence, and 2 denotes corrupt/inconsistent inputs. A passing manifest still requires real gameplay verification.

**Repository convention:** Keep one `README.md` and one `PHASE7_STATUS.md`, updated in place. Git commits preserve the historical changes without extra version-numbered report files. Modern rendering APIs remain the final development phase.
