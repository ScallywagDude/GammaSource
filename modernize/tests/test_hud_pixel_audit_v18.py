"""Synthetic codec and atlas-rectangle tests; no proprietary fixture assets."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
spec=importlib.util.spec_from_file_location('hud_pixel_audit_v18',ROOT/'modernize/hud_pixel_audit_v18.py')
hud=importlib.util.module_from_spec(spec);spec.loader.exec_module(hud)


def vtf(width,height,image_format,payload,frames=1,mips=1,minor=1,low=None):
    header=bytearray(64)
    header[:4]=b'VTF\0'
    struct.pack_into('<III',header,4,7,minor,64)
    struct.pack_into('<HHIHH',header,16,width,height,0,frames,0)
    struct.pack_into('<iB',header,52,image_format,mips)
    struct.pack_into('<iBB',header,57,-1,0,0)
    if low:
        fmt,w,h,data=low
        struct.pack_into('<iBB',header,57,fmt,w,h)
        payload=data+payload
    return bytes(header)+payload


class HudPixelAudit(unittest.TestCase):
    def test_dxt1_interpolation_and_transparency(self):
        codes=sum((i%4)<<(i*2) for i in range(16))
        image=hud.decode_pixels(struct.pack('<HHI',0xf800,0x07e0,codes),4,4,13)
        self.assertEqual(image.getpixel((0,0)),(255,0,0,255))
        self.assertEqual(image.getpixel((1,0)),(0,255,0,255))
        self.assertEqual(image.getpixel((2,0)),(170,85,0,255))
        self.assertEqual(hud.dds_reference_check(struct.pack('<HHI',0xf800,0x07e0,codes),4,4,13,image)['status'],'matches')
        transparent=hud.decode_pixels(struct.pack('<HHI',0,0xffff,3),4,4,20)
        self.assertEqual(transparent.getpixel((0,0))[3],0)

    def test_dxt3_explicit_alpha_and_odd_dimension_edge_blocks(self):
        alpha=sum((i%16)<<(4*i) for i in range(16))
        block=alpha.to_bytes(8,'little')+struct.pack('<HHI',0x001f,0,0)
        image=hud.decode_pixels(block+block,5,3,14)
        self.assertEqual(image.size,(5,3))
        self.assertEqual(image.getpixel((0,0)),(0,0,255,0))
        self.assertEqual(image.getpixel((3,0))[3],51)
        self.assertEqual(image.getpixel((4,0))[3],0)

    def test_dxt5_both_alpha_interpolation_modes(self):
        codes=sum((i%8)<<(i*3) for i in range(16))
        block=bytes([255,0])+codes.to_bytes(6,'little')+struct.pack('<HHI',0xf800,0,0)
        image=hud.decode_pixels(block,4,4,15)
        self.assertEqual([image.getpixel((i,0))[3] for i in range(4)],[255,0,218,182])
        block=bytes([0,255])+codes.to_bytes(6,'little')+struct.pack('<HHI',0xf800,0,0)
        image=hud.decode_pixels(block,4,4,15)
        self.assertEqual(image.getpixel((2,1))[3],0)
        self.assertEqual(image.getpixel((3,1))[3],255)

    def test_uncompressed_channel_order_and_alpha(self):
        cases={0:(11,22,33,44),1:(44,33,22,11),11:(22,33,44,11),12:(33,22,11,44),16:(33,22,11,255)}
        for fmt,color in cases.items():
            self.assertEqual(hud.decode_pixels(bytes([11,22,33,44]),1,1,fmt).getpixel((0,0)),color)
        self.assertEqual(hud.decode_pixels(bytes([99,17]),1,1,6).getpixel((0,0)),(99,99,99,17))
        self.assertEqual(hud.decode_pixels(bytes([123]),1,1,8).getpixel((0,0)),(255,255,255,123))
        self.assertEqual(hud.decode_pixels(struct.pack('<H',0xf800),1,1,17).getpixel((0,0)),(255,0,0,255))
        self.assertEqual(hud.decode_pixels(bytes([0,0,255]),1,1,9).getpixel((0,0))[3],0)

    def test_vtf_mip_frame_and_thumbnail_offsets(self):
        payload=b''
        for size in (2,4,8):
            for color in (bytes([255,0,0,255]),bytes([0,0,255,255])):
                payload+=color*(size*size)
        low=(13,4,4,struct.pack('<HHI',0xf800,0,0))
        metadata,frames=hud.decode_vtf(vtf(8,8,0,payload,frames=2,mips=3,low=low))
        self.assertEqual(metadata['decoded_subimages'],6)
        self.assertEqual(metadata['trailing_bytes'],0)
        self.assertTrue(metadata['thumbnail_pixels_decoded'])
        self.assertEqual(frames[0].getpixel((7,7)),(255,0,0,255))
        self.assertEqual(frames[1].getpixel((7,7)),(0,0,255,255))
        with self.assertRaises(hud.PixelDecodeError):hud.decode_vtf(vtf(8,8,0,payload[:-1],frames=2,mips=3))
        with self.assertRaises(hud.PixelDecodeError):hud.decode_vtf(vtf(8,8,0,payload,minor=2))

    def test_additive_nonblank_and_aspect_preserving_fit(self):
        black=hud.decode_pixels(bytes([0,0,0,255])*4,2,2,0)
        self.assertEqual(hud.pixel_metrics(black,additive=True)['effective_nonblank_pixels'],0)
        self.assertEqual(hud.pixel_metrics(black,additive=False)['effective_nonblank_pixels'],4)
        self.assertEqual(hud.fit_dimensions(170,45,85,100),(85,22))
        self.assertEqual(hud.fit_dimensions(24,24,248,96),(24,24))

    def test_weapon_membership_bounds_and_blank_icon_audit(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'scripts').mkdir();(root/'materials/sprites').mkdir(parents=True)
            (root/'materials/sprites/test.vmt').write_text('"UnlitGeneric" { "$basetexture" "sprites/test" "$additive" "1" }')
            (root/'materials/sprites/test.vtf').write_bytes(vtf(2,2,0,bytes([0,0,0,255])*4))
            script='"WeaponData" { "TextureData" { "weapon" { "file" "sprites/test" "x" "0" "y" "0" "width" "2" "height" "2" } "ammo" { "file" "sprites/test" "x" "1" "y" "1" "width" "2" "height" "2" } } }'
            (root/'scripts/weapon_fixture.txt').write_text(script)
            report=hud.audit_branch('hl2',[root],registered_classnames={'weapon_fixture'})
            self.assertEqual(report['registered_scripts_inspected'],1)
            self.assertEqual(report['textures_pixels_decoded'],1)
            icons={i['role']:i for i in report['weapons'][0]['icons']}
            self.assertEqual(icons['weapon']['status'],'blank_icon_pixels')
            self.assertEqual(icons['ammo']['status'],'outside_texture_bounds')
            report=hud.audit_branch('hl2',[root],registered_classnames=set())
            self.assertEqual(report['script_only_entries'],['weapon_fixture'])

    def test_legacy_resolution_priority_and_leaf_material_normalization(self):
        direct={'file':'sprites/invalid_direct','640':{'file':'test'},'320':{'file':'other'}}
        self.assertEqual(hud.select_sprite_spec(direct),(direct,'direct'))
        nested={'640':{'file':'test','width':'24'},'320':{'file':'other','width':'12'}}
        self.assertEqual(hud.select_sprite_spec(nested),(nested['640'],'legacy_resolution_640'))
        fallback={'640':{},'320':{'file':'other'}}
        self.assertEqual(hud.select_sprite_spec(fallback),(fallback['320'],'legacy_resolution_320'))
        empty={'file':'','640':{'file':'test'}}
        self.assertEqual(hud.select_sprite_spec(empty),(empty['640'],'legacy_resolution_640'))
        bad640={'640':{'file':''},'320':{'file':'test'}}
        self.assertEqual(hud.select_sprite_spec(bad640),(bad640['320'],'legacy_resolution_320'))
        font={'font':'WeaponIcons','file':'sprites/test'}
        self.assertEqual(hud.select_sprite_spec(font),(font,'font_role_source_unsupported'))
        nested_font={'640':{'file':'test','font':'WeaponIcons'},'320':{'file':'test'}}
        self.assertEqual(hud.select_sprite_spec(nested_font),(nested_font['640'],'font_legacy_resolution_640_source_unsupported'))
        self.assertEqual(hud.sprite_material_name('a_icons1',True),'sprites/a_icons1')
        self.assertEqual(hud.sprite_material_name('a_icons1'),'a_icons1')
        self.assertEqual(hud.sprite_material_name('sprites/crosshairs'),'sprites/crosshairs')
        self.assertEqual(hud.sprite_material_name('ui\\crosshairs'),'ui\\crosshairs')
        self.assertEqual(hud.sprite_material_name('C:test',True),'C:test')
        self.assertIsNone(hud.sprite_material_name(''))


if __name__=='__main__':unittest.main()
