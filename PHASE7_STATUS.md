# Phase 7 — Weapon restoration status (v22 baseline)

**Status: NOT COMPLETE.** This report audits only original metadata from the user's local v22 archive. It does not publish or reproduce proprietary engine/game source or game assets.

| Branch | Registered weapon classes | Compiled extra `impulse 101` grants | Excluded uncompiled candidates | Fully verified (all four evidence flags) |
| --- | ---: | ---: | ---: | ---: |
| HL2/HL2MP | 32 | 21 | 1 | 0 |
| HL1 | 15 | 15 | 0 | 0 |
| Counter-Strike | 28 | 25 | 0 | 0 |
| TF2 | 38 | 25 | 6 | 0 |
| **Total** | **113** | **86** | **7** | **0** |

The branch-grant manifest passed structural checks. “Extra” grants are additions to each game's original `impulse 101`, not necessarily every weapon in the final inventory. Registered classes include incomplete, internal, or experimental entities; neither registration nor successful path verification means the weapon works.

**Blocking verification:**
- Original or authorized replacement viewmodels, world models, textures and material assignments
- Correct per-weapon HUD imagery, icon rectangles, scaling, animations, ammunition and attachments
- In-game console `impulse 101` testing for each branch; exclusion of abstract/internal and noncompiled entities
- Confirmed Windows compilation/linking and actual runtime gameplay

All four verification indicators are false for all 113 registrations in the supplied manifest. This means “not yet verified,” **not** “113 broken weapons.”

### Reproducing

With your private local v22 source ZIP, run `python phase7_manifest_check.py /path/to/HL2_2003_Modernization_Source_v22.zip`. It exits 1 for valid but incomplete records, 2 for invalid/inconsistent reports, and 0 only when all listed evidence indicators are true. This evidence gate does not replace actual gameplay checks.

**Next milestone:** complete an authorized asset-backed branch-by-branch weapon report and execute a Windows gameplay verification pass before marking Phase 7 complete.
