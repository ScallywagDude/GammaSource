"""Native C++98 Crab Synth hit-policy and source wiring tests."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
import json
ROOT = Path(__file__).resolve().parents[2]

class CrabSynthTests(unittest.TestCase):
    def test_charge_decisions_native_cpp98(self):
        cpp = shutil.which('g++') or shutil.which('clang++')
        if not cpp:
            self.skipTest('No native C++ compiler installed')
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / ('crabsynth.exe' if os.name == 'nt' else 'crabsynth')
            proc = subprocess.run([cpp, '-std=c++98', '-Wall', '-Wextra',
                                   '-pedantic', '-Werror',
                                   str(ROOT/'modernize/tests/crabsynth_charge_smoke.cpp'),
                                   '-o', str(target)], capture_output=True, text=True)
            self.assertEqual(proc.returncode,0,proc.stderr)
            result = subprocess.run([str(target)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_npc_source_wiring(self):
        code=(ROOT/'dlls/hl2_dll/npc_crabsynth.cpp').read_bytes().decode('latin-1').replace('\r\n','\n')
        self.assertEqual(code.count('// HL2MODERN_CRABSYNTH_CHARGE_DAMAGE'),1)
        self.assertIn('HL2ModernCrabCharge::Classify(',code)
        self.assertIn('sk_crabsynth_charge_dmg.GetFloat()',code)
        self.assertIn('pEnemy->TakeDamage(info)',code)
        self.assertIn('!pEnemy->IsAlive() || !FVisible(pEnemy)',code)
        self.assertIn('SetSchedule(SCHED_CRAB_CHARGE_HITRIGHT)',code)
        self.assertIn('!GetEnemy() || IsUnreachable(GetEnemy())',code)
        self.assertIn('TaskFail(FAIL_NO_ROUTE);\n\t\t\t\treturn;',code)

    def test_inventory_describes_source_only_status(self):
        entries=json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
        crab=next(e for e in entries if e['name']=='npc_crabsynth')
        self.assertTrue(crab['charge_hit_source_patch'])
        self.assertFalse(crab['runtime_verified'])
        self.assertFalse(crab['assets_verified'])

    def test_patch_idempotent(self):
        path=ROOT/'dlls/hl2_dll/npc_crabsynth.cpp'
        before=path.read_bytes()
        result=subprocess.run(['python',str(ROOT/'modernize/patch_crabsynth.py')],
                              capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(before,path.read_bytes())
        self.assertIn('patch_crabsynth.py',(ROOT/'modernize/apply_modernization.py').read_text())

if __name__=='__main__': unittest.main()
