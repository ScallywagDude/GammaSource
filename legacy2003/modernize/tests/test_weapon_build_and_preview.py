"""Build-aware inventory and multiplayer model UI regression checks."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('weapon_build_audit',ROOT/'modernize/weapon_build_audit.py')
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)

class WeaponBuildAndPreviewTests(unittest.TestCase):
    def test_branch_server_configuration_inclusion(self):
        report=mod.audit()
        self.assertEqual(set(report['branches']),{'hl2','hl1','cstrike','tf2'})
        # Linux paths must compare case-insensitively against 2003 MSVC inputs.
        for branch in ('hl2','hl1','cstrike','tf2'):
            self.assertEqual(report['branches'][branch]['server_excluded'],[])
        self.assertNotIn('weapon_minigun',report['branches']['tf2']['server_excluded'])
    def test_both_build_modes_explicit(self):
        report=mod.audit()
        item=next(x for x in report['branches']['hl2']['weapons'] if x['classname']=='weapon_binoculars')
        self.assertTrue(item['debug']['server'])
        self.assertTrue(item['release']['server'])
    def test_persisted_report_matches_generator(self):
        report=json.loads((ROOT/'modernize/weapon_build_report.json').read_text())
        self.assertEqual(report,mod.audit())
    def test_legacy_combo_initial_index_mapping(self):
        text=(ROOT/'gameui/LabeledCommandComboBox.cpp').read_bytes().decode('latin1')
        self.assertIn('HL2MODERN_MODEL_UI_SAFETY',text)
        section=text.split('void CLabeledCommandComboBox::SetInitialItem(int index)',1)[1].split('void CLabeledCommandComboBox::OnTextChanged',1)[0]
        self.assertIn('ActivateItem(index)',section)
        self.assertNotIn('ActivateItem(comboBoxID)',section)
        self.assertNotIn('strcpy( item->command, engineCommand );',text)
        self.assertIn('index >= 0 && index < m_Items.Count()',text)
    def test_portrait_invalid_data_cleared(self):
        menu=(ROOT/'gameui/OptionsSubMultiplayer.cpp').read_bytes().decode('latin1')
        bitmap=(ROOT/'gameui/BitmapImagePanel.cpp').read_bytes().decode('latin1')
        self.assertIn('No installed compatible models',menu)
        self.assertNotIn('strcpy( currentmodel, p );',menu)
        self.assertIn('m_bUploaded = false;',bitmap)
        self.assertEqual(bitmap.count('#include <tier0/memdbgon.h>'),1)
    def test_patch_is_idempotent(self):
        files=['gameui/LabeledCommandComboBox.cpp','gameui/OptionsSubMultiplayer.cpp','gameui/BitmapImagePanel.cpp']
        before={p:(ROOT/p).read_bytes() for p in files}
        proc=subprocess.run([sys.executable,str(ROOT/'modernize/patch_multiplayer_model_ui_safety.py')],capture_output=True,text=True)
        self.assertEqual(proc.returncode,0,proc.stderr)
        self.assertEqual(before,{p:(ROOT/p).read_bytes() for p in files})
        self.assertIn('patch_multiplayer_model_ui_safety.py',(ROOT/'modernize/apply_modernization.py').read_text())
        self.assertIn('weapon_build_audit.py',(ROOT/'modernize/apply_modernization.py').read_text())
if __name__=='__main__':unittest.main()
