from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_modern_shaders as shaders


class ShaderToolDiscovery(unittest.TestCase):
    def test_explicit_directory_precedes_path(self):
        with tempfile.TemporaryDirectory() as directory:
            tool=Path(directory)/'fxc.exe'
            tool.write_bytes(b'fixture - resolution only, never executed')
            with mock.patch.object(shaders.shutil,'which',return_value='unrelated/fxc.exe'):
                self.assertEqual(shaders.resolve_tool('fxc',[directory]),str(tool.resolve()))

    def test_sdk_fallback_and_missing_tool(self):
        with tempfile.TemporaryDirectory() as directory:
            tool=Path(directory)/'dxc.exe'
            tool.write_bytes(b'fixture - resolution only, never executed')
            with mock.patch.object(shaders.shutil,'which',return_value=None),mock.patch.object(shaders,'sdk_tool_dirs',return_value=[Path(directory)]):
                self.assertEqual(shaders.resolve_tool('dxc'),str(tool.resolve()))
                self.assertIsNone(shaders.resolve_tool('glslangValidator'))

    def test_hlsl_stages_with_installed_compilers(self):
        available={tool:shaders.resolve_tool(tool) for tool in ('fxc','dxc')}
        if not any(available.values()):self.skipTest('Optional HLSL compilers unavailable')
        with tempfile.TemporaryDirectory() as directory:
            for name,tool,args in shaders.jobs(Path(directory)):
                if tool not in available or not available[tool]:continue
                with self.subTest(stage=name):
                    result=subprocess.run([available[tool]]+args,capture_output=True,text=True,errors='replace',timeout=60)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    flag='/Fo' if '/Fo' in args else '-Fo'
                    output=Path(args[args.index(flag)+1])
                    self.assertGreater(output.stat().st_size,4)
                    self.assertEqual(output.read_bytes()[:4],b'DXBC')


if __name__=='__main__':unittest.main()
