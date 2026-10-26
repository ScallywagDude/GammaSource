# Original procedural body corrections

The v5 converter discarded procedural bones. This removed sixteen authored Alyx
shoulder, bicep, elbow, ulna, wrist, pectoral, latt, and trapezius corrections and
twelve Kleiner corrections. These helpers directly influence 578 of Alyx's 5,018
vertices and 424 of Kleiner's 3,944 vertices, so their omission changes visible
body geometry even when the animation samples themselves match.

`datacache/legacymdl.cpp` now translates both original procedural types:

- Type 1 axis interpolation: identical 176-byte payload, six positions and six
  quaternions, with the moved bone's procedure pointer rebased.
- Type 2 quaternion interpolation: identical 12-byte control/count header and
  48-byte trigger records; both moved owner-relative pointers are rebased.

`BONE_ALWAYS_PROCEDURAL` (0x4) retains its original meaning and is preserved. A
helper's controlling bone and its ancestors inherit the use masks requiring the
helper, including dependencies without direct vertex use. The normal v48 solver
then evaluates the original data. Bounds, indices, finite fields, axis selection,
and the runtime's 32-trigger stack limit are validated before serialization.

`datacache/mdlcache.cpp` uses `legacy_models_cache_v6` so old converted files do
not shadow the restored data. Original files and previous caches are preserved.

## Evidence collected before the v6 native run

Full source inventory: 6,282 original v36/v37 variants, 866 procedural records,
all type 2, at most twelve triggers, no invalid fields/dependencies found. Type 1
has no example in this corpus; its layout translation follows both checked-in
original and modern struct definitions.

The source helper audit evaluated 1,367 authored absolute Alyx control poses and
21,872 helper transforms. Omitting the corrections changes helper world positions
by up to 11.2083 units and rotations by up to 171.4713 degrees. Kleiner's 624 poses
and 7,488 helper transforms show up to 90.0729 degrees of missing correction.
These measurements isolate procedural transforms; they are not a claim that all
final combined scenes have passed visually.

The audit also verifies the separate release-build normalization fix in
`public/bone_setup.cpp`: 9,160 Alyx and 4,939 Kleiner blended helper quaternions are
nonunit before normalization. Omitting normalization can change basis lengths by
6.38%. The coordinating pose owner moved the normalization out of an `Assert`
whose expression was omitted in release builds.

Other inspected geometry fields do not show corruption: both models' bone
parents, bind transforms, geometry, retained bone IDs, and rendered palette
associations match. All checked rendered weight sums are within 0.0000001 of one.
The actual 3-influence renderer omits the smallest fourth influence; that is
separate from the discarded helpers. Flex data has no fixed-point overflow and
maximum post-loader component error below 0.0009 units.

External proof directory:
`C:/Users/doge6/Documents/GammaSourceTest/impact_restoration_2026-10-26/all_model_validation/`

- `procedural_source_inventory.json`
- `procedural_weighted_geometry.json`
- `alyx_procedural_source_transforms.json`
- `kleiner_procedural_source_transforms.json`
- `alyx_kleiner_skinning_flex_audit.json`
- `alyx_kleiner_vtx_skinning_audit.json`

Reproduce transformed helper/payload checks with:
`tools/gamma/audit_legacy_procedures.py <original.mdl> --converted <actual-v6-cache.mdl> --out <report.json>`

Actual v6 cache byte comparison passes for Alyx's sixteen and Kleiner's twelve
procedures, including payloads, control indices, parents, and procedural flags.
The 21,872 and 7,488 transformed-helper comparisons have zero positional error and
less than 0.000004 degrees of quaternion roundoff. Reports are
`alyx_v6_procedural_native_cache.json` and `kleiner_v6_procedural_native_cache.json`.
The numerical audit intentionally uses identical authored source control poses
for both payloads, separating procedural conversion from the existing raw-channel
and subframe audits. Final combined live-matrix and visual scene checks belong to
the coordinating agent.

Source checkpoint after these changes:

- `datacache/legacymdl.cpp`: `C69C726370670E60A961D84698F6BDBE963ECCFA4F346F35CB07E628DF7738CE`
- `datacache/mdlcache.cpp`: `46ECD3CD25FF39AC2067C3EB8A2BE62F04239B58612FD2F3B23DBC0257B0CDEB`

Both files were saved at approximately `2026-10-26T13:52:55Z`.
