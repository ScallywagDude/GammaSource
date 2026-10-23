"""Numerical checks for modern postprocess reference and legacy gamma protections."""
from pathlib import Path
import math, os, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]

def saturate(value): return min(1.0,max(0.0,value))
def aces(x): return saturate((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14))
def srgb(x):
    x=saturate(x)
    return 12.92*x if x<=0.0031308 else 1.055*(x**(1/2.4))-0.055

class LightingTest(unittest.TestCase):
    def test_native_gamma_inputs_and_fresnel(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as d:
            out=Path(d)/('lighting.exe' if os.name=='nt' else 'lighting')
            proc=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',
                            str(ROOT/'tests/lighting_parameters_smoke.cpp'),'-o',str(out)],
                            text=True,capture_output=True)
            self.assertEqual(proc.returncode,0,proc.stderr)
            res=subprocess.run([str(out)],text=True,capture_output=True)
            self.assertEqual(res.returncode,0,res.stderr)
            self.assertIn('lighting parameter tests passed',res.stdout)

    def test_aces_exposure_monotonic(self):
        samples=[aces(i/16.0) for i in range(256)]
        self.assertTrue(all(0<=v<=1 and math.isfinite(v) for v in samples))
        self.assertEqual(samples, sorted(samples))
        self.assertEqual(aces(0),0)

    def test_srgb_stays_bounded(self):
        for i in range(1001):
            x=i/1000.0
            self.assertTrue(0<=srgb(x)<=1.000001)
        self.assertAlmostEqual(srgb(0.0),0)
        self.assertAlmostEqual(srgb(1.0),1.0)
        self.assertAlmostEqual(srgb(0.5),0.73535698,places=6)

    def test_legacy_renderer_wiring(self):
        source=(ROOT.parent/'materialsystem/colorspace.cpp').read_bytes().decode('latin-1')
        self.assertIn('HL2MODERN_GAMMA_INPUT_GUARD',source)
        self.assertIn('SanitizeGammaParameters( screenGamma, texGamma, overbright )',source)
        shader=(ROOT.parent/'materialsystem/stdshaders/common_ps_fxc.h').read_bytes().decode('latin-1')
        lightmap=(ROOT.parent/'materialsystem/stdshaders/lightmappedgeneric_ps20.fxc').read_bytes().decode('latin-1')
        self.assertIn('saturate( HALF_CONSTANT(1.0f) - dot( normal, eye ) )',shader)
        self.assertIn('saturate( 1.0 - dot( worldSpaceNormal, eyeVect ) )',lightmap)
        self.assertIn('patch_lighting.py',(ROOT/'apply_modernization.py').read_text())

    def test_shader_sources_and_optional_build_plan(self):
        paths=list((ROOT/'shaders').rglob('*'))
        self.assertEqual(len([p for p in paths if p.is_file() and p.suffix in ('.hlsl','.vert','.frag')]),5)
        plan=subprocess.run([sys.executable,str(ROOT/'build_modern_shaders.py'),'--plan'],
                            capture_output=True,text=True)
        self.assertEqual(plan.returncode,0,plan.stderr)
        for api in ('Direct3D 11','Direct3D 12','OpenGL 3.3','Vulkan'):
            self.assertIn(api,plan.stdout)
        self.assertEqual(plan.stdout.count('Direct3D 11'),2)

if __name__=='__main__': unittest.main()
