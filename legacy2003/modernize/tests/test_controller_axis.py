import unittest, shutil, subprocess, tempfile, os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class ControllerAxisTests(unittest.TestCase):
    def test_native_axis(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('No native compiler')
        # GNU/libstdc++ already supplies C++98 wrappers; only the Windows
        # clang/MSVC-STL combination needs this explicit C CRT namespace bridge.
        includes=['-I',str(ROOT/'tests/cpp98shim')] if os.name=='nt' and 'clang' in Path(compiler).name.lower() else []
        with tempfile.TemporaryDirectory() as tmp:
            binary=Path(tmp)/('axis.exe' if os.name=='nt' else 'axis')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic']+includes+
                                  [str(ROOT/'tests/controller_axis_smoke.cpp'),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True,check=True)
            self.assertIn('controller axis tests passed',result.stdout)
    def test_button_bitmask_resets_each_sample(self):
        data=(ROOT.parent/'cl_dll/in_joystick.cpp').read_bytes().decode('latin-1')
        self.assertIn('ji.dwButtons = 0; // button bitmask',data)
        self.assertIn('HL2ModernController::RadialDeadzone',data)
if __name__=='__main__': unittest.main()
