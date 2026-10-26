"""Content/parser, search-path and repair tests using newly generated fixtures."""
import hashlib
import importlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import asset_validation_v18 as audit
import asset_case_patch_v18 as repair
import package_release_v18 as package
import apply_release_overlay as overlay
import compose_asset_repairs_v18 as compose


def texture(width=64,height=32):
    data=bytearray(80+width*height*4)
    data[:4]=b'VTF\0'
    struct.pack_into('<IIIHHI',data,4,7,1,80,width,height,0x2000)
    struct.pack_into('<i',data,52,0)
    data[56]=1
    return bytes(data)


class ContentValidation(unittest.TestCase):
    def test_composed_repairs_preserve_both_edits_and_reject_conflicts(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/'assets'
            (root/'materials').mkdir(parents=True)
            path=root/'materials/test.vmt'
            original=b'"UnlitGeneric" { "$basetexture" "wrong" '
            path.write_bytes(original)
            def entry(edit):
                return {'branch':'hl2','root_index':0,'path':'materials/test.vmt',
                    'original_sha256':repair.sha(original),'edits':[edit]}
            case=entry({'offset':31,'before':'wrong','after':'Correct'})
            syntax=entry({'offset':len(original),'before':'','after':'}'})
            # Derive the token offset from the fixture rather than its formatting.
            case['edits'][0]['offset']=original.index(b'wrong')
            result,diff=compose.compose([{'files':[case]},{'files':[syntax]}],{'hl2':[root]})
            target=Path(td)/'composed'
            repair.apply_plan(result,{'hl2':[root]},target)
            self.assertEqual((target/'hl2/root0/materials/test.vmt').read_bytes(),original.replace(b'wrong',b'Correct')+b'}')
            conflict=entry({'offset':case['edits'][0]['offset'],'before':'wrong','after':'Other'})
            self.assertRaises(ValueError,compose.compose,[{'files':[case]},{'files':[conflict]}],{'hl2':[root]})

    def test_exact_casing_and_root_priority_never_mix_branches(self):
        with tempfile.TemporaryDirectory() as td:
            base=Path(td)
            first=base/'first';second=base/'second';tf=base/'tf'
            (first/'models/Weapons').mkdir(parents=True)
            (second/'models/weapons').mkdir(parents=True)
            tf.mkdir()
            (first/'models/Weapons/Example.mdl').write_bytes(b'first')
            (second/'models/weapons/example.mdl').write_bytes(b'second')
            index=audit.AssetIndex([first,second])
            result=index.resolve('models/weapons/example.mdl')
            self.assertEqual(result['status'],'case_mismatch')
            self.assertEqual(result['root_index'],0)
            self.assertEqual(index.disk_path(result).read_bytes(),b'first')
            self.assertEqual(audit.AssetIndex([tf]).resolve('models/weapons/example.mdl')['status'],'missing')
            self.assertEqual(audit.AssetIndex([]).resolve('models/weapons/example.mdl')['status'],'unverified_no_asset_directory')

    def test_invalid_paths(self):
        for path in ('../secret','/root/x','C:/x','materials/../x','a//b','a/./b','a\x00b','//server/x'):
            self.assertIsNone(audit.relative_asset(path),path)

    def test_legacy_hud_resolution_is_selected_without_changing_direct_paths(self):
        node,resolution=audit.resolve_hud_spec({'640':{'file':'crosshairs','x':'0'},'320':{'file':'other'}})
        self.assertEqual(node['file'],'sprites/crosshairs')
        self.assertEqual(resolution,'640')
        direct={'file':'root_material','640':{'file':'crosshairs'}}
        self.assertEqual(audit.resolve_hud_spec(direct),(direct,None))
        fallback,resolution=audit.resolve_hud_spec({'320':{'file':'Sprites/crosshairs'}})
        self.assertEqual(fallback['file'],'Sprites/crosshairs')
        self.assertEqual(resolution,'320')

    def test_texture_headers_and_actual_hud_bounds(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/'materials/hud').mkdir(parents=True)
            (root/'materials/hud/new.vmt').write_text('"UnlitGeneric" { "$basetexture" "hud/new" }')
            (root/'materials/hud/new.vtf').write_bytes(texture())
            branch=audit.BranchAudit([root])
            good=branch.hud('weapon',{'file':'hud/new','x':'8','y':'2','width':'32','height':'16'})
            self.assertEqual(good['status'],'verified_rectangle')
            bad=branch.hud('weapon',{'file':'hud/new','x':'48','y':'2','width':'32','height':'16'})
            self.assertEqual(bad['status'],'outside_texture_bounds')
            self.assertFalse(good['artwork_match_and_visual_scale_verified'])
            # Cached decoded content must never hide a later reference's wrong case.
            self.assertEqual(branch.material('HUD/New')['status'],'case_mismatch')
            self.assertEqual(branch.material('hud/new')['status'],'verified_file')
            self.assertEqual(branch.texture('HUD/New')['status'],'case_mismatch')
            self.assertEqual(branch.texture('hud/new')['status'],'verified_file')
            (root/'materials/hud/short.vtf').write_bytes(b'VTF\0'+b'\0'*16)
            self.assertEqual(audit.inspect_vtf(root/'materials/hud/short.vtf')['status'],'invalid_vtf_header')

    def test_duplicate_random_waves_all_checked_and_material_include_cycle(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/'scripts').mkdir()
            (root/'sound/fixture').mkdir(parents=True)
            (root/'materials').mkdir()
            (root/'scripts/game_sounds_fixture.txt').write_text('"Weapon.Test" { "rndwave" { "wave" "fixture/one.wav" "wave" "fixture/two.wav" } }')
            (root/'scripts/sounds.txt').write_text('"Player.PickupWeapon" { "wave" "fixture/one.wav" }')
            (root/'scripts/game_sounds_manifest.txt').write_text('"game_sounds_manifest" { "precache_file" "scripts/game_sounds_fixture.txt" "precache_file" "scripts/sounds.txt" }')
            (root/'sound/fixture/one.wav').write_bytes(b'fixture')
            (root/'materials/a.vmt').write_text('"Patch" { "include" "b.vmt" }')
            (root/'materials/b.vmt').write_text('"Patch" { "include" "a.vmt" }')
            branch=audit.BranchAudit([root])
            event=branch.sound('Weapon.Test')
            self.assertEqual(len(event['waves']),2)
            self.assertEqual(event['status'],'manifest_resolved_event')
            self.assertEqual(branch.sound('Player.PickupWeapon')['status'],'manifest_resolved_event')
            self.assertEqual({x['status'] for x in event['waves']},{'verified_file','missing'})
            branch.material('a')
            self.assertEqual(branch.materials['materials/b.vmt']['includes'][0]['status'],'cyclic_material_include')
            self.assertEqual(branch.texture('env_cubemap')['status'],'runtime_generated_texture')

    def test_case_plan_hashes_and_apply_does_not_overwrite_original(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/'content'
            (root/'scripts').mkdir(parents=True)
            (root/'models/Weapons').mkdir(parents=True)
            (root/'models/Weapons/New.mdl').write_bytes(b'no copied content')
            script=root/'scripts/weapon_new.txt'
            script.write_bytes(b'"WeaponData" { "viewmodel" "models/weapons/new.mdl" }\r\n')
            original=script.read_bytes()
            files,patches=repair.prepare('hl2',[root])
            self.assertEqual(len(files),1)
            self.assertIn('models/Weapons/New.mdl',''.join(patches))
            target=Path(td)/'overlay'
            count=repair.apply_plan({'files':files},{'hl2':[root]},target)
            self.assertEqual(count,1)
            self.assertEqual(script.read_bytes(),original)
            fixed=target/'hl2/root0/scripts/weapon_new.txt'
            self.assertIn(b'models/Weapons/New.mdl',fixed.read_bytes())
            verified=repair.verify_output({'files':files},{'hl2':[root]},target)
            self.assertEqual(verified['failures'],[])
            self.assertEqual(verified['originals_verified_unchanged'],1)
            self.assertRaises(ValueError,repair.apply_plan,{'files':files},{'hl2':[root]},target)
            script.write_bytes(original+b'changed')
            self.assertRaises(ValueError,repair.apply_plan,{'files':files},{'hl2':[root]},Path(td)/'new')

    def test_deterministic_package_reproduction_and_wrong_base_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            folder=Path(td)
            root=folder/'source'
            (root/'modernize/reports').mkdir(parents=True)
            (root/'retained.txt').write_bytes(b'original')
            (root/'modified.txt').write_bytes(b'after')
            (root/'added.txt').write_bytes(b'new')
            base=folder/'base.zip'
            with zipfile.ZipFile(base,'w') as z:
                z.writestr('retained.txt',b'original')
                z.writestr('modified.txt',b'before')
            first=package.run(base,root=root,out=folder/'out')
            second=package.run(base,root=root,out=folder/'out')
            self.assertEqual(first,second)
            patch=folder/'out/HL2_2003_Modernization_Overlay_v18.zip'
            count=overlay.apply(base,patch,folder/'reproduced')
            self.assertGreater(count,3)
            self.assertEqual((folder/'reproduced/modified.txt').read_bytes(),b'after')
            self.assertEqual((folder/'reproduced/retained.txt').read_bytes(),b'original')
            wrong=folder/'wrong.zip'
            with zipfile.ZipFile(wrong,'w') as z:
                z.writestr('retained.txt',b'other')
            self.assertRaises(ValueError,overlay.apply,wrong,patch,folder/'notwritten')
            self.assertRaises(ValueError,package.run,base,root=root,out=root/'outputs')
            truncated=folder/'truncated.zip'
            with zipfile.ZipFile(patch) as source,zipfile.ZipFile(truncated,'w') as target:
                for name in source.namelist():
                    if name!='modernize/release_manifest.json':
                        target.writestr(name,source.read(name))
            self.assertRaises(ValueError,overlay.apply,base,truncated,folder/'incomplete')
            for path in ('../escape','/absolute','C:/drive','dir\\back','a//x'):
                self.assertFalse(package.safe_name(path))


if __name__=='__main__':
    unittest.main()
