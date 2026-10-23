"""Platform-neutral engine directory path regression tests."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class SafePathTests(unittest.TestCase):
    def test_native_boundary_policy(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler:self.skipTest('No native C++ compiler')
        includes=['-I',str(ROOT/'tests/cpp98shim')] if os.name=='nt' and 'clang' in Path(compiler).name.lower() else []
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/('pathtest.exe' if os.name=='nt' else 'pathtest')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror']+includes+
                            [str(ROOT/'tests/safe_paths_smoke.cpp'),'-o',str(output)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            result=subprocess.run([str(output)],check=True,capture_output=True,text=True)
            self.assertIn('path handling smoke tests passed',result.stdout)
    def test_engine_wiring(self):
        source=(ROOT.parent/'engine/sys_dll.cpp').read_bytes().decode('latin-1')
        self.assertIn('HL2MODERN_SAFE_DIRECTORY_PATHS',source)
        self.assertIn('CopyDirectory(path, testpath, sizeof(testpath))',source)
        self.assertNotIn('strcpy( testpath, path );',source)
        self.assertIn('CreateDirHierarchy( testpath, "GAME" )',source)
        self.assertIn('patch_engine_paths.py',(ROOT/'apply_modernization.py').read_text())
