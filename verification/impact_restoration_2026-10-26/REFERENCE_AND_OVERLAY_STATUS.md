# Original 2003 blood and bullet impact restoration

Active source: `C:\Users\doge6\Documents\HL2BetaMegaCuck\GammaSource`. No old split engine tree was edited. No build or game was launched by this asset task. Runtime visual QA is pending and remains coordinated after the animation work.

## References and findings

The supplied MegaBuild stores common original effects in `hl2/materials` and `hl2/scripts/decals.txt`; `hl2_2003` does not contain separate blood/decal effect folders. Original code is in `modernization_v18` and is inventoried in `reference_code_manifest.json`.

- `cl_dll/c_te_bloodsprite.cpp` calls the legacy animated blood sprite and adds `FX_Blood`.
- `cl_dll/c_impact_effects.cpp::FX_Blood` emits two `effects/blood` and two `effects/blood2` puffs, growing from size 2 to 8 over roughly 0.25–0.5 seconds.
- `cl_dll/c_te_legacytempents.cpp::BloodSprite` animates the spray in 0.25 seconds. Its drop-model argument is unused in this original implementation.
- `dlls/util.cpp::UTIL_BloodDrips` selects single-player red RGB 64/0/0 or yellow 128/128/0, alpha 255, and sprite size clamped from amount/10 to 3–16. Mechanical hits retain sparks/smoke.
- `cl_dll/fx_blood.cpp` separately implements the original drop/gore/cloud spray with `effects/blood_drop`, `blood_gore`, and `blood_puff`.
- Original impact groups use individual `shot1`–`shot5` textures and original flesh wounds. The 2018 `game/shared/decals.cpp` selected `scripts/decals_subrect.txt`, which maps these groups to retail atlas materials. Asset replacement at the old names alone would therefore not restore bullet marks. The 2018 `BloodImpact` callback also routes recognized colors through modern particle systems, requiring the coordinated client-code change.

## Installed private overlay

All files are new private paths under `GammaSource/gammasource_content_2018`. Shared/reference files and hard links were not modified.

- Materials: `materials/gamma_2003/<original material path>.vmt/.vtf`.
- Blood material names: `gamma_2003/effects/{blood,blood2,blood_gore,blood_drop,blood_puff}`.
- Sprite material names: `gamma_2003/sprites/{blood,bloodspray}`.
- Decal list: `scripts/gamma_2003_decals.txt`.

The private decal list explicitly merges the frozen current 2018 baseline with original `Impact.Concrete`, `Impact.Metal`, `Impact.Wood`, `Impact.Glass`, `Impact.Flesh`, `Impact.BloodyFlesh`, `Impact.AlienFlesh`, six `Blood` and six `YellowBlood` variants. Original `BigShot`, `BulletProof`, `GlassBreak`, and `ExplosiveGunshot` groups also restore bullet-related markers. Other 2018 groups and extra material translations remain present.

There are 141 assets: 80 VMTs and 61 VTFs, totaling 510,276 bytes. Every texture is byte-identical to both the MegaBuild reference and `C:\Users\doge6\Videos\GammaSource_Build\gammasource_content`. VMT shaders, scales, flags and numeric parameters are preserved; only texture/material references receive the private prefix. Dependency traversal includes all 20 `$modelmaterial` wrappers. This original graph contains no bump/normal-map dependencies. The gore material intentionally references the original `effects/blood` texture; drop and puff reference original `effects/splash2`, exactly as supplied.

The private namespace prevents mounted TF2/HL2 VPKs, converted assets or retail atlas materials from replacing these original textures. The client source owner is coordinating callback material paths and loading the private script, with a fallback if it is absent.

## Reproduce and verify

From the active GammaSource directory, run Python 3 on:
`tools/gamma/restore_2003_impact_assets.py`

Defaults use the adjacent MegaBuild `hl2` reference and output to `gammasource_content_2018`. Optional arguments: `--reference`, `--out`, `--baseline`, `--report`. The frozen baseline is `tools/gamma/reference/decals_subrect_2018.txt`.

`provenance_manifest.json` records every original/output SHA-256 and dependency. `asset_validation.json` confirms complete closure, exact texture bytes, preserved VMT tokens, unchanged extra groups and a byte-identical independent regeneration. A separate generated copy is in this report folder's `private_overlay`. `secondary_2003_reference_check.json` records the independent 61/61 match.

Still required in an isolated game session: verify blood hits, animated sprite/puffs, red and yellow wall stains, original wall bullet marks on concrete/metal/wood/glass, and flesh/model wound markers. Check both material resolution and visible behavior; successful asset generation is not runtime verification. Preserve the completed weapon values, mandatory beta hands and latest UI/loading behavior while integrating.
