"""Regression checks for legacy Tracker VGUI accessor namespace ordering."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHIM = ROOT / 'modernize' / 'tracker_vgui_compat' / 'VGUI_Controls.h'
SCRIPT = ROOT / 'modernize' / 'repair_tracker_vgui_namespace_round7.py'
CALLS = re.compile(r'\b(?:vgui::)?(?:system|ivgui|surface|filesystem|scheme|input|ipanel|localize)\s*\(\s*\)')
INC = re.compile(r'(?m)^\s*#\s*include\s*[<\"]([^>\"\r\n]+)[>\"]')


class TrackerVGuiNamespaceTests(unittest.TestCase):
    def test_shim_sets_namespace_before_controls(self):
        s = SHIM.read_text()
        self.assertLess(s.index('<vgui/VGUI.h>'), s.index('<vgui_controls/Controls.h>'))
        self.assertLess(s.index('<vgui/KeyCode.h>'), s.index('<vgui_controls/Controls.h>'))
        self.assertLess(s.index('<vgui/MouseCode.h>'), s.index('<vgui_controls/Controls.h>'))

    def test_accessor_callers_force_macro_before_other_headers(self):
        callers = 0
        for p in (ROOT / 'Tracker').rglob('*.cpp'):
            s = p.read_bytes().decode('latin1')
            if not CALLS.search(s):
                continue
            callers += 1
            includes = INC.findall(s)
            with self.subTest(source=p.relative_to(ROOT).as_posix()):
                self.assertTrue(includes)
                self.assertEqual(includes[0].lower(), 'vgui/vgui.h')
        self.assertGreater(callers, 40)

    def test_real_headers_compile_in_legacy_order(self):
        clang = shutil.which('clang++')
        if not clang:
            self.skipTest('Clang C++ not available')
        # Stub only unrelated platform/runtime dependencies to compile the
        # real VGUI.h, VGUI_Controls.h and Controls.h declaration order.
        program = '''#define INTERFACE_H
#define VSTDLIB_STRTOOLS_H
#define _WCHAR_T_DEFINED
typedef void* (*CreateInterfaceFn)(const char *, int *);
#include <VGUI_Controls.h>
#include <vgui/VGUI.h>
int main() {
  return vgui::system() == 0 || vgui::ivgui() == 0 ||
         vgui::surface() == 0 || vgui::filesystem() == 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'check.cpp'
            p.write_text(program)
            proc = subprocess.run([clang, '-std=c++98', '-fsyntax-only',
                                   '-Wno-unknown-pragmas', '-Wno-macro-redefined',
                                   '-I' + str(ROOT / 'modernize/tracker_vgui_compat'),
                                   '-I' + str(ROOT / 'public'), str(p)],
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_repair_idempotent(self):
        proc = subprocess.run([sys.executable, str(SCRIPT)], cwd=ROOT,
                              capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('changed: 0', proc.stdout)

    def test_pipeline_wires_repair_after_previous_tracker_repairs(self):
        s = (ROOT / 'modernize/apply_modernization.py').read_text()
        self.assertIn("'repair_tracker_vgui_namespace_round7.py'", s)
        self.assertLess(s.index("'repair_tracker_win32_accessors_round6.py'"),
                        s.index("'repair_tracker_vgui_namespace_round7.py'"))


if __name__ == '__main__':
    unittest.main()
