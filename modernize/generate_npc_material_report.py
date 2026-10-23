#!/usr/bin/env python3
"""Render human-readable NPC evidence alongside the machine-readable audits."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'modernize/reports'


def code_list(values):
    return ', '.join('`'+value+'`' for value in values) or 'none statically identified'


def main():
    entries=json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
    source=json.loads((OUT/'NPC_ASSET_AUDIT_V18.json').read_text())
    external=json.loads((OUT/'NPC_EXTERNAL_ASSET_AUDIT_V18.json').read_text())
    local={entry['classname']:entry for entry in external['npcs']}
    lines=['# NPC, material and shader implementation evidence — v18', '',
        'This report distinguishes source repairs from restored visual content. The supplied v17 archive contains no NPC MDL, QC, SMD, VMT, VTF or WAV content. Utility TGA samples under `utils/viewbump` provide no recoverable NPC geometry, rig or animation. No installed game assets were copied into this release.', '',
        '## Implemented source changes', '',
        '- `dlls/hl2_dll/npc_assassin.cpp`: level precaching for the existing red-eye glow and blue trail; create eye effects only when the named Eye attachment exists; skip pistol fire when the intended left/right muzzle transform is unavailable.',
        '- `dlls/hl2_dll/npc_bullsquid.cpp`: require the intended Mouth attachment before calculating a spit projectile trajectory.',
        '- `dlls/hl2_dll/npc_mortarsynth.cpp`: skip an unavailable energy-gun attachment rather than using an invalid transform.',
        '- `dlls/hl2_dll/npc_combineguard.cpp`: reject aiming calculations when the muzzle index or transform is unavailable.',
        '- `dlls/hl2_dll/npc_lightstalk.cpp`: guard failed glow-sprite allocation. Existing animation and fade logic remains active.',
        '- `dlls/hl2_dll/npc_launcher.cpp`: avoid precaching empty map-supplied projectile/sound names and skip firing without MissileModel. Its original invisible player-model dummy is explicitly identified as a dummy, not a restored enemy appearance. Hammer now exposes the actual MissileModel/LaunchSound/FlySound/StartOn fields.',
        '- `materialsystem/stdshaders/WaterCheap_ps20.fxc`: bound eye-vector normalization in both NV3X and standard BLEND permutations and clamp the Fresnel dot to prevent negative reflection from quantized normal overshoot. The distance floor is 0.01 world units; squared floor 0.0001 is representable in half precision. Normal viewing distances preserve the prior computation.', '',
        'The idempotent source overlays are `modernize/patch_npc_assets.py` and `modernize/patch_material_assets.py`. They preserve source encodings and line endings and refuse an unexpected patch site.', '',
        '## Inventory and Hammer integration', '',
        f'The source inventory covers {len(entries)} cut/experimental classes; {sum(entry["prototype_candidate"] for entry in entries)} have unconditional source registrations and Debug HL2 project inputs. Conscript remains behind its original whole-file `#if 0`; enabling the obsolete implementation would require an AI/API port, not simply an asset reference change. Registrations are not newly asserted to work in game.', '',
        '`generate_cut_npcs.py` now removes comments, resolves string-valued model macros, distinguishes disabled/conditional registration, records all project configurations, follows verified Roller model inheritance and records direct effect, sound, attachment, sequence and activity dependencies. This repairs the old Combine Guard inventory selecting a commented metal-gib model and the old missing model entries for Ichthyosaur/Light Stalk. Default Roller Bull uses the Roller model, with its alternate spikes retained as a dependency.', '',
        '`hl2_2003_cut_npcs.fgd` provides 20 experimental Hammer definitions with the source model displayed when installed. The invisible Launcher uses a point box. Twenty VMF source test rooms are supplied; these are not compiled maps and provide neither missing content nor runtime verification. Aquatic/complex AI and combat require appropriately authored runtime scenarios.', '',
        '| Classname | Primary source model | Registered / HL2 input | External file / case | Model format |',
        '|---|---|---|---|---|']
    for entry in entries:
        evidence=local[entry['name']]
        model=entry['model_reference'] or 'unresolved'
        version=evidence['model_content']['version'] if evidence['model_content'] else 'absent'
        lines.append(f'| `{entry["name"]}` | `{model}` | {entry["registration_active"]} / {entry["included_in_debug_hl2"]} | {evidence["model_file_status"]} / {evidence["model_case_status"]} | {version} |')
    lines.extend(['', '## Actual asset results and missing content', '',
        f'The source-only audit finds {source["scanned_unpacked_mdl_count"]} unpacked models. Therefore every primary and secondary NPC model remains missing from the distributable source package. The external audit scans {external["scanned_unpacked_mdl_count"]:,} distinct MDL paths in the developer-supplied `hl2_2003` and common `hl2` directories, read-only. All 21 referenced primary paths exist there (including the Launcher dummy), but all 21 MDLs report version 37. The supplied `public/studio.h` specifies version 36. These installed models are reported as unsupported-version metadata, not verified compatible models. Seventeen primary paths differ in case. The files are evidence of external availability only.', '',
        'The bounded `source_asset_formats.py` reader decodes only the supplied 32-bit versions 35/36, using the source version-conversion evidence for 35. It checks declared lengths, signed counts, table spans, strings and attachment bone indices. It reports newer versions without guessing their layout. Version 35/36 vertices are embedded; a missing retail-style VVD is not itself a fault. Metadata success still does not verify mesh topology, UV alignment, weight painting, collision fitting, animation quality or effects.', '',
        'Restoration requires legally supplied compatible model geometry/rig/animations, matching VTX files and referenced materials/textures; collision PHY where the NPC needs it; the listed audio/event definitions; and any projectile/effect dependencies. There is no sufficient NPC source material here for an honest reconstruction of the original rigs or appearances. Nothing in this release substitutes an unrelated enemy model.', '',
        'The exact static requirements follow. Sound events need their original sound-script definitions and referenced WAVs. Numeric attachments, animation-event payloads, dynamic assets and dependencies inherited beyond the noted models require runtime/model-authoring review; the inventory does not pretend those are fully resolved.', ''])
    for entry in entries:
        lines.extend(['### '+entry['name'], '',
            '- Source: `'+str(entry['source'])+'`.',
            '- Model files: '+code_list(entry.get('model_references',[]))+'.',
            '- Effect materials: '+code_list(entry.get('effect_material_references',[]))+'.',
            '- Named attachments: '+code_list(entry.get('named_attachments',[]))+'.',
            '- Explicit sequence labels: '+code_list(entry.get('named_sequences',[]))+'.',
            '- Activities referenced by source: '+code_list(entry.get('activity_references',[]))+'.',
            '- Direct sound files: '+code_list(entry.get('sound_file_references',[]))+'.',
            '- Sound-script events: '+code_list(entry.get('sound_event_references',[]))+'.', ''])
    lines.extend(['## Rendering/material scope and verification', '',
        'The supplied V17 DX9 shader source, normal-map sampling, lightmap/Fresnel safeguards, transparency/refraction paths and experimental postprocess resources were inspected. The specific additional numerical repair above changes the real existing cheap-water shader. No material or texture files for restored enemies are present to repair UV assignment, alpha channels, normal/specular maps, cubemaps or textures. Existing utility brick/bump TGAs were retained; they cannot substitute for enemy textures.', '',
        'The five modern postprocess shader sources remain future integration assets. A later installed-SDK search found fxc/dxc outside PATH, exposed and repaired an HLSL reserved-word syntax error, and compiled four D3D11/D3D12 stage binaries. These project-owned experimental outputs and provenance are under `modernize/shaders/compiled_v18`; exact results are in `reports/SHADER_VALIDATION_V18.md`. Four GLSL/Vulkan jobs remain blocked by unavailable glslangValidator. No DX11, DX12, Vulkan or OpenGL backend is claimed functional, and the legacy DX9 shader permutations remain uncompiled.', '',
        'Fifteen targeted automated tests passed: three bounded MDL metadata/malformed-content tests, four NPC asset-audit tests, two source-inventory tests, three map-source tests, and three cheap-water numerical/source tests. Both source overlays were rerun successfully. Full release validation is recorded separately by the release integrator. No game build, GPU test, Hammer/VBSP compilation or in-game NPC/animation/audio validation has been performed.', '',
        'Reproduce the targeted tests from the project root:', '',
        '```text',
        'python -m unittest modernize.tests.test_source_asset_formats modernize.tests.test_npc_asset_audit modernize.tests.test_cut_npc_inventory modernize.tests.test_cut_npc_maps modernize.tests.test_material_assets -v',
        'python modernize/generate_cut_npcs.py',
        'python modernize/generate_npc_testmaps.py',
        'python modernize/npc_asset_audit.py --assets-root . --out modernize/reports/NPC_ASSET_AUDIT_V18.json',
        'python modernize/build_modern_shaders.py --plan',
        '```', '',
        'Detailed machine-readable records: `cut_npc_inventory.json`, `reports/NPC_ASSET_AUDIT_V18.json`, `reports/NPC_EXTERNAL_ASSET_AUDIT_V18.json`. The external-root report is machine-specific and is intentionally distinct from deliverable content verification.', ''])
    (OUT/'NPC_MATERIAL_IMPLEMENTATION_V18.md').write_text('\n'.join(lines),encoding='utf-8')
    print('Wrote NPC_MATERIAL_IMPLEMENTATION_V18.md')


if __name__=='__main__':main()
