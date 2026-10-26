"""Preview integration contracts; GPU rendering still requires a runtime test."""
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
def source(path):
    return (ROOT / path).read_bytes().decode('latin1')

class AnimatedPreviewIntegration(unittest.TestCase):
    def test_optional_interface_matches_provider_and_consumer(self):
        header = source('public/IGameUIModelPreview.h')
        self.assertIn('"GameUIModelPreview001"', header)
        self.assertIn('EXPOSE_SINGLE_INTERFACE(CGameUIModelPreview, IGameUIModelPreview, GAMEUI_MODEL_PREVIEW_INTERFACE_VERSION)', source('engine/gameui_modelpreview.cpp'))
        self.assertIn('engineFactory(GAMEUI_MODEL_PREVIEW_INTERFACE_VERSION,NULL)', source('gameui/GameUI_Interface.cpp'))
        self.assertNotIn('GameUIModelPreview', source('public/IGameUIFuncs.h'))

    def test_projects_include_bones_and_exclude_dedicated_renderer(self):
        project = ET.parse(ROOT / 'engine/engine.vcxproj')
        for path in ('.\\gameui_modelpreview.cpp', '..\\game_shared\\bone_setup.cpp'):
            nodes = project.findall(f'.//m:ClCompile[@Include="{path}"]', NS)
            self.assertEqual(len(nodes), 1)
            exclusions = nodes[0].findall('m:ExcludedFromBuild', NS)
            self.assertEqual(len(exclusions), 2)
            self.assertTrue(all(e.text == 'true' and 'Dedicated ' in e.get('Condition') for e in exclusions))
            self.assertIn('SOURCE=' + path, source('engine/engine.dsp'))
        gameui = ET.parse(ROOT / 'gameui/GameUI.vcxproj')
        self.assertEqual(len(gameui.findall('.//m:ClCompile[@Include=".\\PlayerModelPreviewPanel.cpp"]', NS)), 1)
        self.assertIn('SOURCE=.\\PlayerModelPreviewPanel.cpp', source('gameui/GameUI.dsp'))

    def test_new_renderer_uses_actual_selected_model_and_animation(self):
        engine = source('engine/gameui_modelpreview.cpp')
        self.assertIn('GetModelForName(m_ModelPath,', engine)
        self.assertIn('CalcPose(header, NULL,', engine)
        self.assertIn('Studio_BuildMatrices(header,', engine)
        self.assertIn('info.m_pHardwareData = &model->studio.hardwareData', engine)
        self.assertIn('ForcedMaterialOverride(NULL)', engine)
        self.assertIn('g_pStudioRender->DrawModel(info, origin', engine)
        gui = source('gameui/PlayerModelPreviewPanel.cpp')
        self.assertLess(gui.index('Begin3DPaint('), gui.index('gameuimodelpreview->Render('))
        self.assertGreater(gui.index('End3DPaint('), gui.index('gameuimodelpreview->Render('))
        self.assertIn('input()->SetMouseCapture(GetVPanel())', gui)
        self.assertIn('now - m_LastInteractionTime > 2.0', gui)

    def test_branch_and_installation_gates_are_preserved(self):
        menu = source('gameui/OptionsSubMultiplayer.cpp')
        self.assertIn('HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory())', menu)
        self.assertIn('if (!filesystem()->FileExists(entry.path)) continue;', menu)
        self.assertIn('!gameuimodelpreview->IsModelAvailable(entry.path)', menu)
        self.assertIn('m_pModelList->ApplyChanges()', menu)
        self.assertRegex(source('engine/cl_main.cpp'), r'ConVar\s+cl_model\([^\n]*FCVAR_ARCHIVE\s*\|\s*FCVAR_USERINFO')
        engine = source('engine/gameui_modelpreview.cpp')
        self.assertIn('header.version != STUDIO_VERSION', engine)
        self.assertIn('header.numbones > MAXSTUDIOBONES', engine)
        for suffix in ('.dx80.vtx', '.dx7_3bone.vtx', '.dx7_2bone.vtx'):
            self.assertIn('"' + suffix + '"', engine)

    def test_preview_reference_cannot_release_server_or_client_model(self):
        loader = source('engine/modelloader.h')
        self.assertIn('FMODELLOADER_GAMEUI = (1<<6)', loader)
        mask = re.search(r'FMODELLOADER_REFERENCEMASK\s*=\s*\(([^)]+)\)', loader).group(1)
        self.assertIn('FMODELLOADER_GAMEUI', mask)
        engine = source('engine/gameui_modelpreview.cpp')
        self.assertIn('ReleaseAllModels(IModelLoader::FMODELLOADER_GAMEUI)', engine)
        self.assertNotIn('ReleaseAllModels(IModelLoader::FMODELLOADER_CLIENT)', engine)
        self.assertIn('gameuimodelpreview->ClearModel()', source('gameui/GameUI_Interface.cpp'))

if __name__ == '__main__':
    unittest.main()
