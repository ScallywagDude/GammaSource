from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
def source(p):return (ROOT/p).read_text(encoding='latin1')
class Integration(unittest.TestCase):
    def test_v37_preflight_is_required_before_relabel(self):
        x=source('engine/l_studio.cpp')
        self.assertIn('LegacyVertexFile(',x)
        self.assertLess(x.index('LegacyVertexFile('),x.index('phdr->version = STUDIO_VERSION'))
        self.assertIn('if (!compatible)',x)
    def test_same_check_is_used_for_preview_players_and_weapons(self):
        for path in ['engine/gameui_modelpreview.cpp','dlls/hl2_dll/hl2_player.cpp','dlls/player.cpp']:
            x=source(path)
            self.assertIn('LegacyVertexFile(',x,path)
            self.assertIn('!v37 ||',x) if path != 'engine/gameui_modelpreview.cpp' else self.assertIn('header.version!=37 ||',x)
    def test_only_v6_legacy_vtx_is_accepted(self):
        x=source('modernize/studio_sidecar_v21.h')
        self.assertIn('U32(vtx)!=6',x)
        self.assertIn('U32(vtx+16)!=U32(mdl+8)',x)
if __name__=='__main__': unittest.main()
