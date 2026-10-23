from pathlib import Path
import json, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]
class ModelAndWeapons(unittest.TestCase):
    def test_playermodel_policy_native(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler:self.skipTest('no compiler')
        with tempfile.TemporaryDirectory() as td:
            exe=Path(td)/'test'
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                         str(ROOT/'modernize/tests/playermodel_policy_smoke.cpp'),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
    def test_ui_uses_vetted_model_names(self):
        menu=(ROOT/'gameui/OptionsSubMultiplayer.cpp').read_bytes().decode('latin1')
        opts=(ROOT/'gameui/OptionsDialog.cpp').read_bytes().decode('latin1')
        player=(ROOT/'dlls/hl2_dll/hl2_player.cpp').read_bytes().decode('latin1')
        self.assertEqual(menu.count('HL2MODERN_MULTIPLAYER_MODELS'),1)
        self.assertEqual(opts.count('HL2MODERN_MULTIPLAYER_TAB'),1)
        self.assertEqual(player.count('HL2MODERN_HL2_PLAYERMODEL'),1)
        self.assertIn('HL2ModernPlayerModels::Find(requested)',player)
        self.assertIn('HL2Modern_CompatiblePlayerModel(entry->path)',player)  # v19 validates source format, not just a filename
        self.assertIn('m_pModelList->ApplyChanges()',menu)
    def test_branch_separation(self):
        registry=json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text())['branches']
        for b in ('hl2','hl1','cstrike','tf2'):self.assertTrue(registry[b])
        self.assertIn('weapon_immolator',[x['classname'] for x in registry['hl2']])
        self.assertNotIn('weapon_immolator',[x['classname'] for x in registry['tf2']])
        self.assertIn('weapon_minigun',[x['classname'] for x in registry['tf2']])
        self.assertNotIn('weapon_minigun',[x['classname'] for x in registry['hl2']])
    def test_regenerator_is_idempotent(self):
        paths=[ROOT/'gameui/OptionsDialog.cpp',ROOT/'gameui/OptionsSubMultiplayer.cpp',ROOT/'dlls/hl2_dll/hl2_player.cpp']
        before=[p.read_bytes() for p in paths]
        run=subprocess.run([sys.executable,str(ROOT/'modernize/patch_player_model_selection.py')],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        self.assertEqual(before,[p.read_bytes() for p in paths])
if __name__=='__main__':unittest.main()
