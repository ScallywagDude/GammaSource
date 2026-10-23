#!/usr/bin/env python3
"""Apply all checked-in HL2 2003 modernization overlays to an extracted tree.

Usage from source root:
    py -3 modernize/apply_modernization.py
This tool does not download Steamworks, engine assets, or closed-source SDKs.
"""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
scripts=['convert_dsp.py','patch_xinput.py','patch_npc_create.py','patch_crabsynth.py','patch_houndeye.py','patch_bullsquid.py','patch_player_model_selection.py','expand_hl2_models.py','patch_preview_status.py','patch_model_dropdown_integrity.py','patch_model_path_policy.py','patch_impulse101_branches.py','patch_grants_buildaware.py','patch_weapon_grant_diagnostics.py','patch_preview_bitmap.py','patch_multiplayer_model_ui_safety.py','patch_weapon_hud.py','patch_weapon_icon_fallback.py','patch_weapon_asset_safety.py','patch_weapon_restoration_v18.py','patch_stickylauncher_spawn.py','patch_hopwire_spawn.py','restore_hopwire_combat.py','restore_manhack_control.py','restore_tf2_prototype_weapons.py','restore_brickbat.py','patch_hl1_weapon_buckets.py','patch_renderer_select.py','patch_multiplayer_chat.py','patch_fair_spawns.py','patch_map_votes.py','patch_xinput_deadzone.py','patch_steam_client.py','patch_engine_paths.py','patch_spawn_protection.py','patch_lighting.py','patch_team_balance.py','patch_lightstyles.py','patch_server_text.py','patch_deathnotice.py','generate_cut_npcs.py','generate_npc_testmaps.py','generate_multiplayer_testmap.py','weapon_branch_audit.py','weapon_build_audit.py','weapon_asset_audit.py','weapon_asset_audit_v23.py','build_audit.py','validate.py']
# Preserve the historical patch order, then apply content-dependent repairs.
for anchor, extra in [('patch_bullsquid.py','patch_npc_assets.py'),
                      ('patch_lighting.py','patch_material_assets.py'),
                      ('convert_dsp.py','patch_windows_crt_compat.py'),
                      ('patch_hopwire_spawn.py','patch_repairgun_targets.py'),
                      ('convert_dsp.py','patch_legacy_shader_tools.py')]:
    if extra not in scripts:
        scripts.insert(scripts.index(anchor)+1,extra)
if not (root/'dlls'/'hl.dsp').exists() or not (root/'engine'/'engine.dsp').exists():
    raise SystemExit('Extract the original source_2003_leak.zip first, then overlay these files into the same root.')
# Entity syntax compatibility must follow every historical source generator.
scripts.insert(scripts.index('validate.py'),'patch_hud_customization.py')
scripts.insert(scripts.index('validate.py'),'patch_debug_class_validation.py')
scripts.insert(scripts.index('validate.py'),'patch_follow_handles.py')
scripts.insert(scripts.index('validate.py'),'patch_entity_compiler_compat.py')
# Keep post-overlay compiler repairs after all source regenerators.
scripts.insert(scripts.index('validate.py'),'fix_legacy_compiler_errors.py')
# Restore archived Tracker prerequisites and legacy include spellings after DSP conversion.
scripts.insert(scripts.index('validate.py'),'repair_tracker_source_paths.py')
# Reconcile the old Tracker UI source with the existing VGUI2 control interfaces.
scripts.insert(scripts.index('validate.py'),'repair_tracker_vgui2_api.py')
for script in scripts:
    print(f'Running {script}...',flush=True)
    cmd=[sys.executable,str(root/'modernize'/script)]
    if script == 'convert_dsp.py': cmd.append('--check')
    subprocess.run(cmd,check=True,cwd=root)
print('Structural checks passed. Compile/test on Windows before considering any phase complete.')
