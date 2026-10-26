"""Focused regression tests for built-in 2002 HUD and Mega Build content importer."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'modernize/integrate_beta_megabuild.py'
spec=importlib.util.spec_from_file_location('integrate_beta_megabuild',SCRIPT)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def vtf(width,height):
    head=bytearray(64)
    head[:4]=b'VTF\0'
    struct.pack_into('<III',head,4,7,1,64)
    struct.pack_into('<HH',head,16,width,height)
    return bytes(head)


class MegaBuildIntegrationTests(unittest.TestCase):
    def test_engine_mounts_source_content_without_argument(self):
        source=(ROOT/'engine/common.cpp').read_text(encoding='latin1')
        part=source.split('void COM_InitFilesystem (void)',1)[1].split('void COM_Log(',1)[0]
        self.assertIn('gammasource_content',part)
        self.assertIn('PATH_ADD_TO_HEAD',part)
        self.assertNotIn('CheckParm( "-gammasourcecontent" )',part)
        self.assertTrue((ROOT/'gammasource_content/materials/sprites/hud1.vtf').is_file())
        self.assertTrue((ROOT/'gammasource_content/scripts/mod_textures.txt').is_file())

    def test_rejects_archives_with_path_traversal(self):
        for unsafe in ['../hl2/materials/evil.vmt','C:/hl2/materials/a.vmt','hl2/materials/../../evil.vmt',
                       '/hl2/materials/evil.vmt']:
            self.assertIsNone(mod.game_path(unsafe))
        self.assertEqual(mod.game_path('Megabuild/hl2/materials/awesome.vtf'),'materials/awesome.vtf')
        self.assertIsNone(mod.game_path('Megabuild/e3_2003/materials/crossover.vtf'))
        self.assertIsNone(mod.game_path('HL2/bin/client.dll'))

    def test_megapatch_merges_texture_quality_and_preserves_hud(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            target=root/'gamma';target.mkdir()
            existing=target/'materials/street';existing.mkdir(parents=True)
            (existing/'brick.vtf').write_bytes(vtf(2048,2048))
            (target/'materials/sprites').mkdir(parents=True)
            (target/'materials/sprites/hud1.vtf').write_bytes(b'original-2002-hud')
            payload=root/'megapatch.zip'
            with zipfile.ZipFile(payload,'w') as z:
                z.writestr('megapatch/hl2/materials/street/brick.vtf',vtf(512,512))
                z.writestr('megapatch/hl2/materials/street/hires.vtf',vtf(4096,4096))
                z.writestr('megapatch/hl2/materials/sprites/hud1.vtf',b'wrong-mega-hud')
                z.writestr('megapatch/hl2/scripts/weapon_pistol.txt',b'very cool')
                z.writestr('megapatch/hl2/bin/engine.dll',b'evil')
                z.writestr('../hl2/materials/escape.vtf',b'evil')
            report=mod.integrate([payload],target)
            self.assertEqual((existing/'brick.vtf').read_bytes(),vtf(2048,2048))
            self.assertEqual((existing/'hires.vtf').read_bytes(),vtf(4096,4096))
            self.assertEqual((target/'materials/sprites/hud1.vtf').read_bytes(),b'original-2002-hud')
            self.assertEqual((target/'scripts/weapon_pistol.txt').read_bytes(),b'very cool')
            self.assertFalse((target/'bin/engine.dll').exists())
            self.assertFalse((root/'escape.vtf').exists())
            self.assertGreater(report['results']['kept_higher_resolution'],0)
            self.assertTrue(report['complete'])

    def test_rejected_new_model_keeps_existing_working_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            dest=root/'out';dest.mkdir()
            p=dest/'models/weapons';p.mkdir(parents=True)
            (p/'v_pistol.mdl').write_bytes(b'valid existing model')
            (p/'v_pistol.dx80.vtx').write_bytes(b'valid existing vtx')
            package=root/'mega.zip'
            with zipfile.ZipFile(package,'w') as z:
                z.writestr('hl2/models/weapons/v_pistol.mdl',b'IDST'+struct.pack('<i',44)+b'0'*500)
                z.writestr('hl2/models/weapons/v_pistol.dx80.vtx',b'unvalidated')
            rep=mod.integrate([package],dest)
            self.assertEqual((p/'v_pistol.mdl').read_bytes(),b'valid existing model')
            self.assertEqual((p/'v_pistol.dx80.vtx').read_bytes(),b'valid existing vtx')
            self.assertEqual(rep['results']['skipped_incompatible_model'],1)

    def test_folder_import_and_idempotence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            mega=root/'beta';content=mega/'hl2/materials/stuff';content.mkdir(parents=True)
            (content/'test.vtf').write_bytes(vtf(1024,1024))
            target=root/'out'
            first=mod.integrate([mega],target)
            second=mod.integrate([mega],target)
            self.assertEqual(first['results']['installed'],1)
            self.assertEqual(second['results']['unchanged'],1)
            self.assertEqual((target/'materials/stuff/test.vtf').read_bytes(),vtf(1024,1024))

    def test_converted_user_supplied_viewmodels(self):
        # Check actual user-provided models bundled into this source snapshot.
        p=ROOT/'gammasource_content/models/weapons'
        for name in ('v_crowbar','v_pistol','v_smg1'):
            mdl=(p/(name+'.mdl')).read_bytes()
            vtx=(p/(name+'.dx80.vtx')).read_bytes()
            self.assertEqual(mdl[:4],b'IDST')
            self.assertEqual(struct.unpack_from('<i',mdl,4)[0],36)
            result=mod.model37.check_vtx(vtx,mdl)
            self.assertGreater(result['vertices'],0)

    def test_unsupported_vtf_stays_out_of_legacy_engine(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            package=root/'newer.zip'
            newer=bytearray(vtf(4096,4096))
            struct.pack_into('<I',newer,8,4) # VTF7.4 (resource dictionary)
            with zipfile.ZipFile(package,'w') as z:
                z.writestr('hl2/materials/hires/newer.vtf',newer)
            result=mod.integrate([package],root/'overlay')
            self.assertEqual(result['results']['skipped_unsupported_vtf'],1)
            self.assertFalse((root/'overlay/materials/hires/newer.vtf').exists())

    def test_does_not_follow_preexisting_content_junction(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            destination=root/'overlay';destination.mkdir()
            protected=root/'original';protected.mkdir()
            link=destination/'materials'
            try:link.symlink_to(protected,target_is_directory=True)
            except (OSError,NotImplementedError):self.skipTest('symlinks are disabled')
            result=mod.staged_copy(destination,'materials/unsafe.txt',b'test')
            self.assertEqual(result,'skipped_symlink')
            self.assertFalse((protected/'unsafe.txt').exists())

if __name__=='__main__':unittest.main()
