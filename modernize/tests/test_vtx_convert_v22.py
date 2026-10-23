"""Synthetic, independent packed-structure tests for v7->v6 conversions."""
import importlib.util
from pathlib import Path
import struct
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'convert_vtx7_to_v6_v22.py'
spec = importlib.util.spec_from_file_location('vtx_v22', MODULE)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    b = bytearray(36)
    def add(data):
        x = len(b); b.extend(data); return x
    def at(fmt,pos,*args):
        struct.pack_into('<'+fmt,b,pos,*args)
    repl = add(b'\x00'*8)
    body = add(b'\x00'*8)
    model= add(b'\x00'*8)
    lod  = add(b'\x00'*12)
    mesh = add(b'\x00'*9)
    group= add(b'\x00'*25)
    verts=add(b''.join(struct.pack('<BBBBhbbb', 0,1,2,2,i,0,1,-1) for i in range(3)))
    indices=add(struct.pack('<3H', 0,1,2))
    strip=add(b'\x00'*27)
    changes=add(struct.pack('<ii',0,3))
    replacements=add(b'\x00'*6)
    name=add(b'models/testing/skin\x00')
    at('iiHHiiiiii',0,7,128,16,8,3,0x1234abcd,1,repl,1,body)
    at('ii',repl,1,replacements-repl)
    at('hi',replacements,5,name-replacements)
    at('ii',body,1,model-body)
    at('ii',model,1,lod-model)
    at('iif',lod,1,mesh-lod,25.0)
    at('iiB',mesh,1,group-mesh,1)
    at('iiiiiiB',group,3,verts-group,3,indices-group,1,strip-group,2)
    at('iiiihBii',strip,3,0,3,0,2,1,1,changes-strip)
    mdl=bytearray(96)
    struct.pack_into('<4sII',mdl,0,b'IDST',37,0x1234abcd)
    struct.pack_into('<I',mdl,76,len(mdl))
    return bytes(b),bytes(mdl)

class TestVtx7Conversion(unittest.TestCase):
    def test_full_tree(self):
        source,mdl=fixture()
        target=mod.convert_v7_to_v6(source,mdl)
        version,cache,maxstrip,maxtri,nvert,crc,nlod,repl,nparts,parts =struct.unpack_from('<iiHHiiiiii',target,0)
        self.assertEqual((version,cache,maxstrip,maxtri,nvert,crc,nlod,nparts),(6,128,16,8,4,0x1234abcd-0x100000000 if 0x1234abcd>0x7fffffff else 0x1234abcd,1,1))
        self.assertEqual(struct.unpack_from('<i',target,repl)[0],1)
        mat= repl+struct.unpack_from('<i',target,repl+4)[0]
        self.assertEqual(struct.unpack_from('<h',target,mat)[0],5)
        self.assertIn(b'models/testing/skin\x00',target)
        model=parts+struct.unpack_from('<i',target,parts+4)[0]
        lod=model+struct.unpack_from('<i',target,model+4)[0]
        mesh=lod+struct.unpack_from('<i',target,lod+4)[0]
        group=mesh+struct.unpack_from('<i',target,mesh+4)[0]
        vert=group+struct.unpack_from('<i',target,group+4)[0]
        self.assertEqual(struct.unpack_from('<4B4hhB',target,vert),(0,1,2,0,0,1,-1,-1,0,2))
        idx=group+struct.unpack_from('<i',target,group+12)[0]
        self.assertEqual(struct.unpack_from('<3H',target,idx),(0,1,2))
        strip=group+struct.unpack_from('<i',target,group+20)[0]
        self.assertEqual(struct.unpack_from('<hB',target,strip+16),(2,1))
        change=strip+struct.unpack_from('<i',target,strip+23)[0]
        self.assertEqual(struct.unpack_from('<ii',target,change),(0,3))
    def test_reject_truncation(self):
        src,_=fixture()
        for part in (src[:12],src[:45],src[:120],src[:-1]):
            with self.assertRaises(mod.FormatError): mod.convert_v7_to_v6(part)
    def test_reject_non_v7(self):
        src,_=fixture()
        mutated=bytearray(src);struct.pack_into('<I',mutated,0,6)
        with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(mutated)
    def test_mdl_checksum(self):
        src,mdl=fixture(); mutated=bytearray(mdl);mutated[8]^=1
        with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(src,mutated)
    def test_reject_malformed_counts(self):
        src,_=fixture()
        for pos,value in ((28,9999),(20,999),(32,0x7fffffff)):
            mutated=bytearray(src);struct.pack_into('<i',mutated,pos,value)
            with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(mutated)
    def test_reject_out_of_range_vertex(self):
        src,_=fixture(); mutated=bytearray(src)
        # Find group from independently-known fixture layout
        at=36+8+8+8+12+9
        off=at+struct.unpack_from('<i',mutated,at+12)[0]
        struct.pack_into('<H',mutated,off,999)
        with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(mutated)
    def test_reject_unsupported_delta_flex(self):
        src,_=fixture(); mutated=bytearray(src)
        group=36+8+8+8+12+9
        mutated[group+24]=4
        with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(mutated)
    def test_advertised_512_bone_palette_with_small_real_strips(self):
        src,_=fixture(); mutated=bytearray(src)
        struct.pack_into('<H',mutated,8,512)
        out=mod.convert_v7_to_v6(mutated)
        self.assertEqual(struct.unpack_from('<H',out,8)[0],16)
    def test_reject_real_strip_exceeding_legacy_palette(self):
        src,_=fixture(); mutated=bytearray(src)
        strip=36+8+8+8+12+9+25+27+6
        struct.pack_into('<h',mutated,strip+16,17)
        with self.assertRaises(mod.FormatError):mod.convert_v7_to_v6(mutated)

if __name__=='__main__':unittest.main()
