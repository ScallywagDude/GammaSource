# Limited HUD diagnostic-sheet review

On 2026-10-22 the root agent inspected the local diagnostic sheets `hl2_atlases_01.png` and `hl2_weapon_icons_01.png` under the workspace's sibling `asset_visual_audit_v18` directory. AR1/AR2 active and inactive silhouettes broadly align with the labels printed in their source atlas. This is limited HUD visual QA, not a match against rendered weapon models; the available later model layout has not been validated against this source.

The pixel-audit agent also inspected `hl2_weapon_icons_01.png` and `hl1_atlases_02.png` to confirm that decoded atlas/crop imagery appears coherent and that sheets preserve proportions. None of these spot checks certifies all artwork, animations, runtime scale, alpha blending or GPU appearance.

Source-unregistered scripts such as HL2 `weapon_357`, `weapon_alyxgun` and bottle/crossbow scripts remain explicitly script-only in the JSON, report and sheet labels. Their presence in a local asset folder does not establish a corresponding weapon implementation in the active branch.

Three HL1 autoaim regions are empty under their actual additive material rule: `weapon_357` at `[24,72,24,24]`, `weapon_glock` at `[0,72,24,24]`, and `weapon_shotgun` at `[72,72,24,24]`. Each has zero nonblack pixels with nonzero alpha. These are pixel findings; whether intentional or requiring replacement must be assessed against the original HUD behavior. HL2 hopwire's ammo/crosshair/autoaim use nested legacy resolution metadata. The audit mirrors the restored source's conservative 640-then-320 parser and records the selected block; these entries are not empty icons. Font-only HUD entries remain unverified because the supplied source does not implement that rendering path.

No icon artwork or asset rectangle was changed during this diagnostic pass. All contact-sheet PNGs are kept outside the source archive; only the audit tool, synthetic tests and text/JSON results are packaged.
