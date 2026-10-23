"""Compile real spawn policy in C++98 plus assert in-game source hooks."""
from pathlib import Path
import unittest, shutil, subprocess, tempfile, os
ROOT=Path(__file__).resolve().parents[2]
class SpawnProtectionTests(unittest.TestCase):
    def test_native_cpp98_policy(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('Native C++ compiler not available')
        with tempfile.TemporaryDirectory() as tmp:
            binary=Path(tmp)/('spawn.exe' if os.name=='nt' else 'spawn')
            subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(ROOT/'modernize/tests/spawn_protection_smoke.cpp'),'-o',str(binary)],capture_output=True,text=True,check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,check=True)
            self.assertIn('spawn protection policy tests passed',result.stdout)
    def test_server_damage_attack_spawn_and_cleanup(self):
        text=(ROOT/'dlls/player.cpp').read_bytes().decode('latin-1')
        self.assertEqual(text.count('HL2MODERN_SPAWN_PROTECTION'),1)
        self.assertIn('mp_spawn_protection_seconds.GetFloat()',text)
        self.assertIn('g_HL2ModernSpawnGuard.Begin(entindex()',text)
        self.assertIn('g_HL2ModernSpawnGuard.Protected(entindex()',text)
        self.assertIn('g_HL2ModernSpawnGuard.Cancel(entindex()',text)
        self.assertIn('m_nButtons & (IN_ATTACK | IN_ATTACK2)',text)
        self.assertIn('info.GetAttacker()->IsPlayer()',text)
        server=(ROOT/'dlls/gameinterface.cpp').read_bytes().decode('latin-1')
        self.assertIn('HL2Modern_ResetSpawnProtectionSlot( ENTINDEX( pEdict ) );',server)
        self.assertIn('HL2Modern_ResetSpawnProtectionSlot(ENTINDEX(pEntity));',server)
        self.assertIn('extern void HL2Modern_ResetSpawnProtectionSlot(int slot);',server)
    def test_config_default(self):
        for name in ('server_deathmatch.cfg','server_teamdeathmatch.cfg'):
            config=(ROOT/'modernize/configs'/name).read_text()
            self.assertIn('mp_spawn_protection_seconds 2',config)
        self.assertIn('patch_spawn_protection.py',(ROOT/'modernize/apply_modernization.py').read_text())
if __name__=='__main__': unittest.main()
