from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class WeaponHUDTests(unittest.TestCase):
    def test_hud_scaling_portable_cpp98(self):
        cc=shutil.which('g++') or shutil.which('clang++')
        if not cc:self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/('fit.exe' if os.name=='nt' else 'fit')
            p=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(ROOT/'modernize/tests/hud_fit_smoke.cpp'),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stderr)
    def test_source_hud_routes_icons_without_overscaling(self):
        src=(ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp').read_bytes().decode('latin1')
        self.assertEqual(src.count('HL2MODERN_WEAPON_HUD_FIT'),1)
        self.assertIn('pWeapon->GetSpriteInactive()',src)
        self.assertIn('HL2ModernHud::FitIcon',src)
        self.assertIn('sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col)',src)
    def test_patch_idempotence(self):
        src=ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp'
        before=src.read_bytes()
        p=subprocess.run([sys.executable,str(ROOT/'modernize/patch_weapon_hud.py')],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(before,src.read_bytes())
        self.assertIn('patch_weapon_hud.py',(ROOT/'modernize/apply_modernization.py').read_text())
if __name__=='__main__':unittest.main()
