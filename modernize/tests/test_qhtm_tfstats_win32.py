"""Portable smoke/structural checks for QHTM and TFStats Win32 fixes.

A real Visual Studio x86 build remains necessary for Windows SDK, CRT, PCH,
and library dependencies. These tests target the C++98 source regressions.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
QHTM = ROOT / 'vgui2' / 'QHTM'


class CompilerRepairs(unittest.TestCase):
    def test_qhtm_windows_typedefs_are_not_shadowed(self):
        text = (QHTM / 'DebugHlp.h').read_text(encoding='latin1')
        self.assertNotIn('typedef const unsigned short * LPCWSTR;', text)
        self.assertNotIn('typedef const char * LPCSTR;', text)

    def test_template_dependent_types_and_copy_behavior(self):
        self.assertIn('typename CMap<Key, Value>::BucketEntry', (QHTM / 'Map.inl').read_text())
        self.assertIn('typename CMap<Key, Value>::BucketEntry', (QHTM / 'MapIter.h').read_text())
        self.assertIn('typename std::map<T,time_t>::iterator', (ROOT / 'utils/tfstats/timeindexedlist.h').read_text())
        self.assertIn('inline CSize( const CSize& size )', (QHTM / 'WinHelper.h').read_text())

    def test_qhtm_include_directories_all_configurations(self):
        ns = {'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
        projects = list(QHTM.rglob('*.vcxproj'))
        self.assertGreaterEqual(len(projects), 9)
        for project in projects:
            tree = ET.parse(str(project))
            for group in tree.findall('.//m:ItemDefinitionGroup',ns):
                cl = group.find('m:ClCompile',ns)
                if cl is None: continue
                inc = cl.find('m:AdditionalIncludeDirectories',ns)
                self.assertIsNotNone(inc, project)
                self.assertIn('$(ProjectDir)..\\QHTM\\', inc.text, str(project))

    def test_scripts_repeat_without_changes(self):
        scripts = ('repair_qhtm_win32.py', 'repair_tfstats_templates.py',
                   'repair_tracker_controls_accessors.py')
        for name in scripts:
            run = subprocess.run([sys.executable, str(ROOT / 'modernize' / name)],
                                 cwd=str(ROOT), text=True, capture_output=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertTrue(any(s in run.stdout for s in
                                ('0 files modified','already patched', 'added: 0')), run.stdout)

    @unittest.skipUnless(shutil.which('g++'), 'Requires GNU C++ for portable C++98 tests')
    def test_qhtm_template_runtime_cxx98(self):
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            (t/'windows.h').write_text('''#ifndef _WINDOWS_
#define _WINDOWS_
#include <stddef.h>
#include <ctype.h>
#define ASSERT(x) ((void)0)
typedef unsigned int UINT;
typedef unsigned int DWORD;
typedef const char *LPCTSTR;
#endif
''')
            (t/'tchar.h').write_text('#include <strings.h>\n#define _tcsicmp strcasecmp\n')
            (t/'map.h').write_text('#include "'+(QHTM/'Map.h').as_posix()+'"\n')
            (t/'MapIter.Inl').write_text('#include "'+(QHTM/'MapIter.inl').as_posix()+'"\n')
            (t/'main.cpp').write_text('''#include "Map.h"
#include "MapIter.h"
int main() {
    Container::CMap<int,int> m(5);
    m.SetAt(1,8); m.SetAt(2,9);
    if (!m.Lookup(1) || *m.Lookup(1) != 8) return 1;
    Container::CMapIter<int,int> it(m);
    int count=0;
    while (!it.EOL()) { ++count; it.Next(); }
    if (count!=2) return 2;
    m.RemoveAt(1);
    return m.Lookup(1) == NULL ? 0 : 3;
}
''')
            cmd=['g++','-std=c++98','-fpermissive', '-I'+str(t),'-I'+str(QHTM),
                 str(t/'main.cpp'),'-o',str(t/'main')]
            run=subprocess.run(cmd,text=True,capture_output=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stderr)
            run=subprocess.run([str(t/'main')],capture_output=True,timeout=5)
            self.assertEqual(run.returncode,0)

    @unittest.skipUnless(shutil.which('g++'), 'Requires GNU C++ for portable C++98 tests')
    def test_tfstats_template_runtime_cxx98(self):
        with tempfile.TemporaryDirectory() as td:
            t=Path(td)
            text=(ROOT/'utils/tfstats/timeindexedlist.h').read_text(encoding='latin1')
            text=text.replace('#include "TFStatsApplication.h"',
                              '#include <ctime>\nclass AppStub { public: void fatalError(const char *); }; extern AppStub *g_pApp;')
            text=text.replace('#include "util.h"','')
            (t/'ti.h').write_text(text)
            (t/'main.cpp').write_text('''#include "ti.h"
AppStub *g_pApp=0;
int main() { CTimeIndexedList<int> t; t.add(1,5); t.cut(2); return t.atTime(1,0)==5 ? 0 : 1; }
''')
            run=subprocess.run(['g++','-std=c++98','-I'+str(t),str(t/'main.cpp'),'-o',str(t/'main')],
                               capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stderr)
            run=subprocess.run([str(t/'main')],capture_output=True,timeout=5)
            self.assertEqual(run.returncode,0)


if __name__=='__main__':
    unittest.main()
