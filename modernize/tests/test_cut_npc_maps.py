"""Validate the deterministic VMF generator and convex brush face planes."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'modernize'))
import generate_npc_testmaps as maps
import generate_multiplayer_testmap as mpmap


class NpcMapTests(unittest.TestCase):
    def test_all_brush_normals_face_outward(self):
        bounds = ((-100, -200, -300), (50, 60, 70))
        expected = {'x+': (1, 0, 0), 'x-': (-1, 0, 0),
                    'y+': (0, 1, 0), 'y-': (0, -1, 0),
                    'z+': (0, 0, 1), 'z-': (0, 0, -1)}
        for axis, norm in expected.items():
            actual = maps.normal(maps.face_vertices(bounds, axis))
            for i in range(3):
                self.assertGreater(actual[i] * norm[i], 0) if norm[i] else self.assertEqual(actual[i], 0)

    def test_no_disabled_implementation_in_generated_maps(self):
        candidates = json.loads((ROOT/'modernize/cut_npc_inventory.json').read_text())
        for item in candidates:
            path = maps.OUT / (item['name'] + '_test.vmf')
            self.assertEqual(path.exists(), item['prototype_candidate'])
            if item['prototype_candidate']:
                contents = path.read_text()
                self.assertEqual(contents.count('"classname" "'+item['name']+'"'), 1)
                self.assertEqual(contents.count('    solid\n'), 6)
                self.assertEqual(contents.count('        side\n'), 36)

    def test_deathmatch_arena_has_real_spawn_classes(self):
        mapfile = mpmap.OUT / 'mp_modern_test.vmf'
        content = mapfile.read_text()
        self.assertEqual(content.count('"classname" "info_player_deathmatch"'), 8)
        self.assertEqual(content.count('"classname" "npc_houndeye"'), 0)
        self.assertEqual(content.count('"classname" "weapon_pistol"'), 1)
        self.assertEqual(content.count('    solid\n'), 6)


if __name__=='__main__':
    unittest.main()
