"""Independent synthetic asset fixtures only; no original game artwork."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import weapon_asset_audit_v23 as v23

class WeaponAssetV23Tests(unittest.TestCase):
    def test_nested_sprite_selection(self):
        data={'640':{'file':'vgui/gun','width':'12','height':'8'},'320':{'file':'vgui/low'}}
        self.assertEqual(v23.select_sprite(data)[1],'640')
        data['file']='vgui/direct'
        self.assertEqual(v23.select_sprite(data)[1],'direct')
        self.assertEqual(v23.select_sprite({'font':'Tahoma'})[1],'font_or_missing')

    def test_directory_layer_resolution_and_branch_isolation(self):
        with tempfile.TemporaryDirectory() as dir:
            base=Path(dir);override=base/'overlay';game=base/'hl2';foreign=base/'tf2'
            for d in (override,game,foreign):d.mkdir()
            (game/'scripts').mkdir(); (foreign/'scripts').mkdir()
            (game/'scripts/weapon_test.txt').write_text('"WeaponData" { "viewmodel" "models/missing.mdl" "playermodel" "models/missing.mdl" }')
            (foreign/'scripts/weapon_test.txt').write_text('"WeaponData" { "viewmodel" "models/tf.mdl" "playermodel" "models/tf.mdl" }')
            registry={b:[] for b in v23.BRANCHES}
            registry['hl2']=[{'classname':'weapon_test','implementation_file':'test.cpp'}]
            registry['tf2']=[{'classname':'weapon_test','implementation_file':'test.cpp'}]
            report=v23.audit({'hl2':[override,game],'tf2':[foreign]},registry)
            a=report['branches']['hl2']['weapons'][0]
            b=report['branches']['tf2']['weapons'][0]
            self.assertEqual(a['script_layer'],1)
            self.assertEqual(a['viewmodel']['path'],'models/missing.mdl')
            self.assertEqual(b['viewmodel']['path'],'models/tf.mdl')
            (override/'scripts').mkdir()
            (override/'scripts/weapon_test.txt').write_text('"WeaponData" { "viewmodel" "models/override.mdl" }')
            report=v23.audit({'hl2':[override,game]},registry)
            self.assertEqual(report['branches']['hl2']['weapons'][0]['script_layer'],0)

    def test_legacy_hud_640_rectangle_and_material_layer(self):
        with tempfile.TemporaryDirectory() as td:
            game=Path(td); (game/'materials/hud').mkdir(parents=True)
            (game/'materials/hud/icon.vmt').write_text('"UnlitGeneric" { "$basetexture" "hud/icon" }')
            header=bytearray(20);header[:4]=b'VTF\0';struct.pack_into('<HH',header,16,64,32)
            (game/'materials/hud/icon.vtf').write_bytes(header)
            icon={'640':{'file':'hud/icon','x':'8','y':'4','width':'40','height':'22'}}
            x=v23.check_icon([game],'weapon',icon)
            self.assertEqual(x['status'],'rectangle_verified');self.assertEqual(x['layout'],'640')
            icon['640']['width']='60'
            self.assertEqual(v23.check_icon([game],'weapon',icon)['status'],'out_of_bounds')
            icon['640']['width']='15';icon['640']['file']='../escape'
            self.assertEqual(v23.check_icon([game],'weapon',icon)['status'],'invalid_material_path')

    def test_patch_material_inheritance_and_overrides(self):
        with tempfile.TemporaryDirectory() as td:
            game=Path(td)
            (game/'materials/hud').mkdir(parents=True)
            header=bytearray(20);header[:4]=b'VTF\0'
            struct.pack_into('<HH',header,16,64,32)
            (game/'materials/hud/icon.vtf').write_bytes(header)
            (game/'materials/hud/alt.vtf').write_bytes(header)
            (game/'materials/hud/base.vmt').write_text('"UnlitGeneric" { "$basetexture" "hud/icon" }')
            (game/'materials/hud/patch.vmt').write_text('"Patch" { "include" "hud/base" }')
            icon={'file':'hud/patch','x':1,'y':2,'width':16,'height':8}
            result=v23.check_icon([game],'weapon',icon)
            self.assertEqual(result['status'],'rectangle_verified')
            self.assertEqual(result['texture'],'materials/hud/icon.vtf')
            (game/'materials/hud/patch.vmt').write_text('"Patch" { "include" "hud/base" "replace" { "$basetexture" "hud/alt" } }')
            self.assertEqual(v23.check_icon([game],'weapon',icon)['texture'],'materials/hud/alt.vtf')
            (game/'materials/hud/patch.vmt').write_text('"Patch" { "include" "hud/base" "insert" { "$basetexture" "hud/alt" } }')
            self.assertEqual(v23.check_icon([game],'weapon',icon)['texture'],'materials/hud/icon.vtf')

    def test_patch_material_cycles_and_invalid_includes(self):
        with tempfile.TemporaryDirectory() as td:
            game=Path(td);(game/'materials/hud').mkdir(parents=True)
            (game/'materials/hud/a.vmt').write_text('"Patch" { "include" "hud/b" }')
            (game/'materials/hud/b.vmt').write_text('"Patch" { "include" "hud/a" }')
            icon={'file':'hud/a','width':8,'height':8}
            self.assertEqual(v23.check_icon([game],'weapon',icon)['status'],'cyclic_or_deep_material')
            (game/'materials/hud/a.vmt').write_text('"Patch" { "include" "../secret" }')
            self.assertEqual(v23.check_icon([game],'weapon',icon)['status'],'invalid_material_include')
            (game/'materials/hud/a.vmt').write_text('"Patch" { "include" "hud/missing" }')
            self.assertEqual(v23.check_icon([game],'weapon',icon)['status'],'missing_material')

    def test_nested_patch_across_asset_layers(self):
        with tempfile.TemporaryDirectory() as td:
            top=Path(td)/'overlay';base=Path(td)/'game';foreign=Path(td)/'tf2'
            for path in (top,base,foreign):
                (path/'materials/hud').mkdir(parents=True)
            (top/'materials/hud/icon.vmt').write_text('"Patch" { "include" "hud/outer" "replace" { "$basetexture" "hud/new" } }')
            (base/'materials/hud/outer.vmt').write_text('"Patch" { "include" "hud/root" "insert" { "$basetexture" "hud/unused" } }')
            (base/'materials/hud/root.vmt').write_text('"UnlitGeneric" { "$basetexture" "hud/old" }')
            (top/'materials/hud/new.vtf').write_bytes(self._vtf_header(64,64))
            (foreign/'materials/hud/new.vtf').write_bytes(self._vtf_header(64,64))
            icon={'file':'hud/icon','x':4,'y':4,'width':16,'height':16}
            record=v23.check_icon([top,base],'weapon',icon)
            self.assertEqual(record['status'],'rectangle_verified')
            self.assertEqual(record['texture'],'materials/hud/new.vtf')
            self.assertEqual([x['layer'] for x in record['material_chain']],[0,1,1])
            self.assertEqual(record['texture_layer'],0)
            (top/'materials/hud/new.vtf').unlink()
            self.assertEqual(v23.check_icon([top,base],'weapon',icon)['status'],'missing_texture')

    def test_nested_patch_depth_bound(self):
        with tempfile.TemporaryDirectory() as td:
            base=Path(td)
            (base/'materials/chain').mkdir(parents=True)
            for i in range(14):
                (base/f'materials/chain/p{i}.vmt').write_text(f'"Patch" {{ "include" "chain/p{i+1}" }}')
            icon={'file':'chain/p0','width':1,'height':1}
            self.assertEqual(v23.check_icon([base],'weapon',icon)['status'],'cyclic_or_deep_material')

    @staticmethod
    def _vtf_header(width,height):
        hdr=bytearray(20)
        hdr[:4]=b'VTF\0'
        struct.pack_into('<HH',hdr,16,width,height)
        return hdr

    def test_model_versions_and_nonexistent_assets(self):
        with tempfile.TemporaryDirectory() as td:
            game=Path(td); (game/'models').mkdir()
            self.assertEqual(v23.check_model([game],'models/missing.mdl')['status'],'missing_model')
            file=game/'models/weapon.mdl'
            file.write_bytes(b'garbage' * 40)
            self.assertEqual(v23.check_model([game],'models/weapon.mdl')['status'],'invalid_mdl_header')
            data=bytearray(80);data[:4]=b'IDST';struct.pack_into('<III',data,4,37,1234,0);data[12:19]=b'gun.mdl';struct.pack_into('<I',data,76,80)
            file.write_bytes(data)
            self.assertEqual(v23.check_model([game],'models/weapon.mdl')['status'],'no_compatible_2003_vertex_file')

    def test_no_assets_report_is_not_missing(self):
        registry={b:[] for b in v23.BRANCHES}
        registry['cstrike']=[{'classname':'weapon_test','implementation_file':'test.cpp'}]
        row=v23.audit({},registry)['branches']['cstrike']['weapons'][0]
        self.assertEqual(row['status'],'unverified_no_assets')

if __name__=='__main__':unittest.main()
