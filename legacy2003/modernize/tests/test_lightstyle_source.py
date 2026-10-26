"""Tests for bounded network lightstyle parsing and cache invalidation."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]

class LightstyleTests(unittest.TestCase):
    def test_cpp98_lighting_policy(self):
        cpp=shutil.which('g++') or shutil.which('clang++')
        if not cpp: self.skipTest('No C++ compiler')
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/('styles.exe' if os.name=='nt' else 'styles')
            result=subprocess.run([cpp,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',
                   str(ROOT/'modernize/tests/lightstyle_policy_smoke.cpp'),'-o',str(exe)],
                   capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('lightstyle policy tests passed',run.stdout)

    def test_2003_lightstyle_bug_and_network_fixed(self):
        client=(ROOT/'engine/cl_parse.cpp').read_bytes().decode('latin-1')
        engine=(ROOT/'engine/gl_rlight.cpp').read_bytes().decode('latin-1')
        self.assertEqual(client.count('// HL2MODERN_BOUNDED_LIGHTSTYLE_FROM_NETWORK'),1)
        self.assertEqual(client.count('Q_strncpy (cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));'),2)
        self.assertNotIn('Q_strcpy (cl_lightstyle[i].map,  MSG_ReadString());',client)
        self.assertEqual(engine.count('// HL2MODERN_SMOOTH_LIGHTSTYLES'),1)
        self.assertIn('r_smooth_lightstyles( "r_smooth_lightstyles", "0"',engine)
        self.assertIn('HL2ModernLightstyles::Sample(',engine)
        self.assertIn('d_lightstyleframe[j] = r_framecount;',engine)

    def test_regenerator_is_idempotent(self):
        before=[(ROOT/f).read_bytes() for f in ('engine/gl_rlight.cpp','engine/cl_parse.cpp')]
        out=subprocess.run(['python',str(ROOT/'modernize/patch_lightstyles.py')],capture_output=True,text=True)
        self.assertEqual(out.returncode,0,out.stderr)
        after=[(ROOT/f).read_bytes() for f in ('engine/gl_rlight.cpp','engine/cl_parse.cpp')]
        self.assertEqual(before,after)
        self.assertIn('patch_lightstyles.py',(ROOT/'modernize/apply_modernization.py').read_text())
