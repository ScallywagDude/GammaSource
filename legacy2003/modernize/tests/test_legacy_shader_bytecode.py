"""Check real DX9 compiler/assembler output and legacy DWORD packing."""
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'modernize'))
import legacy_shader_tool as tool
import patch_legacy_shader_tools as patch


def header_bytes(text):
    return b''.join(struct.pack('<I', int(word,16)) for word in re.findall(r'0x([0-9a-f]{8})',text))


class LegacyShaderBytecodeTests(unittest.TestCase):
    def test_dword_header_roundtrips_exact_bytes_and_rejects_other_formats(self):
        bytecode = struct.pack('<4I', 0xffff0200, 0x00000001, 0x800f0000, 0x0000ffff)
        header = tool.shader_header(bytecode)
        self.assertIn('const DWORD g_shader_main[]', header)
        declaration, opening, *_ = header.splitlines()
        self.assertTrue(declaration.endswith(' ='))
        self.assertEqual(opening,'{')
        self.assertEqual(header_bytes(header), bytecode)
        self.assertTrue(tool.shader_header(bytecode,'dwVertexShader',False).startswith('DWORD dwVertexShader[]'))
        self.assertTrue(tool.shader_header(bytecode,'dwPixelShader',False).startswith('DWORD dwPixelShader[]'))
        for invalid in (b'', b'DXBC'+b'\0'*20, bytecode+b'\0', bytecode[:-4]+b'\0'*4):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    tool.shader_header(invalid)

    def test_overlay_reapplication_preserves_shader_script_bytes(self):
        paths = ('devtools/bin/fxc_prep.pl','devtools/bin/vsh_prep.pl','devtools/bin/psh_prep.pl',
                 'materialsystem/stdshaders/common_vs_fxc.h')
        for relative in paths:
            source = (ROOT/relative).read_bytes().decode('latin1').replace('\r\n','\n')
            self.assertEqual(patch.restore(relative, source), source, relative)
        gamma = (ROOT/paths[-1]).read_text()
        self.assertIn('return pow( linearValue, cOOGamma );', gamma)
        self.assertIn('tmp = a * linearValue + b;', gamma)

    def test_register_pressure_retry_preserves_profile_and_full_precision(self):
        with tempfile.TemporaryDirectory() as td:
            source = Path(td)/'shader.hlsl';source.write_text('fixture')
            header = Path(td)/'shader.h'
            calls = []
            def compile_once(command, **kwargs):
                calls.append(command)
                if len(calls)==1:
                    return subprocess.CompletedProcess(command,1,'','error X4505: maximum temp register index exceeded')
                output = Path(next(arg[3:] for arg in command if arg.startswith('/Fo')))
                output.write_bytes(struct.pack('<2I',0xfffe0200,0xffff))
                return subprocess.CompletedProcess(command,0,'','')
            with mock.patch.object(tool, 'resolve_tool', return_value='fxc'), \
                 mock.patch.object(tool.subprocess, 'run', side_effect=compile_once):
                self.assertEqual(tool.main(['compile','/Tvs_2_0','/Fh'+str(header),str(source)]),0)
            self.assertEqual(len(calls),2)
            self.assertIn('/Gis',calls[1])
            self.assertNotIn('/Gpp',calls[1])
            self.assertIn('/Tvs_2_0',calls[1])
            self.assertEqual(struct.unpack_from('<I',header_bytes(header.read_text()))[0],0xfffe0200)

    def test_other_compiler_errors_are_not_retried(self):
        with tempfile.TemporaryDirectory() as td:
            source = Path(td)/'shader.hlsl';source.write_text('invalid fixture')
            header = Path(td)/'shader.h'
            failed = subprocess.CompletedProcess([],3,'','error X3000: syntax error')
            with mock.patch.object(tool, 'resolve_tool', return_value='fxc'), \
                 mock.patch.object(tool.subprocess, 'run', return_value=failed) as run:
                self.assertEqual(tool.main(['compile','/Tvs_2_0','/Fh'+str(header),str(source)]),3)
                self.assertEqual(run.call_count,1)
                self.assertFalse(header.exists())

    def test_explicit_legacy_compiler_only_handles_register_overflow(self):
        with tempfile.TemporaryDirectory() as td:
            source = Path(td)/'shader.hlsl';source.write_text('fixture')
            header = Path(td)/'shader.h'
            bytecode = struct.pack('<2I',0xfffe0200,0xffff)
            arguments = ['/nologo','/DLIGHT_COMBO=16','/Tvs_2_0','/Emain']
            for diagnostic, expected, fallback_calls in (
                ('error X4505: maximum temp register index exceeded',0,1),
                ('error X3000: syntax error',3,0),
                ('error X4505: maximum temp register index exceeded\nerror X3000: syntax error',3,0),
            ):
                with self.subTest(diagnostic=diagnostic), \
                     mock.patch.dict(os.environ, {'HL2_MODERN_D3DX9':'selected/d3dx9_31.dll'}), \
                     mock.patch.object(tool, 'resolve_tool', return_value='fxc'), \
                     mock.patch.object(tool.subprocess, 'run', return_value=subprocess.CompletedProcess([],3,'',diagnostic)), \
                     mock.patch.object(tool, 'compile_legacy_d3dx', return_value=bytecode) as legacy:
                    self.assertEqual(tool.main(['compile',*arguments,'/Fh'+str(header),str(source)]), expected)
                    self.assertEqual(legacy.call_count,fallback_calls)
                    if fallback_calls:
                        legacy.assert_called_once_with(source,arguments,'selected/d3dx9_31.dll')
                        self.assertEqual(header_bytes(header.read_text()),bytecode)

    @unittest.skipUnless(os.name=='nt' and os.environ.get('HL2_MODERN_D3DX9'), 'Optional legacy DirectX compiler not selected')
    def test_real_legacy_compiler_preserves_vs20_for_register_limited_teeth(self):
        source = ROOT/'materialsystem/stdshaders/teeth_vs20.fxc'
        bytecode = tool.compile_legacy_d3dx(source, ['/nologo','/Emain','/Tvs_2_0',
            '/DLIGHT_COMBO=16','/DFOG_TYPE=0','/DNUM_BONES=0','/DBUMPMAP=1'],os.environ['HL2_MODERN_D3DX9'])
        self.assertEqual(struct.unpack_from('<I',bytecode)[0],0xfffe0200)
        self.assertEqual(header_bytes(tool.shader_header(bytecode)),bytecode)
        with self.assertRaisesRegex(ValueError,'cannot preserve option'):
            tool.compile_legacy_d3dx(source,['/Tvs_2_0','/Gpp'],os.environ['HL2_MODERN_D3DX9'])

    @unittest.skipUnless(os.name=='nt', 'Windows SDK assembly test')
    def test_real_vertex_and_pixel_assembly_keep_original_shader_models(self):
        try:
            tool.compiler_dll()
        except ValueError as exc:
            self.skipTest(str(exc))
        with tempfile.TemporaryDirectory() as td:
            for profile, body, version in (
                ('vs_1_1', 'dcl_position v0\nmov oPos, v0\n', 0xfffe0101),
                ('ps_1_1', '#if COLOR\nmov r0, v0\n#else\nmov r0, v1\n#endif\n', 0xffff0101),
            ):
                with self.subTest(profile=profile):
                    source = Path(td)/'shader.asm'
                    source.write_text(profile+'\n'+body)
                    bytecode = tool.assemble(source, ['COLOR=1'])
                    self.assertEqual(struct.unpack_from('<I', bytecode)[0], version)
                    self.assertEqual(struct.unpack_from('<I', bytecode,len(bytecode)-4)[0], 0xffff)
                    self.assertEqual(header_bytes(tool.shader_header(bytecode)), bytecode)

    @unittest.skipUnless(os.name=='nt', 'Windows SDK FXC test')
    def test_real_fxc_retains_ps20_and_writes_dwords(self):
        if not (os.environ.get('HL2_MODERN_FXC') or tool.resolve_tool('fxc')):
            self.skipTest('Windows SDK FXC unavailable')
        with tempfile.TemporaryDirectory() as td:
            source = Path(td)/'shader.hlsl'
            header = Path(td)/'shader.h'
            source.write_text('float4 main(float4 color:COLOR0):COLOR0 { return color; }')
            self.assertEqual(tool.main(['compile','/nologo','/Tps_2_0','/Emain','/Fh'+str(header),str(source)]),0)
            data = header_bytes(header.read_text())
            self.assertEqual(struct.unpack_from('<I',data)[0],0xffff0200)
            self.assertEqual(struct.unpack_from('<I',data,len(data)-4)[0],0xffff)


if __name__=='__main__':
    unittest.main()
