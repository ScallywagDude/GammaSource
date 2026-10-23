"""Branch isolation, C++98 policy compilation, idempotence and preview wiring."""
import json, os, shutil, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def text(p):return (ROOT/p).read_bytes().decode('latin1')
class ModelWeaponTests(unittest.TestCase):
    def test_cpp98_model_allowlist(self):
        cc=shutil.which('g++') or shutil.which('clang++')
        if not cc:self.skipTest('C++ compiler not installed')
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/('model.exe' if os.name=='nt' else 'model')
            p=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',str(ROOT/'modernize/tests/playermodel_policy_smoke.cpp'),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
    def test_menu_and_server_models(self):
        dialog=text('gameui/OptionsDialog.cpp')
        self.assertIn('HL2MODERN_MULTIPLAYER_TAB',dialog)
        self.assertIn('HL2ModernPlayerModels::IsHL2GameDir',dialog)
        opts=text('gameui/OptionsSubMultiplayer.cpp')
        self.assertIn('HL2MODERN_MULTIPLAYER_MODELS',opts)
        self.assertIn('HL2MODERN_ANIMATED_PREVIEW',opts)
        self.assertIn('m_pModelPreview->SetModel(entry->path)',opts)
        self.assertIn('gameuimodelpreview->IsModelAvailable(entry.path)',opts)
        self.assertNotIn('vgui/playermodels/%s',opts)
        self.assertIn('HL2ModernPlayerModels::Find(safePath)',opts)
        combo=text('gameui/LabeledCommandComboBox.cpp')
        self.assertIn('HL2MODERN_COMBO_LIVE_UPDATE',combo)
        self.assertIn('changed->SetPtr("panel", this)',combo)
        server=text('dlls/hl2_dll/hl2_player.cpp')
        self.assertIn('HL2MODERN_HL2_PLAYERMODEL',server)
        self.assertIn('engine->InfoKeyValue(info,"model")',server)
        self.assertNotIn("'void CHL2_Player",server)
    def test_branch_weapon_guards(self):
        inv=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        self.assertEqual({x['branch'] for x in inv},{'hl2','cs','tf2','hl1'})
        src={'hl2':text('dlls/hl2_dll/hl2_player.cpp'),'cs':text('dlls/cstrike/cs_player.cpp'),'tf2':text('dlls/tf2_dll/tf_player.cpp')}
        for entry in inv:
            self.assertFalse(entry['assets_verified'])
            if entry['branch']=='hl1':continue
            name=entry['branch']
            self.assertIn('HL2MODERN_BRANCH_IMPULSE101_'+name.upper(),src[name])
            for wep in entry.get('compiled_impulse101_extra',entry['impulse101_extra']):
                self.assertIn('"'+wep+'"',src[name])
            # Branches must never receive weapons from another branch-only roster.
            unique=set(entry['impulse101_extra'])-set().union(*(set(other.get('impulse101_extra',[])) for other in inv if other['branch']!=name))
            for other_name,other_src in src.items():
                if other_name!=name:
                    for wep in unique:self.assertNotIn('"'+wep+'"',other_src)
    def test_run_scripts_repeatably(self):
        files=['dlls/player.cpp','dlls/hl2_dll/hl2_player.cpp','dlls/cstrike/cs_player.cpp','dlls/tf2_dll/tf_player.cpp','gameui/OptionsSubMultiplayer.cpp','gameui/OptionsDialog.cpp','gameui/LabeledCommandComboBox.cpp','gameui/BitmapImagePanel.cpp']
        before={name:(ROOT/name).read_bytes() for name in files}
        for script in ('patch_player_model_selection.py','patch_impulse101_branches.py','patch_preview_bitmap.py'):
            result=subprocess.run([sys.executable,str(ROOT/'modernize'/script)],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
        for name in files:self.assertEqual(before[name],(ROOT/name).read_bytes(),name)
        apply=text('modernize/apply_modernization.py')
        for script in ('patch_player_model_selection.py','patch_impulse101_branches.py','patch_preview_bitmap.py'):self.assertIn(script,apply)
if __name__=='__main__':unittest.main()
