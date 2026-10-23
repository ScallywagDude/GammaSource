"""Regression tests for structural conversion, run on Linux/macOS/Windows."""
import pathlib
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
from convert_dsp import get_flaginfo, link_libraries, parse, msbuild_custom
import convert_dsp as converter

N={'p':'http://schemas.microsoft.com/developer/msbuild/2003'}

class ConverterTests(unittest.TestCase):
    def test_project_and_filter_identity_is_path_flavor_independent(self):
        posix_root=pathlib.PurePosixPath('/source')
        windows_root=pathlib.PureWindowsPath('C:/source')
        posix=posix_root/'dlls/hl.dsp'
        windows=windows_root/'dlls/hl.dsp'
        self.assertEqual(converter.relative_project_name(posix,posix_root),'dlls/hl.dsp')
        self.assertEqual(converter.relative_project_name(windows,windows_root),'dlls/hl.dsp')
        self.assertEqual(converter.project_guid(posix,posix_root),converter.project_guid(windows,windows_root))
        self.assertEqual(converter.filter_guid(posix,'Source Files',posix_root),'{820A9FFF-D6EC-5318-8A6C-2A2FC109233B}')
        self.assertEqual(converter.filter_guid(posix,'Source Files',posix_root),converter.filter_guid(windows,'Source Files',windows_root))
        paths=[windows_root/'appframework/appframework.dsp',windows_root/'Tracker/AdminServer/AdminServer.dsp']
        self.assertEqual([converter.relative_project_name(p,windows_root) for p in sorted(paths,key=lambda p:converter.relative_project_name(p,windows_root))],
                         ['Tracker/AdminServer/AdminServer.dsp','appframework/appframework.dsp'])

    def test_solution_and_batch_newlines_are_exact_bytes(self):
        with tempfile.TemporaryDirectory() as td:
            root=pathlib.Path(td)
            file=root/'plain.txt'
            converter.write_crlf(file,'one\ntwo\r\nthree\rfour\n')
            self.assertEqual(file.read_bytes(),b'one\r\ntwo\r\nthree\r\nfour\r\n')
            project={'path':root/'branch/test.dsp','name':'sample','guid':'{TEST}','configs':{'Debug':{},'Release':{}}}
            with mock.patch.object(converter,'ROOT',root):
                converter.generate_sln(root/'sample.sln',[project])
                converter.build_cmd([{'dsp':'branch/test.dsp','type':'Debug','configuration':'Debug'}],[project],{})
            solution=(root/'sample.sln').read_bytes()
            self.assertIn(b'sample (branch/test.dsp)',solution)
            self.assertIn(b'"branch\\test.vcxproj"',solution)
            for data in (solution,(root/'build_hl2_vs2026.cmd').read_bytes()):
                self.assertNotIn(b'\r\r\n',data)
                self.assertNotIn(b'\n',data.replace(b'\r\n',b''))
                self.assertTrue(data.endswith(b'\r\n'))

    def test_original_mastertest_postbuild_is_preserved_without_execution(self):
        original=ROOT/'master/mastertest/mastertest.dsp'
        project=converter.parse(original)
        expected='if not exist "d:\\quiver" mkdir "d:\\quiver"\r\ncopy /Y "$(TargetPath)" "d:\\quiver\\mastertest.exe"'
        for config in ('Debug','Release'):
            self.assertEqual(project['configs'][config]['postcopy'],['d:\\quiver\\mastertest.exe'])
            self.assertEqual(converter.staging_command(original.parent,project['configs'][config]['postcopy'][0]),expected)
        # A native Windows destination must have identical metadata when
        # parsed from either LF or CRLF VC6 input on the current host.
        with tempfile.TemporaryDirectory() as td:
            directory=pathlib.Path(td)
            lf=directory/'test_lf.dsp';crlf=directory/'test_crlf.dsp'
            text=original.read_bytes().decode('latin1').replace('\r\n','\n')
            lf.write_bytes(text.encode('latin1'));crlf.write_bytes(text.replace('\n','\r\n').encode('latin1'))
            self.assertEqual(converter.parse(lf)['configs'],converter.parse(crlf)['configs'])
        # Relative staging outside the source tree is still not expanded.
        self.assertIsNone(converter.staging_command(ROOT,'../outside.dll'))
        self.assertIsNone(converter.staging_command(ROOT,'d:drive_relative.dll'))

    def test_linker_options_are_not_libraries(self):
        flags=(r'user32.lib "..\old stuff\value.lib" '
               r'/nodefaultlib:"LIBCD" /implib:"Debug\server.lib" '
               r'/out:"build\unused.lib" /pdb:"Debug\mod.pdb"')
        self.assertEqual(link_libraries(flags),['user32.lib',r'..\old stuff\value.lib'])
        self.assertEqual(get_flaginfo(flags)['libraries'],['user32.lib',r'..\old stuff\value.lib'])

    def test_shader_input_macros_convert(self):
        self.assertEqual(msbuild_custom(r'fxctmp9\$(InputName).inc'),r'fxctmp9\%(Filename).inc')
        self.assertEqual(msbuild_custom(r'echo $(SOURCE)'),r'echo %(FullPath)')

    def test_shader_project_has_custom_build_rules(self):
        r=ET.parse(ROOT/'materialsystem/stdshaders/stdshader_dx9.vcxproj')
        items=r.findall('.//p:CustomBuild',N)
        self.assertGreaterEqual(len(items),15)
        hsv=next(e for e in items if e.get('Include','').lower().endswith('hsv_ps20.fxc'))
        cmds=[e.text for e in hsv.findall('p:Command',N)]
        outs=[e.text for e in hsv.findall('p:Outputs',N)]
        self.assertEqual(len(cmds),2)
        self.assertTrue(all('fxc_prep.pl' in c for c in cmds))
        self.assertTrue(all('%(Filename)' in o for o in outs))

    def test_all_vcxproj_xml_parse(self):
        projects=list(ROOT.rglob('*.vcxproj'))
        self.assertEqual(len(projects),153)
        for f in projects:
            with self.subTest(project=str(f.relative_to(ROOT))): ET.parse(f)

    def test_source_patches_and_inputs(self):
        npc=(ROOT/'dlls/ai_concommands.cpp').read_bytes().decode('latin-1')
        shader=(ROOT/'materialsystem/cmaterialsystem.cpp').read_bytes().decode('latin-1')
        self.assertEqual(npc.count('HL2MODERN_NPC_SPAWN_SAFETY'),1)
        self.assertEqual(npc.count('pCreated ? pCreated->MyNPCPointer() : NULL'),2)
        self.assertIn('HL2MODERN_RENDER_BACKEND_SELECTION',shader)
        self.assertIn('shaderapivulkan',shader)
        self.assertIn('shaderapidx12',shader)

if __name__=='__main__': unittest.main()
