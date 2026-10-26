"""The model inventory only labels v7 conversion eligibility, not gameplay compatibility."""
import struct, tempfile, unittest
from pathlib import Path
from modernize.scan_studio_v21 import inventory
from modernize.tests.test_vtx_convert_v22 import fixture

class V22Scanner(unittest.TestCase):
    def test_v7_conversion_candidate(self):
        vtx,mdl=fixture()
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)
            (p/'human.mdl').write_bytes(mdl)
            (p/'human.dx80.vtx').write_bytes(vtx)
            row=inventory([('hl2',p)])[0]
            self.assertTrue(row['has_v7_conversion_candidate'])
            self.assertEqual(row['status'],'no_compatible_2003_vertex_file')
            self.assertEqual(row['vertex_companions'][0]['status'],'v7_offline_conversion_candidate')
            self.assertFalse(row['runtime_studio_layout_verified'])
            # The output must be revalidated as v6 before the model is usable.
            from modernize.convert_vtx7_to_v6_v22 import convert_v7_to_v6
            (p/'human.dx80.vtx').write_bytes(convert_v7_to_v6(vtx,mdl))
            row=inventory([('hl2',p)])[0]
            self.assertEqual(row['status'],'2003_loader_candidate')
    def test_foreign_checksum_not_candidate(self):
        vtx,mdl=fixture(); changed=bytearray(vtx);struct.pack_into('<I',changed,16,1)
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)
            (p/'human.mdl').write_bytes(mdl)
            (p/'human.dx80.vtx').write_bytes(changed)
            self.assertFalse(inventory([('hl2',p)])[0]['has_v7_conversion_candidate'])
if __name__=='__main__':unittest.main()
