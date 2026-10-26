"""Exercise real Debug registration macros through private base declarations."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DebugClassValidation(unittest.TestCase):
    def test_private_three_level_registration_chain(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('Portable C++ compiler required')
        header = (ROOT / 'public/networkvar.h').read_bytes().decode('latin1').replace('\r\n', '\n')
        bridge = header.split('// HL2MODERN_DEBUG_CLASS_BRIDGE:', 1)[1].split('#ifdef _DEBUG', 1)[0]
        bridge = bridge[bridge.index('template <typename T>'):]
        macros = header.split('#ifdef _DEBUG', 1)[1].split('#else', 1)[0]
        source = '''#include <assert.h>
#include <string.h>
static int calls = 0;
int InternalCheckDeclareClass(const char *a,const char *b,void *p,void *q)
{ ++calls; assert(strcmp(a,b)==0); assert(p==q); return 0; }
''' + bridge + macros + '''
class Root { DECLARE_CLASS_NOBASE(Root); };
class Middle : public Root { DECLARE_CLASS(Middle,Root); };
class Leaf : public Middle { DECLARE_CLASS(Leaf,Middle); };
template<typename T> int CheckDeclareClass_Access(T *) { return T::CheckDeclareClass("Leaf"); }
int main() { assert(CheckDeclareClass_Access((Leaf*)0)==0); assert(calls==3); }
'''
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            unit = folder / 'debug_class.cpp'
            executable = folder / ('debug_class.exe' if os.name == 'nt' else 'debug_class')
            unit.write_text(source)
            result = subprocess.run([compiler, '-std=c++98', '-Wall', '-Wextra', '-pedantic',
                                     str(unit), '-o', str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
