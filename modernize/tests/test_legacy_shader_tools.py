"""Shader commands retain their inputs and propagate interpreter/tool errors."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'modernize'))
import convert_dsp as converter
import run_shader_tool as runner


class LegacyShaderToolsTests(unittest.TestCase):
    def test_legacy_perl_command_only_changes_interpreter_routing(self):
        original = r'..\..\devtools\bin\perl ..\..\devtools\bin\fxc_prep.pl -dx9 -ps20a $(InputName).fxc'
        converted = converter.msbuild_custom(original)
        self.assertIn('run_shader_tool.py" --perl "$(HL2ModernPerl)"', converted)
        self.assertTrue(converted.endswith(r'..\..\devtools\bin\fxc_prep.pl -dx9 -ps20a %(Filename).fxc'))
        self.assertEqual(converter.msbuild_custom('echo perl script.pl'), 'echo perl script.pl')
        self.assertEqual(converter.msbuild_custom(r'fxctmp9\$(InputName).inc'), r'fxctmp9\%(Filename).inc')

    def test_explicit_interpreter_precedes_environment_and_path(self):
        good = subprocess.CompletedProcess([], 0, 'v5.42.3', '')
        with mock.patch.object(runner.shutil, 'which', return_value='selected/perl.exe') as which, \
             mock.patch.object(runner.subprocess, 'run', return_value=good) as run:
            self.assertEqual(runner.resolve_perl('explicit/perl.exe', {'HL2_MODERN_PERL':'other/perl.exe'}),
                             ('selected/perl.exe', 'v5.42.3'))
            which.assert_called_once_with('explicit/perl.exe')
            self.assertEqual(run.call_args.args[0], ['selected/perl.exe', '-e', 'print $^V'])

    def test_environment_override_and_missing_broken_interpreters(self):
        with mock.patch.object(runner.shutil, 'which', return_value=None) as which:
            with self.assertRaisesRegex(ValueError, 'not found'):
                runner.resolve_perl(environ={'HL2_MODERN_PERL':'unavailable/perl.exe'})
            which.assert_called_once_with('unavailable/perl.exe')
        for result in (subprocess.CompletedProcess([], 1, '', 'loader failed'),
                       subprocess.CompletedProcess([], 0, 'not Perl', '')):
            with mock.patch.object(runner.shutil, 'which', return_value='perl'), \
                 mock.patch.object(runner.subprocess, 'run', return_value=result):
                with self.assertRaisesRegex(ValueError, 'version probe'):
                    runner.resolve_perl(environ={})

    def test_script_arguments_and_failure_status_are_preserved(self):
        with tempfile.TemporaryDirectory() as td:
            script = Path(td)/'shader input.pl'
            script.write_text('fixture; not executed by the mocked process')
            with mock.patch.object(runner, 'resolve_perl', return_value=('portable/perl.exe','v5.42.3')), \
                 mock.patch.object(runner.subprocess, 'run', return_value=subprocess.CompletedProcess([], 7)) as run:
                self.assertEqual(runner.main([str(script), '-dx9', 'name with spaces.fxc']), 7)
                self.assertEqual(run.call_args.args[0], ['portable/perl.exe', str(script), '-dx9', 'name with spaces.fxc'])

    def test_generated_shader_projects_expose_override_without_legacy_perl(self):
        ns = {'p':converter.NS}
        for name in ('stdshader_dbg', 'stdshader_dx9'):
            tree = ET.parse(ROOT/'materialsystem/stdshaders'/f'{name}.vcxproj')
            properties = tree.findall('.//p:HL2ModernPerl', ns)
            self.assertEqual([node.text for node in properties], ['$(HL2_MODERN_PERL)', 'perl'])
            commands = [node.text for node in tree.findall('.//p:CustomBuild/p:Command', ns)]
            self.assertTrue(commands)
            self.assertTrue(all('run_shader_tool.py' in command for command in commands))
            self.assertTrue(all(r'devtools\bin\perl' not in command for command in commands))
            dependencies = [node.text for node in tree.findall('.//p:CustomBuild/p:AdditionalInputs', ns)]
            self.assertTrue(all('legacy_shader_tool.py' in value and 'run_shader_tool.py' in value for value in dependencies))


if __name__ == '__main__':
    unittest.main()
