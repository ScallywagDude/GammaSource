"""v17: actual weapon grant results and bounded branch-local model selection."""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

class Phase7Phase8V17(unittest.TestCase):
    def test_all_three_branch_grants_report_results(self):
        for rel in ('dlls/hl2_dll/hl2_player.cpp', 'dlls/cstrike/cs_player.cpp', 'dlls/tf2_dll/tf_player.cpp'):
            src=(ROOT/rel).read_bytes().decode('latin1')
            self.assertEqual(src.count('HL2MODERN_IMPULSE101_RESULT'),1,rel)
            self.assertIn('HasNamedPlayerItem(weapons[k])',src)
            self.assertIn('failed %u',src)
            self.assertIn('++failed',src)
            self.assertIn('++already',src)
    def test_hl1_grants_not_changed(self):
        for rel in ('dlls/hl1/hl1_player.cpp','dlls/hl1_dll/hl1_player.cpp'):
            if (ROOT/rel).is_file():
                self.assertNotIn('HL2MODERN_IMPULSE101_RESULT',(ROOT/rel).read_bytes().decode('latin1'))
    def test_combo_does_not_read_stale_index(self):
        src=(ROOT/'gameui/LabeledCommandComboBox.cpp').read_bytes().decode('latin1')
        self.assertEqual(src.count('HL2MODERN_COMBO_INDEX_GUARD'),1)
        self.assertIn('m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count()',src)
        self.assertIn('if (safeText[0] ==',src)
    def test_unselected_model_clears_previous_geometry(self):
        src=(ROOT/'gameui/OptionsSubMultiplayer.cpp').read_bytes().decode('latin1')
        idx=src.index('if( !modelname[0] )')
        self.assertIn('m_pModelPreview->ClearModel()',src[idx:idx+400])
        selected=src.split('m_pModelPreview->SetModel(entry->path)',1)[1]
        self.assertIn('else',selected[:100])
        self.assertIn('m_pModelPreview->ClearModel()',selected[:150])
    def test_model_branch_path_handling_compiles_in_cpp98(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler:self.skipTest('no native compiler')
        script=r'''#include "hl2_playermodel_policy.h"
#include <cassert>
int main() {
 using namespace HL2ModernPlayerModels;
 assert(IsHL2GameDir("HL2"));
 assert(IsHL2GameDir("C:\\games\\HL2\\"));
 assert(IsHL2GameDir("/games/HL2MP///"));
 assert(IsHL2GameDir("hl2mp"));
 assert(!IsHL2GameDir("/games/HL2MP-other/"));
 assert(!IsHL2GameDir("/games/tf2/"));
 assert(!IsHL2GameDir(""));
 assert(!IsHL2GameDir("/"));
 assert(!IsHL2GameDir((const char*)0));
}
'''
        with tempfile.TemporaryDirectory() as td:
            f=Path(td)/'test.cpp';exe=Path(td)/'test'
            f.write_text(script)
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT/'modernize'),str(f),'-o',str(exe)],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
    def test_reapply_all_three_patches_is_idempotent(self):
        files=['gameui/LabeledCommandComboBox.cpp','gameui/OptionsSubMultiplayer.cpp','modernize/hl2_playermodel_policy.h',
               'dlls/hl2_dll/hl2_player.cpp','dlls/cstrike/cs_player.cpp','dlls/tf2_dll/tf_player.cpp']
        original={rel:(ROOT/rel).read_bytes() for rel in files}
        for name in ('patch_model_dropdown_integrity.py','patch_model_path_policy.py','patch_weapon_grant_diagnostics.py'):
            p=subprocess.run([sys.executable,str(ROOT/'modernize'/name)],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(p.returncode,0,name+': '+p.stderr)
        self.assertEqual(original,{rel:(ROOT/rel).read_bytes() for rel in files})

if __name__=='__main__':unittest.main()
