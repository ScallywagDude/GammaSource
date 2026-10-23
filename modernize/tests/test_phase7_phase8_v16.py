"""Phase 7/8 hardening: only compiled branch weapons, visible HUD fallback, more vetted HL2 models."""
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

class ScopedPhases(unittest.TestCase):
    def test_weapon_list_matches_compiled_inventory(self):
        audit=json.loads((ROOT/'modernize/weapon_build_report.json').read_text())['branches']
        manifest=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        lookup={'hl2':'hl2','cs':'cstrike','tf2':'tf2'}
        for row in manifest:
            if row['branch'] not in lookup:continue
            branch=lookup[row['branch']]
            comp={r['classname'] for r in audit[branch]['weapons'] if r['server_compiled_all_configs']}
            self.assertEqual(set(row['compiled_impulse101_extra']),set(row['impulse101_extra']) & comp)
            self.assertEqual(set(row['excluded_not_compiled']),set(row['impulse101_extra']) - comp)
    def test_tf2_excluded_prototypes_not_granted(self):
        report=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        tf2=next(x for x in report if x['branch']=='tf2')
        data=(ROOT/'dlls/tf2_dll/tf_player.cpp').read_bytes().decode('latin1')
        start=data.index('HL2MODERN_BRANCH_IMPULSE101_TF2')
        array=data[start:data.index('};',start)]
        for cls in tf2['excluded_not_compiled']:
            self.assertNotIn('"'+cls+'"',array)
        for cls in tf2['compiled_impulse101_extra']:
            self.assertIn('"'+cls+'"',array)
    def test_hl2_uncompiled_weapon_not_granted(self):
        report=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        hl2=next(x for x in report if x['branch']=='hl2')
        self.assertIn('weapon_brickbat',hl2['excluded_not_compiled'])
        data=(ROOT/'dlls/hl2_dll/hl2_player.cpp').read_bytes().decode('latin1')
        start=data.index('HL2MODERN_BRANCH_IMPULSE101_HL2')
        self.assertNotIn('"weapon_brickbat"',data[start:data.index('};',start)])
    def test_weapon_hud_icon_label_when_missing(self):
        data=(ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp').read_bytes().decode('latin1')
        self.assertEqual(data.count('HL2MODERN_MISSING_WEAPON_ICON'),1)
        self.assertIn('if (!bIconDrawn)',data)
        self.assertIn('const char *fallback = "WPN"',data)
        self.assertIn('bIconDrawn = true;',data)
    def test_hl2_model_list_extended_but_branch_specific(self):
        hdr=(ROOT/'modernize/hl2_playermodel_policy.h').read_text()
        self.assertIn('group03m/male_01.mdl',hdr)
        self.assertIn('group01/female_07.mdl',hdr)
        self.assertNotIn('models/tf2/',hdr)
        self.assertIn('IsHL2GameDir',hdr)
        self.assertLessEqual(hdr.count('{ "'),64)
        self.assertIn('if (!filesystem()->FileExists(entry.path)) continue;', (ROOT/'gameui/OptionsSubMultiplayer.cpp').read_bytes().decode('latin1'))
    def test_model_list_compiles_native_cpp98(self):
        cc=shutil.which('g++') or shutil.which('clang++')
        if not cc:self.skipTest('no C++ compiler')
        with tempfile.TemporaryDirectory() as td:
            binary=Path(td)/'models'
            p=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                str(ROOT/'modernize/tests/playermodel_policy_smoke.cpp'),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr)
    def test_preview_status_not_false_3d(self):
        src=(ROOT/'gameui/OptionsSubMultiplayer.cpp').read_bytes().decode('latin1')
        self.assertIn('HL2MODERN_ANIMATED_PREVIEW',src)
        self.assertIn('m_pModelPreview->SetModel(entry->path)',src)
        engine=(ROOT/'engine/gameui_modelpreview.cpp').read_text()
        self.assertIn('g_pStudioRender->DrawModel(info, origin',engine)
        self.assertIn('m_Status = "Model geometry could not be loaded"',engine)
    def test_generators_idempotent(self):
        paths=['modernize/hl2_playermodel_policy.h','modernize/weapon_impulse101_manifest.json',
          'gameui/OptionsSubMultiplayer.cpp','cl_dll/hl2_hud/hud_weaponselection.cpp',
          'dlls/hl2_dll/hl2_player.cpp','dlls/cstrike/cs_player.cpp','dlls/tf2_dll/tf_player.cpp']
        before={p:(ROOT/p).read_bytes() for p in paths}
        for script in ('patch_grants_buildaware.py','expand_hl2_models.py','patch_preview_status.py','patch_weapon_icon_fallback.py'):
            proc=subprocess.run([sys.executable,str(ROOT/'modernize'/script)],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(proc.returncode,0,script+': '+proc.stderr)
        after={p:(ROOT/p).read_bytes() for p in paths}
        self.assertEqual(before,after)

if __name__=='__main__':unittest.main()
