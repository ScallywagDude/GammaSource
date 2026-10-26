# Original IK data compatibility

`datacache/legacymdl.cpp` now preserves the original per-animation IK rules and
uncompressed position/quaternion error tables, per-sequence locks, and autoplay
locks. Old 108-byte rules and 12-byte locks are translated into the modern
152-byte and 32-byte layouts. Pointers are rebased; modern-only fields are zero.
Old unused chain contact/limit vectors are not interpreted as modern knee hints.
The converter marks original animations/sequences for the separately implemented
2003 composition and wrapped-error sampling behavior.

The original compiler allocates `endFrame - iStart + 1` errors, adding two guard
samples if the end extends beyond the frame count. This allocation and the old
decoder's use of `numframes` for wrapped indexing were checked against the actual
2003 `utils/studiomdl/simplify.cpp`, `write.cpp`, and `game_shared/bone_setup.cpp`.

Two original models exposed a static-pose compiler bug: ElitePolice and monk have
42 single-frame IK_SELF rules whose normalized envelopes contain NaN/Infinity
because the compiler divided by `numframes - 1`. Those comparisons gave zero
influence in the old player. The converter preserves rule indices, chain fields,
and the first authored error, while representing the inactive envelope with
finite zeros. Only nonfinite one-frame envelopes take this repair path. Finite
one-frame rules retain their data. Rejecting the whole model would be a new
regression.

The first native run caught another converter validation error: an expanded
normalized sequence table follows the original payload, so its relative lock
references are negative. The range check now validates the resolved address
instead of rejecting that sign. All 641 nonempty sequence-lock references in the
supported corpus are negative. Original unmoved animation-rule, error-table and
chain-link references are nonnegative and are not affected by this rebase.

## Completed numerical checks

- 6,260 supported original v36/v37 variants: 866 rules, 84 repaired static records,
  102 wrapped records, 1,276 sequence locks, and 624 autoplay locks. Both original
  formats count separately; these are not 866 distinct animations in one build.
- Bounds, finite values, chain and bone references, weights, envelope order, and
  original error allocation pass for these records.
- All 866 rules across 143 IK-bearing variants were checked at cycles 0 through 1
  in 0.001 steps and at envelope boundaries plus/minus 0.000001. Active old indexing
  and inactive modern ground-rule clamping stay inside the copied error samples.
- 22 source v37 files are outside the existing normalizer's layout support:
  external/incomplete animation groups, invalid local blend references, or
  truncated original blobs. They are listed explicitly in the source report and
  are not counted as an IK pass or repaired by this change.

External proof directory:
`C:/Users/doge6/Documents/GammaSourceTest/impact_restoration_2026-10-26/all_model_validation/`

- `ik_v5_source_validation.json`: source metadata and layout exclusions.
- `ik_v5_sampling_bounds.json`: original decoder sampling contract.

Actual native-cache byte comparison and final combined runtime scene validation
remain pending with the coordinating agent; the checks above are not a claim
that every animation has been visually verified.

## Reproduce source and cache audits

`tools/gamma/audit_legacy_ik.py --inventory <model_inventory.json> --out <source-report.json>`

After a native v5 warmup, add:
`--cache <t_round3/legacy_models_cache_v5/models> --cache-only --warmup-manifest <warmup_manifest_v5.json>`

The cache audit checks actual bytes for rule fields, error samples, sequence and
autoplay locks, legacy marker bits, and cleared modern-only pointers/flags/vectors.
It returns failure when a cache was requested but no checksum-matched pair exists.

v5 converter checkpoint SHA-256 before the separate procedural-helper restoration:
`2343B019937B594D8CF45EEAA4B66A81F0C15957594DA213EF52C52C9105E9F8`

Converter timestamp (UTC): `2026-10-26T13:41:03.1989620Z`.
