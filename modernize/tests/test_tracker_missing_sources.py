"""Regression coverage for Tracker missing dependencies and legacy VGUI names."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODERN = ROOT / 'modernize'

class TrackerMissingSources(unittest.TestCase):
    def test_legacy_wrapper_headers_forward_existing_real_headers(self):
        wrappers = list((MODERN / 'tracker_vgui_compat').glob('VGUI_*.h'))
        self.assertGreaterEqual(len(wrappers), 40)
        for f in wrappers:
            s = f.read_text()
            includes = re.findall(r'#include <([^>]+)>', s)
            self.assertEqual(len(includes), 1)
            self.assertTrue((ROOT / 'public' / includes[0]).exists(), str(f))
            self.assertNotIn('class ', s)

    def test_projects_include_compat_directory(self):
        for f in (ROOT / 'Tracker').rglob('*.vcxproj'):
            s = f.read_text()
            self.assertIn('modernize\\tracker_vgui_compat', s, str(f))

    def test_originally_missing_compile_inputs_present(self):
        for f in ('shared/TokenLine.cpp', 'shared/TokenLine.h',
                  'common/Info.cpp', 'common/Info.h',
                  'common/AdminServer/IAdminServer.h'):
            self.assertTrue((ROOT / f).is_file(), f)

    @unittest.skipUnless(shutil.which('g++'), 'requires C++ compiler for portable behavioral test')
    def test_tokenizer_and_info_parser_cpp98_behavior(self):
        source = r'''
#include "TokenLine.h"
#include "Info.h"
#include <assert.h>
#include <string.h>
#include <string>
int main() {
  TokenLine t("1 23434 : 20.000 min\nignored");
  assert(t.CountToken() == 5);
  assert(strcmp(t.GetToken(1), "23434") == 0);
  assert(strcmp(t.GetToken(5), "") == 0);
  t.SetLine("log L 07/01/2002 - 16:22:32: \"Player<1><id><1>\" say \"hello world\"");
  assert(t.CountToken() == 8);
  assert(strcmp(t.GetToken(5), "Player<1><id><1>") == 0);
  assert(strcmp(t.GetRestOfLine(7), "\"hello world\"") == 0);
  char *p = t.GetToken(5); p[0] = 'X'; assert(p[0] == 'X');
  assert(strcmp(Info_ValueForKey("\\hostname\\Example Server\\players\\5", "players"), "5") == 0);
  assert(strcmp(Info_ValueForKey("\\hostname\\Example Server\\players\\5", "hostname"), "Example Server") == 0);
  assert(strcmp(Info_ValueForKey("\\bad", "bad"), "") == 0);
  std::string oversized(900, 'x');
  std::string msg = "\\long\\" + oversized;
  assert(strlen(Info_ValueForKey(msg.c_str(), "long")) == 511);
  assert(strcmp(Info_ValueForKey("\\a\\1\\b\\2", "missing"), "") == 0);
}
'''
        with tempfile.TemporaryDirectory() as td:
            sourcefile = Path(td) / 'test.cpp'
            outfile = Path(td) / 'test'
            sourcefile.write_text(source)
            subprocess.run(['g++', '-std=c++98', '-Wall', '-Wextra', '-Werror', '-pedantic',
                            '-I' + str(ROOT / 'shared'), '-I' + str(ROOT / 'common'),
                            str(sourcefile), str(ROOT / 'shared/TokenLine.cpp'),
                            str(ROOT / 'common/Info.cpp'), '-o', str(outfile)],
                           check=True, capture_output=True)
            subprocess.run([str(outfile)], check=True, capture_output=True)

if __name__ == '__main__':
    unittest.main()
