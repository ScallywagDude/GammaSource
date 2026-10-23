import shutil
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import enable_tf2_prototype_weapon_sources as m
from weapon_build_audit import compiled_sources, ROOT,windows_path
class PrototypeCompileTests(unittest.TestCase):
    def test_opt_in_tf2_only(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'hl.vcxproj'
            shutil.copyfile(ROOT/'dlls/hl.vcxproj',p)
            # The optional tool only needs a project whose relative path is meaningful.
            # Supply a path under dlls through the worktree for source refs, but
            # test a copy and compare the XML item names/config exclusions.
            try:
                added=m.enable(p)
            except ValueError:
                # If copied project XML uses an unusual layout fail with that error.
                raise
            self.assertEqual(len(added),6)
            raw=p.read_text()
            self.assertIn('weapon_mortar.cpp',raw)
            self.assertIn('Debug HL2|Win32',raw)
            self.assertEqual(m.enable(p),[])
            debug_tf=compiled_sources(p,'Debug TF2')
            debug_hl2=compiled_sources(p,'Debug HL2')
            self.assertTrue(any(x.endswith('/weapon_mortar.cpp') for x in debug_tf))
            self.assertFalse(any(x.endswith('/weapon_mortar.cpp') for x in debug_hl2))
            self.assertTrue(all(x in raw for x in ('weapon_infiltrator.cpp','weapon_laserrifle.cpp','weapon_pistols.cpp')))
    def test_source_paths_remain_valid(self):
        report=m.audit(ROOT)['branches']['tf2']
        for weapon in report['weapons']:
            if not weapon['server_compiled_all_configs']:
                self.assertTrue((ROOT/weapon['source']).is_file(),weapon['classname'])
if __name__=='__main__':unittest.main()
