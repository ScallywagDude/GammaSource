"""Guarded v37-path integration checks: do not equate version acceptance with rendering."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]
def source(path): return (ROOT/path).read_bytes().decode('latin1')
class StudioCompatibilityV21(unittest.TestCase):
    def test_portable_model_and_vertex_headers(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('host C++98 compiler unavailable')
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/('sidecar.exe' if os.name=='nt' else 'sidecar')
            proc=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                str(ROOT/'modernize/tests/studio_sidecar_v21_smoke.cpp'),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(proc.returncode,0,proc.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
    def test_engine_studio_validator_is_not_claimed_to_compile_on_unix(self):
        # Studio's 2003 platform headers depend on MSVC/Win32; Windows build
        # integration and genuine v37 sample models remain separately untested.
        self.assertIn('class StudioDataValidator',source('modernize/model_preview_studio_validation.h'))
    def test_original_converter_never_relabels_unchecked_v37(self):
        source_h=source('public/studio.h')
        self.assertIn('version >= 35 && version <= STUDIO_VERSION',source_h)
        load=source('engine/l_studio.cpp')
        self.assertIn('HL2MODERN_STUDIO_LOADER_V37_V21',load)
        self.assertLess(load.index('StudioDataValidator('),load.index('phdr->version = STUDIO_VERSION;'))
        self.assertIn('memcmp(buffer,diskBytes.Base(),diskLength)',load)
        self.assertIn('if (!compatible)',load)
    def test_game_ui_and_multiplayer_share_full_layout_validation(self):
        for p in ('engine/gameui_modelpreview.cpp','dlls/hl2_dll/hl2_player.cpp','dlls/player.cpp'):
            s=source(p)
            self.assertIn('StudioDataValidator(',s)
            self.assertIn('.Validate(true)',s)
            self.assertIn('VertexCompanionPath(',s)
        self.assertIn('header.version != 37',source('engine/gameui_modelpreview.cpp'))
    def test_does_not_pretend_new_renderer_vertex_formats_work(self):
        source_h=source('modernize/hl2_player_asset_guard.h')
        self.assertNotIn('.dx90.vtx',source_h)
        self.assertIn('.dx80.vtx',source_h)
    def test_patch_generator_can_replay(self):
        watched=['public/studio.h','engine/l_studio.cpp','engine/gameui_modelpreview.cpp',
          'dlls/hl2_dll/hl2_player.cpp','dlls/player.cpp','modernize/model_preview_studio_validation.h']
        before={p:(ROOT/p).read_bytes() for p in watched}
        result=subprocess.run([shutil.which('python3') or 'python3',str(ROOT/'modernize/patch_studio_v37_v21.py')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(before,{p:(ROOT/p).read_bytes() for p in watched})
if __name__=='__main__':unittest.main()
