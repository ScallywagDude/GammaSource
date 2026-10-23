"""Portable test fixtures for model scanning, no copyrighted asset payloads."""
import json, struct, sys, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
from scan_studio_v21 import inventory
class Scanner(unittest.TestCase):
    def test_branch_separation_and_checksums(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td); (p/'hl2').mkdir(); (p/'tf2').mkdir()
            b=bytearray(80);b[:4]=b'IDST';b[12:23]=b'player.mdl\0'
            for version,branch in [(36,'hl2'),(37,'tf2')]:
                struct.pack_into('<I',b,4,version)
                struct.pack_into('<I',b,8,123)
                struct.pack_into('<I',b,76,80)
                (p/branch/'player.mdl').write_bytes(b)
                vtx=bytearray(44)
                for off,v in [(0,6),(16,123),(20,1),(28,1),(32,36)]: struct.pack_into('<I',vtx,off,v)
                (p/branch/'player.dx80.vtx').write_bytes(vtx)
            rows=inventory([('hl2',p/'hl2'),('tf2',p/'tf2')])
            self.assertEqual(len(rows),2)
            self.assertEqual([r['branch'] for r in rows],['hl2','tf2'])
            self.assertTrue(all(r['status']=='2003_loader_candidate' for r in rows))
            self.assertFalse(rows[1]['runtime_studio_layout_verified'])
            vtx=p/'tf2'/'player.dx80.vtx';bad=bytearray(vtx.read_bytes())
            struct.pack_into('<I',bad,16,999);vtx.write_bytes(bad)
            rows=inventory([('tf2',p/'tf2')]);self.assertEqual(rows[0]['status'],'no_compatible_2003_vertex_file')
    def test_bad_mdl(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'a.mdl';p.write_bytes(b'BAD')
            self.assertEqual(inventory([('hl2',td)])[0]['status'],'invalid_mdl_header')
if __name__=='__main__': unittest.main()
