"""Synthetic authorized asset tree tests; no proprietary model or art bundled."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('weapon_asset_audit',ROOT/'modernize/weapon_asset_audit.py')
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class WeaponAssetAuditTests(unittest.TestCase):
    def test_nested_keyvalues_comments_and_escapes(self):
        kv='''// harmless comment\n"WeaponData" {"viewmodel" "models/weapons/v_test.mdl" /* another */
            "playermodel" "models/weapons/w_test.mdl" "TextureData" {"weapon" {"file" "vgui/hud/test" "x" "2" "y" "3" "width" "18" "height" "21"}}}'''
        x=audit.kv_parse(kv)
        self.assertEqual(x['viewmodel'],'models/weapons/v_test.mdl')
        self.assertEqual(x['texturedata']['weapon']['width'],'18')
        self.assertRaises(ValueError,audit.kv_parse,'"WeaponData" { "viewmodel" "abc"')
    def test_reject_traversal_and_absolute_paths(self):
        for path in ('../steal','materials/../../bad','C:/bad','/tmp/bad','\\\\evil','foo//bar','foo/./bar'):
            self.assertIsNone(audit.clean_asset_path(path,'materials','.vmt'),path)
    def test_hud_texture_dimensions_and_branch_isolation(self):
        with tempfile.TemporaryDirectory() as td:
            folder=Path(td);hl2=folder/'hl2';tf=folder/'tf2'
            (hl2/'scripts').mkdir(parents=True)
            (hl2/'models/weapons').mkdir(parents=True)
            (hl2/'materials/vgui/hud').mkdir(parents=True)
            tf.mkdir()
            (hl2/'scripts/weapon_test.txt').write_text('''"WeaponData" {"viewmodel" "models/weapons/v_test.mdl" "playermodel" "models/weapons/w_test.mdl" "TextureData" {"weapon" {"file" "vgui/hud/test" "x" "8" "y" "4" "width" "32" "height" "16" }}}''')
            for model in ('v_test.mdl','w_test.mdl'):
                (hl2/'models/weapons'/model).write_bytes(b'dummy model')
            (hl2/'materials/vgui/hud/test.vmt').write_text('''"UnlitGeneric" {"$basetexture" "vgui/hud/test"}''')
            (hl2/'materials/vgui/hud/test.vtf').write_bytes(b'VTF\x00'+b'\x07\x00\x00\x00'+b'\x02\x00\x00\x00'+b'\x50\x00\x00\x00'+b'\x40\x00\x20\x00')
            registry={b:[] for b in audit.BRANCHES}
            for b in ('hl2','tf2'):
                registry[b]=[{'classname':'weapon_test','implementation_file':'test.cpp'}]
            report=audit.audit({'hl2':hl2,'tf2':tf},registry)
            weapon=report['branches']['hl2']['weapons'][0]
            self.assertEqual(weapon['status'],'assets_present_unverified_runtime')
            self.assertEqual(weapon['hud_icons'][0]['texture_dimensions'],[64,32])
            self.assertEqual(report['branches']['tf2']['weapons'][0]['status'],'missing_weapon_script')
            (hl2/'scripts/weapon_test.txt').write_text('''"WeaponData" {"viewmodel" "models/weapons/v_test.mdl" "playermodel" "models/weapons/w_test.mdl" "TextureData" {"weapon" {"file" "vgui/hud/test" "x" "33" "y" "4" "width" "32" "height" "16" }}}''')
            report=audit.audit({'hl2':hl2},registry)
            self.assertEqual(report['branches']['hl2']['weapons'][0]['hud_icons'][0]['status'],'outside_texture_bounds')
    def test_unprovided_assets_never_marked_missing(self):
        registry={b:[] for b in audit.BRANCHES}
        registry['hl2']=[{'classname':'weapon_test','implementation_file':'example.cpp'}]
        report=audit.audit({},registry)
        self.assertEqual(report['branches']['hl2']['weapons'][0]['status'],'unverified_no_asset_directory')
    def test_source_hardening_and_idempotence(self):
        h=ROOT/'cl_dll/hud.cpp';w=ROOT/'game_shared/weapon_parse.cpp'
        self.assertIn('HL2MODERN_HUD_SPRITE_BOUNDS',h.read_bytes().decode('latin1'))
        self.assertIn('HL2MODERN_WEAPON_SLOT_BOUNDS',w.read_bytes().decode('latin1'))
        before=(h.read_bytes(),w.read_bytes())
        p=subprocess.run([sys.executable,str(ROOT/'modernize/patch_weapon_asset_safety.py')],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(before,(h.read_bytes(),w.read_bytes()))
        apply=(ROOT/'modernize/apply_modernization.py').read_text()
        self.assertIn('patch_weapon_asset_safety.py',apply)
        self.assertIn('weapon_asset_audit.py',apply)
    def test_report_branch_counts(self):
        report=json.loads((ROOT/'modernize/weapon_asset_report.json').read_text())
        self.assertEqual(set(report['branches']),set(audit.BRANCHES))
        for b in audit.BRANCHES:
            self.assertEqual(report['branches'][b]['summary']['issues'],0)
if __name__=='__main__':unittest.main()
