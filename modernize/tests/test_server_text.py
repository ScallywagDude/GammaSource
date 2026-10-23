"""Bounded multiplayer mapcycle and MOTD parsing regression tests."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class ServerTextTests(unittest.TestCase):
    def test_native_policy(self):
        cpp = shutil.which('g++') or shutil.which('clang++')
        if not cpp:
            self.skipTest('No native compiler')
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp)/('server_text.exe' if os.name == 'nt' else 'server_text')
            cc = subprocess.run([cpp, '-std=c++98', '-Wall', '-Wextra', '-pedantic', '-Werror',
                        str(ROOT/'modernize/tests/server_text_smoke.cpp'), '-o', str(exe)],
                        capture_output=True, text=True)
            self.assertEqual(cc.returncode, 0, cc.stdout+cc.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout+run.stderr)
            self.assertIn('server text policy tests passed', run.stdout)

    def test_actual_multiplayer_source(self):
        src = (ROOT/'game_shared/multiplay_gamerules.cpp').read_bytes().decode('latin-1')
        self.assertEqual(src.count('HL2MODERN_BOUNDED_MAPCYCLE_MOTD'), 1)
        self.assertIn('HL2ModernServerText::ParseMapcycle(', src)
        self.assertIn('HL2ModernServerText::SelectNextMap(', src)
        self.assertIn('HL2ModernServerText::ReadMotdChunk(', src)
        self.assertIn('engine->FreeFile(fileData);', src)
        self.assertIn('engine->FreeFile(data);', src)
        self.assertNotIn('sscanf( pFileList, " %32s"', src)
        self.assertNotIn('strlen( pFileList )', src)
        self.assertIn('patch_server_text.py', (ROOT/'modernize/apply_modernization.py').read_text())
        self.assertIn('else if ( pInflictor )', src)
        self.assertIn('HL2MODERN_NULL_INFLICTOR', src)

    def test_regenerator_idempotent(self):
        target = ROOT/'game_shared/multiplay_gamerules.cpp'
        before = target.read_bytes()
        for script in ('patch_server_text.py', 'patch_deathnotice.py'):
            proc = subprocess.run(['python', str(ROOT/'modernize'/script)],
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(target.read_bytes(), before)

if __name__ == '__main__': unittest.main()
