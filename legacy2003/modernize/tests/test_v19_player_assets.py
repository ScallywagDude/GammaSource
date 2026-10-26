"""v19 player asset gates and preview controls: native and source integration."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

def source(path):
    return (ROOT/path).read_bytes().decode('latin1')

class V19PlayerAssets(unittest.TestCase):
    def test_server_refuses_unsupported_model_before_precache_or_spawn(self):
        code=source('dlls/hl2_dll/hl2_player.cpp')
        self.assertIn('HL2MODERN_PLAYER_ASSET_GUARD_V19', code)
        self.assertIn('if (HL2Modern_CompatiblePlayerModel(path)) engine->PrecacheModel(path);',code)
        spawn=code.split('void CHL2_Player::Spawn(void)',1)[1].split('BaseClass::Spawn();',1)[0]
        self.assertIn('HL2Modern_CompatiblePlayerModel(entry->path)',spawn)
        self.assertIn('HL2Modern_CompatiblePlayerModel("models/player.mdl")',spawn)
        self.assertLess(spawn.index('if (path)'),spawn.index('SetModel(path);'))
        self.assertIn('SetModelIndex(0);',spawn)
        self.assertNotIn('if (entry && filesystem->FileExists(entry->path))', spawn)

    def test_preview_controls_do_not_change_interface_abi(self):
        ui=source('gameui/PlayerModelPreviewPanel.cpp')
        header=source('gameui/PlayerModelPreviewPanel.h')
        self.assertIn('HL2MODERN_PREVIEW_CONTROLS_V19',ui)
        self.assertIn('m_Paused',header)
        self.assertIn('if (!m_Paused) m_AnimationTime += delta;',ui)
        self.assertIn('code == MOUSE_RIGHT',ui)
        self.assertIn('code == MOUSE_MIDDLE',ui)
        self.assertIn('OnMouseDoublePressed(MouseCode code)',ui)
        api=source('public/IGameUIModelPreview.h')
        self.assertIn('GameUIModelPreview001',api)
        self.assertNotIn('RenderWithZoom',api)

    def test_native_cpp98_header_rejection(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('native C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/('asset_check.exe' if os.name=='nt' else 'asset_check')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                                  str(ROOT/'modernize/tests/player_asset_guard_smoke.cpp'),
                                  '-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)

    def test_generator_replay_byte_identical(self):
        paths=('dlls/hl2_dll/hl2_player.cpp','gameui/PlayerModelPreviewPanel.cpp',
               'gameui/PlayerModelPreviewPanel.h')
        before={p:(ROOT/p).read_bytes() for p in paths}
        run=subprocess.run([sys.executable,str(ROOT/'modernize/patch_player_assets_v19.py')],
                           cwd=ROOT,capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        self.assertEqual(before,{p:(ROOT/p).read_bytes() for p in paths})

if __name__=='__main__': unittest.main()
