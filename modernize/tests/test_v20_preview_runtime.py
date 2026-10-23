"""Native v36 vertex-path fixtures and actual GameUI/shared-weapon integration."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT/rel).read_bytes().decode('latin1')

class V20PreviewAndWeaponAssets(unittest.TestCase):
    def test_native_companion_path_rules(self):
        c = shutil.which('g++') or shutil.which('clang++')
        if not c: self.skipTest('No host C++98 compiler')
        with tempfile.TemporaryDirectory() as td:
            exe = Path(td)/('path.exe' if os.name=='nt' else 'path')
            build = subprocess.run([c,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                                    str(ROOT/'modernize/tests/player_vertex_paths_smoke.cpp'),
                                    '-o',str(exe)], capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            test = subprocess.run([str(exe)], capture_output=True,text=True)
            self.assertEqual(test.returncode,0,test.stderr)

    def test_server_and_gameui_use_same_bounded_path_policy(self):
        server = read('dlls/hl2_dll/hl2_player.cpp')
        preview = read('engine/gameui_modelpreview.cpp')
        policy = read('modernize/hl2_player_asset_guard.h')
        self.assertIn('HL2MODERN_VERTEX_COMPANION_V20',policy)
        self.assertIn('HL2MODERN_SERVER_VERTEX_PREFLIGHT_V20',server)
        self.assertIn('HL2MODERN_PREVIEW_VERTEX_PREFLIGHT_V20',preview)
        self.assertIn('HL2ModernPlayerAssets::VertexCompanionPath(',server)
        self.assertIn('HL2ModernPlayerAssets::VertexCompanionPath(',preview)
        self.assertNotIn('Q_snprintf(vertexFile,',preview)
        self.assertNotIn('strcpy(vertexFile,',preview)
        for variant in ('.dx80.vtx','.dx7_3bone.vtx','.dx7_2bone.vtx'):
            self.assertIn(variant,policy)
        self.assertNotIn('.dx90.vtx',policy)

    def test_preview_restores_projection_view_model_and_fog(self):
        src = read('engine/gameui_modelpreview.cpp')
        self.assertIn('HL2MODERN_PREVIEW_MATRIX_SCOPE_V20',src)
        scope = src.split('class PreviewMatrixScope',1)[1].split('} scope(materials);',1)[0]
        for mode in ('MATERIAL_PROJECTION','MATERIAL_VIEW','MATERIAL_MODEL'):
            self.assertIn('MatrixMode('+mode+'); material->PushMatrix();',scope)
            self.assertIn('MatrixMode('+mode+'); material->PopMatrix();',scope)
        self.assertIn('material->FogMode(fog)',scope)
        self.assertNotIn('FogMode(oldFog)',src)
        self.assertLess(src.index('} scope(materials);'),src.index('materials->PerspectiveX('))

    def test_shared_impulse_weapon_needs_2003_vertex_geometry(self):
        shared = read('dlls/player.cpp')
        self.assertIn('HL2MODERN_CHEAT_VERTEX_PREFLIGHT_V20',shared)
        helper = shared.split('static bool HL2Modern_CheatStudioModel',1)[1].split('CBaseCombatWeapon *CBasePlayer::GiveCheatWeapon',1)[0]
        self.assertIn('VertexCompanionPath(header, bytes,',helper)
        self.assertIn('filesystem->FileExists(vertexPath,"GAME")',helper)
        self.assertIn('return false;',helper)
        self.assertIn('HL2Modern_CheatStudioModel(info->szViewModel)',shared)
        self.assertIn('HL2Modern_CheatStudioModel(info->szWorldModel)',shared)

    def test_patch_replay_is_idempotent(self):
        items=['modernize/hl2_player_asset_guard.h','dlls/hl2_dll/hl2_player.cpp',
               'engine/gameui_modelpreview.cpp','dlls/player.cpp']
        before={p:(ROOT/p).read_bytes() for p in items}
        result = subprocess.run([sys.executable,str(ROOT/'modernize/patch_preview_runtime_v20.py')],
                                cwd=ROOT,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(before,{p:(ROOT/p).read_bytes() for p in items})

if __name__ == '__main__': unittest.main()
