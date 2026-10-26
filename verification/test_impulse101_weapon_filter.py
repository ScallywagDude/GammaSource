"""Focused tests for GammaSource's impulse 101 whitelist decisions.

Run: py -3 -m unittest verification.test_impulse101_weapon_filter -v
Requires a C++ compiler (CXX, then clang++ / g++ / cl) for the C++98 check.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'game/server/gamma/impulse101_weapon_filter.h'
SCRIPT = ROOT / 'gammasource_overlay/scripts/weapon_annabelle.txt'
PLAYER = ROOT / 'game/server/player.cpp'


class Impulse101WeaponFilterTests(unittest.TestCase):
    def test_annabelle_is_named(self):
        source = SCRIPT.read_text(encoding='utf-8')
        self.assertRegex(source, r'(?im)^\s*"printname"\s+"Annabelle"')

    def test_annabelle_script_has_weapondata(self):
        source = SCRIPT.read_text(encoding='utf-8')
        self.assertIn('WeaponData', source)
        self.assertIn('"viewmodel"', source)

    def test_filter_wired_to_impulse_101(self):
        source = PLAYER.read_text(encoding='latin-1')
        self.assertIn('"gamma/impulse101_weapon_filter.h"', source)
        self.assertRegex(source, r'GammaImpulse101ShouldGrantWeapon\s*\(\s*weapons\[i\]\s*\)')

    def test_cxx98_grant_policy(self):
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        if not compiler or os.path.basename(compiler).lower() in ('cl','cl.exe'):
            self.skipTest('No usable C++98 compiler found for portable test')
        harness = r'''#include "game/server/gamma/impulse101_weapon_filter.h"
#include <assert.h>
int main() {
    assert(GammaImpulse101ShouldGrantWeapon("weapon_annabelle"));
    assert(GammaImpulse101ShouldGrantWeapon("weapon_pistol"));
    assert(GammaImpulse101ShouldGrantWeapon("weapon_crowbar"));
    assert(!GammaImpulse101ShouldGrantWeapon("weapon_citizenpackage"));
    assert(!GammaImpulse101ShouldGrantWeapon("weapon_citizensuitcase"));
    assert(!GammaImpulse101ShouldGrantWeapon("weapon_cubemap"));
    assert(!GammaImpulse101ShouldGrantWeapon(NULL));
    assert(!GammaImpulse101ShouldGrantWeapon(""));
}
'''
        with tempfile.TemporaryDirectory(prefix='gamma_impulse101_') as td:
            p = Path(td)
            (p/'test.cpp').write_text(harness)
            cmd=[compiler, '-std=c++98', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT), str(p/'test.cpp'), '-o', str(p/'test')]
            cp=subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(cp.returncode,0,cp.stdout+'\n'+cp.stderr)
            ran=subprocess.run([str(p/'test')], capture_output=True, text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+'\n'+ran.stderr)

if __name__ == '__main__':
    unittest.main()
