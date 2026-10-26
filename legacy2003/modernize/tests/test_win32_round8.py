"""Targeted regression checks for Win32 round-8 build errors."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[2]


class Round8Tests(unittest.TestCase):
    def test_serverbrowser_panelmap_visible_to_subclasses(self):
        code = (ROOT/'Tracker/ServerBrowser/BaseGamesPage.h').read_bytes().decode('latin1')
        self.assertRegex(code, r'protected:\s*// Derived panel maps require access[^\n]*\n\s*DECLARE_PANELMAP\(\)')

    def test_font_library_linked_once_per_configuration(self):
        doc = ET.parse(ROOT/'vguimatsurface/vguimatsurface.vcxproj')
        for dep in doc.findall('.//{*}AdditionalDependencies'):
            libs = [(p.strip().lower().replace('/', '\\')) for p in (dep.text or '').split(';')]
            self.assertEqual(sum('vgui_surfacelib.lib' in item for item in libs), 1)
            self.assertNotIn('vgui_surfacelib.lib', libs)

    def test_converter_preserves_single_font_archive(self):
        src = (ROOT/'modernize/convert_dsp.py').read_text()
        self.assertIn("p['path'].stem.casefold()=='vguimatsurface'", src)

    def test_legacy_stream_headers_replaced(self):
        paths = list((ROOT/'worldcraft').rglob('*.h'))+list((ROOT/'worldcraft').rglob('*.cpp'))
        paths.extend([ROOT/'public/tokenreader.h', ROOT/'public/tokenreader.cpp'])
        for path in paths:
            code=path.read_bytes().decode('latin1')
            self.assertNotIn('fstream.h', code, str(path))
            self.assertNotIn('ios::nocreate', code, str(path))
        code=(ROOT/'worldcraft/keyvalues.h').read_bytes().decode('latin1')
        self.assertNotIn('class fstream;',code)

    def test_header_compiles_under_cpp98(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('No C++ compiler for portable smoke test')
        code='''#include "public/legacy_fstream_compat.h"\nint main(){ ifstream in; ofstream out; fstream both; ios::openmode flags=ios::in|ios::binary; (void)flags; return both.is_open(); }\n'''
        with tempfile.TemporaryDirectory() as dirname:
            target=pathlib.Path(dirname)/'test.cpp'
            target.write_text(code)
            result=subprocess.run([compiler,'-std=c++98','-I'+str(ROOT), '-fsyntax-only',str(target)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_unsafe_seh_requires_explicit_opt_in(self):
        p=ET.parse(ROOT/'Directory.Build.targets').getroot()
        groups=p.findall('.//{*}ItemDefinitionGroup')
        self.assertEqual(len(groups),1)
        self.assertIn("'$(HL2ModernLegacyUnsafeSEH)'=='true'",groups[0].get('Condition'))
        self.assertEqual(groups[0].findtext('.//{*}ImageHasSafeExceptionHandlers'),'false')

    def test_perl_fails_fast_before_hl2_build(self):
        code=(ROOT/'modernize/build_windows.cmd').read_text()
        self.assertLess(code.index('run_shader_tool.py --probe'),code.index('call build_hl2_vs2026.cmd'))

    def test_repair_repeatable(self):
        command=['python',str(ROOT/'modernize/repair_win32_round8.py')]
        p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertIn('0 files changed',p.stdout)

if __name__=='__main__':unittest.main()
