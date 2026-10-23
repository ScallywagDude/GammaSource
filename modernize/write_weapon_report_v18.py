#!/usr/bin/env python3
"""Regenerate the source implementation report from the audited v18 inventories."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]

def main():
    inventory=json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text())
    build=json.loads((ROOT/'modernize/weapon_build_report.json').read_text())
    grants={r['branch']:r for r in json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())}
    out=['# Weapon implementation and branch inventory — v18',
         '',
         'Status: source repairs and automated checks completed within the supplied source. No game DLL was successfully built or launched. No weapon is certified playable, visually correct, or animation-complete by this report. The source archive contains no original weapon model, material, texture, animation, or weapon-script payload to restore. Read-only installation inspection is recorded separately by the asset audit; those proprietary assets are not included in this release.',
         '',
         '## Implemented behavior',
         '',
         '- `GiveCheatWeapon` validates a branch-supplied classname, duplicate ownership, the 48-entry inventory capacity, a loadable weapon script, both configured model paths, the installed MDL header/signature/declared length, and registered ammunition before spawning. It accepts current studio version 36 and the source\'s explicit 35→36 compatibility path. Other version metadata is rejected conservatively; the old engine converter overwriting a newer version field is not proof that the newer layout works. These checks do not certify the full model, companion mesh, skeleton, materials, attachments, or animation data.',
         '- The helper preserves/restores `gEvilImpulse101`, checks actual ownership after Touch, and removes a newly created rejected orphan. Already owned weapons are not duplicated. Normal `give`, map pickups, team rules, and their asset loading remain unchanged.',
         '- All four branch overrides now honor `g_iWeaponCheat` and demo-build restrictions. HL2 retains its original suit/health/base arsenal behavior. HL1 keeps its 14 original weapons plus the deliberately shared, project-compiled physics tool. The shared cubemap debugging tool is inventoried and excluded from gameplay grants.',
         '- Refill derives primary and secondary reserve limits from each actually owned weapon\'s branch ammo registry and fills valid clips. This covers prototype ammo and CS HE/flashbang ammunition without adding foreign ammo names. Infinite/nonpositive ammo limits are not converted into invalid counts.',
         '- CS expanded cheats temporarily allow multiple rifles/pistols while granting. Normal pickup/drop slot limits remain in effect. When script positions collide, all weapons in that slot receive a deterministic client/server order from original position plus classname, bounded by the existing 20-position HUD capacity. The present CS arsenal fits that limit. The cheat supplies the branch\'s existing armor-and-helmet state and preserves its money grant; absent night-vision/defuser implementations are not invented.',
         '- HL2 HUD fitting from v17 is preserved. CS and TF2 selection icons now retain aspect ratio and original resolution within their existing layouts, using actual active/inactive artwork. Shared HUD texture loading/reloading rejects out-of-atlas rectangles against actual loaded dimensions, avoids zero-size division and invalid crop drawing, and permits the existing HL2 missing-icon label to appear. No substitute art is mislabeled as a finished original icon.',
         '- The shared HUD loader now supports existing legacy nested raster layouts: a nonempty direct file wins with its original path unchanged; otherwise the first nonempty 640 file, then 320 file is selected. The top-level role name is preserved while rectangles come from the selected layout. Only a leaf filename selected from a nested legacy layout receives the sprites/ prefix. Direct leaf materials and already qualified paths retain existing behavior. Font-only or selected font metadata remains unsupported by this raster HUD source and runtime-unverified; no new glyph renderer is implied. Synthetic C++98 tests exercise precedence, empty values, font metadata, path preservation, prefix bounds, and the actual production helper.',
         '- TF2 right-hand container weapons now receive server ItemPostFrame updates without an inappropriate client-prediction condition; handle comparisons use explicit `.Get()`. The container is excluded from standalone cheat grants because normal class loadouts must initialize compatible children.',
         '',
         '## Recoverable and unfinished prototypes',
         '',
         '| Weapon | Source work and remaining limits |',
         '|---|---|',
         '| HL2 incendiary rifle `weapon_irifle` | Removed the leading `f` compile typo; precaches its existing flare entity; validates ammo and consumes it only after successful flare creation; updates attack timers; switching to it no longer manufactures 90 rounds. Existing original flare, weapon script, meshes and animations are still required; runtime unverified. |',
         '| HL2 tripwire `weapon_tripwire` | Removed unconditional self-removal from Spawn so the existing deployment code can run; validates the returned mine pointer; stops post-removal animation calls; fixes the member-function callback syntax. Existing scripts, compatible weapon/projectile models, rope/missile effects, animations, and damage settings remain required. Its historical global precache registration stays disabled because those resources are not supplied. |',
         '| HL2 thumper `weapon_thumper` | Portable entity precaches before SetModel; ammo is charged only for successful grounded placement; pickup restores ammo or confirms inventory ownership before removing the placed device; null activators are safe; callback pointers are explicit. Original device model, sounds, and weapon animations remain required. |',
         '| HL2 immolator `weapon_immolator` | Initializes the target sentinel, guards missing/dead owners, applies the existing burn tick on direct entity impacts, attributes burn damage to the owner, and stops burning on holster; explicit callback pointer. This retains the existing experimental damage/radius tuning and does not certify effects, game balance, or animations. |',
         '| HL2 manhack weapon `weapon_manhack` | Both attack bodies are empty; ItemPostFrame only idles. It is registered/compiled but excluded as incomplete. A gameplay design, actual deployment/control implementation, and weapon-specific visual/animation integration are required. The unrelated existing NPC is not presented as a finished weapon. |',
         '| HL2 brickbat `weapon_brickbat` | Registration and global precache lines are commented out; it is not a registered weapon. It also has disabled pickup-ammo logic and uncompiled source. Remains an explicit restoration candidate excluded from grants, rather than activating an unsafe partially implemented class. |',
         '| TF2 `weapon_combat_chargeableplasma`, `weapon_infiltrator`, `weapon_laserdesignator`, `weapon_laserrifle`, `weapon_mortar`, `weapon_pistols` | Registered in their source files but excluded from the branch\'s Debug and Release server project inputs. Historical opt-in tooling is preserved. Simply adding source files does not validate their obsolete API usage, network counterparts, class/technology dependencies, zero-valued damage settings, mortar build-object chain, scripts, models, animations, projectiles, or effects. No default build inclusion or working status is fabricated. Chargeable-plasma and mortar client implementations are present in source; presence alone does not establish compatible builds. |',
         '| TF2 `weapon_objectselection`, `weapon_twohandedcontainer` | Internal builder/class-loadout objects. The selection needs a valid object subtype; the two-handed container needs compatible children. Neither is granted as an independently functioning weapon. Normal branch initialization paths are preserved. |',
         '| CS smoke grenade / other retail-only names | No concrete registration or source implementation exists in this supplied CS branch. No guessed registration, retail script, or another game\'s weapon is added. The definitive supported inventory below is the source inventory. |',
         '',
         'The original model UVs, origins, proportions, attachment transforms, hand positions, and animation curves cannot be edited or reconstructed faithfully from weapon C++ alone. No model source/geometry/animation files or authorized replacement mesh were supplied in the archive. Placeholder weapons/icons were not generated. Refer to the complete weapon asset audit for exact model, material, HUD texture and script paths actually found in an authorized installation.',
         '',
         '## Validation and compilation limits',
         '',
         '- Targeted weapon tests: 20 passed, 0 failed, 0 skipped, using the bundled Python and installed Visual Studio LLVM. Includes actual C++98 compile-and-execute checks for grant path/header/ammo policy and HUD atlas/fitting, source/inventory regression checks, branch contamination checks, and old/new generator idempotence. Command: `python -m unittest modernize.tests.test_weapon_restoration_v18 modernize.tests.test_weapon_asset_audit modernize.tests.test_weapon_hud modernize.tests.test_prototype_weapon_compile -v` with `clang++` available in PATH.',
         '- All active registrations were checked after stripping comments against preprocessor nesting: none of the current 113 branch inventory rows is hidden behind an unevaluated `#if`/`#ifdef` registration guard. Commented brickbat and duplicate historical thumper code are not counted.',
         '- `check_weapon_syntax_v18.py` makes 11 real clang-cl translation-unit probes using Release/Win32 server project definitions and includes; results and exact diagnostics are in `weapon_syntax_v18.json`. The probes are blocked by legacy compiler/header incompatibilities, so no server compilation pass is claimed. They did reach weapon bodies and exposed issues repaired here: const-correct CS ordering, missing IFileSystem include, private base weapon datamap access, old member-function callback syntax, and ambiguous TF2 network-handle comparisons.',
         '- Remaining diagnostic families include const-incorrect `Q_strrchr`/`Q_strstr` wrappers, ambiguous x86 inline-assembly operand sizes, a temporary reference in `props.h`, and existing player/datamap compiler issues. The JSON records the tested state; other agents may separately repair shared modernization code. A complete Windows build, linking, weapon selection, firing/reloading, multiplayer replication, class restrictions and asset loading still require future validation.',
         '',
         '## Definitive source inventory',
         '',
         'Each script path is derived from the actual `ReadWeaponDataFromFileForSlot` convention and is a required reference, not proof the script is supplied. “Project included” means both Debug and Release server inputs, not successful compilation. For detailed VM/world model, materials and HUD content, use the companion asset audit. All rows remain runtime-unverified.',
         '']
    base={'weapon_frag','weapon_crowbar','weapon_pistol','weapon_ar2','weapon_shotgun','weapon_smg1','weapon_gauss','weapon_physcannon','weapon_rpg'}
    for branch in ('hl2','hl1','cstrike','tf2'):
        label='cs' if branch=='cstrike' else branch
        grant=grants[label]
        builds={r['classname']:r for r in build['branches'][branch]['weapons']}
        out += ['### '+branch.upper(),'',
                '| Entity | Implementation | Required script | Server project | Grant/source status |',
                '|---|---|---|---|---|']
        for row in inventory['branches'][branch]:
            name=row['classname']
            if row['grant_exclusion_reason']:status='Excluded: '+row['grant_exclusion_reason']
            elif not builds[name]['server_compiled_all_configs']:status='Prototype excluded from default build'
            elif branch=='hl2' and name in base:status='Original base cheat; asset-validated spawn'
            elif branch=='hl1':status='Branch-local existing cheat; asset-validated spawn'
            else:status='Expanded cheat candidate; asset-validated spawn'
            if name in ('weapon_irifle','weapon_tripwire','weapon_thumper','weapon_immolator'):status+='; repaired source, runtime unverified'
            out.append('| `'+name+'` | `'+row['implementation_file']+'` | `scripts/'+name+'.txt` | '+('included' if builds[name]['server_compiled_all_configs'] else 'excluded')+' | '+status+' |')
        for row in inventory.get('unregistered_prototypes',{}).get(branch,[]):
            out.append('| `'+row['classname']+'` | `'+row['implementation_file']+'` | not a registered script contract | excluded/unregistered | Commented registration; unfinished prototype |')
        out += ['']
    out += ['## Reproduction and changed source', '',
            'Run `patch_weapon_restoration_v18.py` after the v17 patches/project conversion; it is in the standard apply chain. The historical grant patches preserve the v18 canonical manifest. Regenerate inventory/report data with the branch/build auditors and `write_weapon_report_v18.py`. No build projects, third-party assets, or binaries are silently enabled or replaced by this patch.', '',
            'Modified engine/game sources: `dlls/player.cpp`, `dlls/player.h`, the HL2/HL1/CS/TF2 player overrides, `weapon_irifle.cpp`, `weapon_tripwire.cpp`, `weapon_thumper.cpp`, `weapon_immolator.cpp`, `game_shared/basecombatweapon_shared.cpp`, `game_shared/hl2/basehlcombatweapon_shared.h`, `game_shared/tf2/weapon_twohandedcontainer.cpp`, `cl_dll/hud.cpp`, and CS/TF2 weapon-selection HUD source. Supporting changes: portable grant/header policy, extended HUD atlas policy, source inventory/build/grant JSON, regression/smoke tests, reproducible final patch, syntax-probe tool/report, report generator, the two historical grant generators, and the standard apply-chain entry.', '']
    path=ROOT/'modernize/reports/WEAPON_IMPLEMENTATION_V18.md'
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('\n'.join(out),encoding='utf-8')
    print(path)

if __name__=='__main__':main()
