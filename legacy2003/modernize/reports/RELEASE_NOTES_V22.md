# HL2 2003 modernization v22 — v7 vertex compatibility tooling

## Implemented

- Added a **conservative offline VTX v7 -> v6 converter** for models whose MDL studio tables are compatible with the 2003 format. This repairs the packed optimized vertex records and rewrites the pointer-relative hierarchy (LOD, mesh, strip groups, indices, strips, bone state changes, and material replacement strings) into the older layout.
- Performs bounded reads/writes (64 MiB cap), material-name validation, index bounds, checksum pairing with the optional original MDL, and checks of old bone palette limits.
- Refuses unsupported v7 delta-flex flags, oversized bone palettes, invalid indices, and corrupt offsets rather than emitting misleading models. Never modifies the source asset; output file must not already exist.
- Added VTX v7 candidate reporting to the branch-aware model scanner. It **does not** claim a converted sidecar is a fully working model.
- Retains all v21 work; normal v36 loading and previously guarded v37 loading code are unchanged.

## Usage

For locally owned/authorized files, from the extracted source directory:

```text
python modernize/scan_studio_v21.py --root hl2=PATH_TO_HL2_ASSETS --output studio_v22_audit.json
python modernize/convert_vtx7_to_v6_v22.py PATH_TO_MODEL.dx80.vtx --mdl PATH_TO_MODEL.mdl --output PATH_TO_NEW_MODEL.dx80.vtx
```

Place the converted file in a **separate test installation** under the companion filename expected by the loader. Never overwrite or cross-mix game branch assets. Conversion only succeeds for a restricted subset of v7 VTX files. MDL layout, vertex positions, animation streams, textures, and render success are independently unverified.

The converter intentionally does not auto-install converted files or allow v37 MDLs with changed studio layouts. The project still lacks the successful Windows compilation and runtime/GPU validation necessary to call the engine playable.

## Validation performed

- Native/source regression: **175 tests, 1 skipped, 0 failures**, including eleven new v22 tests for conversion, corruption rejection, and branch-scoped scanning.
- Structural Visual Studio checks: **153 `.vcxproj` files, 348 configurations** passed; this is not compilation.
- Overlay packaging: exact v21 SHA256 is required, changed-member checksums are enforced, and the generated v22 source is reproducible from v21 plus the v22 overlay.

## Remaining eight-phase status

1. Visual Studio conversion: structural only; Windows compilation unverified.
2. Engine modernization: partial.
3. Vulkan/OpenGL/DX11/DX12: actual backends not implemented.
4. Shaders/lighting: earlier patches retained, runtime unverified.
5. Steam/controller: bootstrap/XInput support partial.
6. Cut enemies: partial source fixes; model and AI runtime work outstanding.
7. Weapons/assets: additional VTX compatibility tool, art and animations not certified.
8. Multiplayer: 3D preview path and gameplay enhancements partial, runtime unverified.
