# GammaSource — NPC restoration and impulse 101 worklist

## Completed in this source update

- `impulse 101` still grants `weapon_annabelle` when its weapon script is available.
- Weapon selection labels it **Annabelle**, using a literal `printname` so the label does not depend on an unavailable translation token.
- `impulse 101` excludes `weapon_citizenpackage` (rations), `weapon_citizensuitcase` (suitcase), and `weapon_cubemap` (developer tool), but these entities are **not unregistered**. Maps and NPCs can still instantiate them.
- The exclusion is applied in the actual `game/server/player.cpp` grant iteration, after normal weapon grants and ahead of checks for owning the weapon.
- Four focused tests pass, including actual C++98 compilation and execution of the grant filter.

## NPC source audit: not yet complete

A static audit of historical `legacy2003/dlls/hl2_dll` NPC `LINK_ENTITY_TO_CLASS` names found **75 unique historical HL2 NPC registrations**. Of these, **68** have source registrations within files directly listed in the current `server_hl2.vpc` or `server_base.vpc` build lists. Seven historical names have source files available but are not directly listed:

| Historical classname | Available source | Required next step |
| --- | --- | --- |
| `npc_assassin` | `game/server/hl2/npc_assassin.cpp` | Port/compile and test AI/animations/assets |
| `npc_bullsquid` | `game/server/hl2/npc_bullsquid.cpp` | Port/compile and test attack AI and animations |
| `npc_houndeye` | `game/server/hl2/npc_houndeye.cpp` | Port/compile and test sonic attack and navigation |
| `npc_hydra` | `game/server/hl2/npc_hydra.cpp` | Port/compile and test scripted animation and physics |
| `npc_miniturret` | `game/server/hl2/npc_turret.cpp` | Port/compile and test turret deployment |
| `npc_sentry` | `game/server/hl2/npc_turret.cpp` | Port/compile and test old sentry behavior |
| `npc_spotlight` | `game/server/hl2/npc_spotlight.cpp` | Port/compile and test its lighting and behaviors |

**This is NOT the total complete NPC restoration list.** It compares registered names in historical HL2 source against current build-file references. Reimagined entity names, TF2 prototype NPCs, different historical branches, 2003 maps, model/sound dependencies, scripted AI, animations, and runtime behavior require separate audits and tests. A class merely being listed in a VPC does not prove that it works.

Machine-readable detail: `verification/beta_npc_class_audit.json`.

## Black rectangle at top-left of screenshot

The screenshot shows a slightly darker **rectangular overlay at the upper-left** of the sky. Its origin is not established by one image. It may be a 2D HUD/debug surface or a renderer artifact. Do not patch the sky renderer based on a screenshot alone. Reproduce it in the same map, take a screenshot with `cl_drawhud 0`, and compare, then use debug material/sky cvars only if the mark persists. A GPU and map-specific run is required to identify and safely fix its source.

## Asset source

Use the user-supplied Google Drive file ID and URL for the Mega Build update source, not a substituted file:

`https://drive.google.com/file/d/1ODwvLQVL9AKRl_VxD45R9wN1trHFpWxc`

The exact link is recorded here, but the full Mega Build archive is **not included in this source patch**, nor is it verified installed. Distinguish update patches from base packages and inspect their file manifests before overlaying any NPC assets.

The user-supplied HL2 Beta download catalogue is:

`https://hl2-beta.ru/index.php?action=downloads;sa=view;down=348`

## Build, testing, and preservation

- Source changes require a native Windows x64 build, staging, and runtime smoke test before marking the gameplay result verified.
- Run `py -3 -m unittest verification.test_impulse101_weapon_filter -v` at the unified `GammaSource` repository root.
- Run `py -3 verification/audit_beta_npc_class_registration.py` to regenerate the source-level NPC audit after further NPC ports.
- Preserve all beta weapons, mandatory hands, HUD, original 2003 loading dialog on plain black, maps, assets, and terrain deformation.
