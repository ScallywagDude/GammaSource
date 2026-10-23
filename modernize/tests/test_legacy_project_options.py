"""Keep original per-file PCH exclusions and VC6 loop-scope semantics."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'modernize'))
import convert_dsp


class LegacyProjectOptions(unittest.TestCase):
    def test_launcher_preserves_explicit_dll_link_recipe(self):
        ns = {'m': convert_dsp.NS}
        tree = ET.parse(ROOT / 'launcher/launcher.vcxproj')
        kinds = tree.findall('.//m:ConfigurationType',ns)
        self.assertTrue(kinds)
        self.assertTrue(all(node.text == 'DynamicLibrary' for node in kinds))

    def test_legacy_archive_copy_requires_explicit_build_option(self):
        ns = {'m': convert_dsp.NS}
        for name in ('shaderlib', 'vtf'):
            tree = ET.parse(ROOT / name / (name + '.vcxproj'))
            commands = tree.findall('.//m:PostBuildEvent/m:Command', ns)
            self.assertTrue(commands)
            for command in commands:
                self.assertEqual(command.get('Condition'), "'$(HL2ModernStageLegacy)'=='true'")

    def test_projects_sharing_directory_keep_distinct_object_files(self):
        ns = {'m': convert_dsp.NS}
        destinations = []
        for name in ('stdshader_dx6', 'stdshader_dx7', 'stdshader_dx8', 'stdshader_dx9'):
            project = ROOT / 'materialsystem/stdshaders' / (name + '.vcxproj')
            group = next(g for g in ET.parse(project).findall('m:PropertyGroup', ns)
                         if "'Release|Win32'" in g.get('Condition', '') and g.find('m:IntDir', ns) is not None)
            expanded = group.findtext('m:IntDir', namespaces=ns).replace('$(ProjectDir)', str(project.parent))
            expanded = expanded.replace('$(Configuration)', 'Release').replace('$(MSBuildProjectName)', name)
            destinations.append(expanded.lower())
        self.assertEqual(len(set(destinations)), len(destinations), 'Shader projects overwrite each other\'s object/PDB files')

    def test_game_modules_keep_static_crt_and_foundation_imports(self):
        ns = {'m': convert_dsp.NS}
        for relative in ('dlls/hl.vcxproj', 'cl_dll/client.vcxproj'):
            for group in ET.parse(ROOT / relative).findall('.//m:ItemDefinitionGroup', ns):
                condition = group.get('Condition', '')
                mode = 'Debug' if "'Debug" in condition else 'Release'
                expected = 'MultiThreadedDebug' if mode == 'Debug' else 'MultiThreaded'
                self.assertEqual(group.findtext('m:ClCompile/m:RuntimeLibrary', namespaces=ns), expected)
                libraries = group.findtext('m:Link/m:AdditionalDependencies', namespaces=ns).split(';')
                for dependency in ('tier0', 'vstdlib'):
                    self.assertIn('..\\' + dependency + '\\build\\' + mode + '\\' + dependency + '.lib', libraries)
                if relative.startswith('cl_dll'):
                    self.assertIn('..\\vgui2\\controls\\build\\' + mode + '\\vgui_controls.lib', libraries)

    def test_shared_sources_retain_original_pch_subtractions(self):
        project = convert_dsp.parse(ROOT / 'dlls/hl.dsp')
        bitbuf = next(f for f in project['files'] if f['path'].lower().endswith('bitbuf.cpp'))
        self.assertIn('/Yu', bitbuf['settings']['*']['CPP_SUBTRACT'])
        ns = {'m': convert_dsp.NS}
        tree = ET.parse(ROOT / 'dlls/hl.vcxproj')
        element = next(e for e in tree.findall('.//m:ClCompile', ns)
                       if e.get('Include', '').lower().endswith('bitbuf.cpp'))
        settings = element.findall('m:PrecompiledHeader', ns)
        self.assertEqual(len(settings), len(project['configs']))
        self.assertTrue(all(s.text == 'NotUsing' for s in settings))

    def test_native_compiler_keeps_legacy_loop_declaration_alive(self):
        compiler = shutil.which('cl')
        if os.name != 'nt' or not compiler:
            self.skipTest('Native MSVC developer environment required')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'scope.cpp'
            source.write_text('int main() { for(int i=0;i<1;++i) {} return i==1?0:1; }\n')
            result = subprocess.run([compiler, '/nologo', '/Zc:forScope-', str(source),
                                     '/Fe:' + str(root / 'scope.exe'), '/Fo:' + str(root / 'scope.obj')],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(subprocess.run([str(root / 'scope.exe')]).returncode, 0)


if __name__ == '__main__':
    unittest.main()
