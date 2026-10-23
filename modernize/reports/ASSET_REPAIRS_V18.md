# Applied local asset repairs — v18

The composed local overlay contains **5,281 edited files and 6,382 edits**. Original installed content was read only. Every original and edited file was verified against its SHA-256 hash; there were zero mismatches. `ASSET_REPAIRS_VALIDATION_V18.json` records the result.

| Repair | Count |
|---|---:|
| Exact reference casing | 6,313 |
| Redundant internal separators plus exact casing | 50 |
| Existing legacy HUD leaf names expanded to the correct sprite path | 5 |
| Same-weapon nonblank regular-reticle coordinates used for blank HL1 autoaim | 6 |
| Single missing final VMT brace | 4 |
| Extra terminal VMT brace | 3 |
| Stray terminal token matching the VMT filename | 1 |

There are 3,897 edited files in the HL2 layers and 1,384 in the HL1 layer. The unique file total is smaller than the edit total because some files need multiple references repaired. Six VMTs require both reference and syntax work; the composed plan preserves both changes. Seven ambiguous malformed VMTs remain untouched, with exact paths in `MATERIAL_SYNTAX_REPAIRS_V18.md`.

The actual edited local files are in `asset_restoration_v18` beside the v18 source folder. `hl2/root0` corresponds to the supplied `hl2_2003` directory, `hl2/root1` to common `hl2`, and `hl1/root0` to `hl1`. These layers must stay separate. They are not automatically installed into the game and are not bundled as proprietary payloads in the source archives.

Only the minimal token plans, unified diffs, tools and validation evidence are shipped in the source/patch archives. No new model, rig, UV layout, animation, original weapon artwork or missing sound was fabricated. These configuration repairs do not establish model-layout compatibility or visual/runtime correctness.

The canonical combined plan is `ASSET_RESTORATION_PLAN_V18.json`, with its matching `.patch`. The separate case/separator, syntax, `HUD_REFERENCE_REPAIR_PLAN_V18.json` and `HUD_AUTOAIM_FALLBACK_PLAN_V18.json` plans remain as provenance; use the combined plan when applying repairs so a full-file overlay from one repair cannot overwrite another repair. Hopwire's nested 640/320 metadata is supported by the restored source loader; its existing artwork and rectangle coordinates are retained. The three blank HL1 autoaim regions now use the same weapon's existing nonblank crosshair, with identical material and dimensions. This is an explicit fallback, not original autoaim-art reconstruction. The post-repair pixel audit verifies all 276 raster crops are nonblank; the 21 font entries remain unverified and source-unsupported.

Reproduce the combined local edited files into a new directory:

```text
python modernize/asset_case_patch_v18.py --apply --plan modernize/reports/ASSET_RESTORATION_PLAN_V18.json --assets-hl2 ../hl2_2003 --assets-hl2 ../hl2 --assets-hl1 ../hl1 --output-dir ../new_asset_restoration_overlay
```

Verify the edited output and confirm original files remain unchanged:

```text
python modernize/asset_case_patch_v18.py --plan modernize/reports/ASSET_RESTORATION_PLAN_V18.json --assets-hl2 ../hl2_2003 --assets-hl2 ../hl2 --assets-hl1 ../hl1 --verify-output ../new_asset_restoration_overlay
```

If regenerating plans from the same authorized local inputs, compose them before applying:

```text
python modernize/compose_asset_repairs_v18.py --input-plan modernize/reports/ASSET_CASE_REPAIR_PLAN_V18.json --input-plan modernize/reports/MATERIAL_SYNTAX_REPAIRS_V18.json --input-plan modernize/reports/HUD_REFERENCE_REPAIR_PLAN_V18.json --input-plan modernize/reports/HUD_AUTOAIM_FALLBACK_PLAN_V18.json --assets-hl2 ../hl2_2003 --assets-hl2 ../hl2 --assets-hl1 ../hl1 --output-dir ../new_composed_overlay
```

An existing output directory, changed original content, unsafe paths or overlapping conflicting edits are refused. A differently updated installation needs fresh plans rather than force-applying stale offsets.
