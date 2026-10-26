"""Native stubbed Win32 test and static lifecycle checks. No Steam SDK needed."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class SteamBridgeTests(unittest.TestCase):
    def test_cpp98_lifecycle_with_fake_runtime(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('No native C++ compiler available')
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/('steamtest.exe' if os.name=='nt' else 'steamtest')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',
                            '-I',str(ROOT/'tests/winshim'),str(ROOT/'tests/steam_client_bridge_smoke.cpp'),
                            '-o',str(output)], text=True, capture_output=True)
            self.assertEqual(build.returncode,0,build.stderr)
            proc=subprocess.run([str(output)],check=True,text=True,capture_output=True)
            self.assertIn('lifecycle smoke tests passed',proc.stdout)
    def test_hooked_into_engine(self):
        source=(ROOT.parent/'engine/sys_mainwind.cpp').read_bytes().decode('latin-1')
        self.assertEqual(source.count('HL2MODERN_STEAM_CLIENT_LIFECYCLE'), 1)
        self.assertIn('CommandLine()->FindParm( "-steamworks" )',source)
        self.assertIn('!CommandLine()->FindParm( "-nosteamworks" )',source)
        self.assertIn('g_HL2ModernSteamClient.Pump();',source)
        self.assertIn('g_HL2ModernSteamClient.Stop();',source)
        self.assertIn('steam_client_bridge.h',source)
    def test_replay_script(self):
        text=(ROOT/'apply_modernization.py').read_text()
        self.assertIn('patch_steam_client.py',text)
if __name__=='__main__': unittest.main()
