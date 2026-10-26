"""Portable C++98 tests for the 2003 BSP visibility decoder hardening."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "engine" / "bsp_vis_safety.h"

CPP_TEST = r'''
#include "bsp_vis_safety.h"
#include "bsp_lump_safety.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <stdlib.h>

static unsigned int seed = 0x1A2B3C4D;
static unsigned int Next() { seed = seed * 1664525u + 1013904223u; return seed; }

int main()
{
    assert(BSPVis_ValidTable(12, 1, 65536));
    assert(BSPVis_ValidTable(4, 0, 65536));
    assert(!BSPVis_ValidTable(11, 1, 65536));
    assert(!BSPVis_ValidTable(4, 1, 65536));
    assert(!BSPVis_ValidTable(4096, -1, 65536));
    assert(!BSPVis_ValidTable(2147483647, 2147483647, 65536));
    assert(!BSPVis_ValidTable(-1, 1, 65536));
    assert(BSPMap_ValidRange(100, 80, 20));
    assert(BSPMap_ValidRange(100, 0, 0));
    assert(!BSPMap_ValidRange(100, 81, 20));
    assert(!BSPMap_ValidRange(100, -1, 10));
    assert(!BSPMap_ValidRange(100, 10, -1));
    assert(!BSPMap_ValidRange(100, 2147483647, 2147483647));
    assert(!BSPMap_ValidRange(2147483648u, 2147483647, 1));
    assert(BSPMap_ValidSubrange(10, 0, 10));
    assert(BSPMap_ValidSubrange(10, 10, 0));
    assert(!BSPMap_ValidSubrange(10, 10, 1));
    assert(!BSPMap_ValidSubrange(10, -1, 2));
    assert(!BSPMap_ValidSubrange(10, 0, -1));
    assert(!BSPMap_ValidSubrange(2147483647, 2147483647, 1));

    // An ordinary row with literals, RLE zeroes and another literal.
    const unsigned char packet[] = {0,0,0,1, 12,0,0,0, 12,0,0,0, 0x45,0,2,0xAB};
    unsigned char out[6] = { 0x11,0x11,0x11,0x11,0x11,0x11 };
    assert(BSPVis_Decompress(packet,sizeof(packet),12,12,out,4));
    assert(out[0] == 0x45 && out[1] == 0 && out[2] == 0 && out[3] == 0xAB);
    assert(out[4] == 0x11 && out[5] == 0x11);

    // No data means all visible; historical -1 offset is supported.
    for(int offset=-20; offset<=16; ++offset)
    {
        memset(out,0,sizeof(out));
        const bool ok=BSPVis_Decompress(packet,sizeof(packet),offset,12,out,4);
        if(offset != 12) {
            assert(!ok);
            for(int i=0;i<4;i++) assert(out[i] == 0xff);
        }
        assert(out[4] == 0 && out[5] == 0);
    }
    const unsigned char badZeroRun[] = {0,0,0,0, 0,0};
    memset(out,0,sizeof(out));
    assert(!BSPVis_Decompress(badZeroRun,sizeof(badZeroRun),4,4,out,3));
    for(int i=0;i<3;i++) assert(out[i] == 0xff);

    // On valid inputs, output must remain identical to the original decoder.
    for(int i=0;i<2000;++i)
    {
        const int len=1+(int)(Next()%1024);
        std::vector<unsigned char> reference(len);
        std::vector<unsigned char> encoded(12,0);
        for(int k=0;k<len;++k) reference[k]=(Next() % 4) ? (unsigned char)(Next()%255+1) : 0;
        for(int k=0;k<len;)
        {
            if(reference[k]) {encoded.push_back(reference[k++]);continue;}
            int count=0;
            while(k<len && !reference[k] && count<255) {++k;++count;}
            encoded.push_back(0);encoded.push_back((unsigned char)count);
        }
        std::vector<unsigned char> actual(len+2,0x7E);
        assert(BSPVis_Decompress(&encoded[0],(int)encoded.size(),12,12,&actual[0],len));
        assert(!memcmp(&actual[0],&reference[0],len));
        assert(actual[len] == 0x7e && actual[len+1]==0x7e);
    }

    // Exercise offset and length arithmetic near integer limits.
    for(int i=0;i<20000;i++)
    {
        unsigned int fileLen=Next();
        int start=(int)Next();
        int bytes=(int)Next();
        const bool accepted=BSPMap_ValidRange(fileLen,start,bytes);
        if(accepted)
        {
            assert(start>=0 && bytes>=0);
            assert((unsigned int)start <= fileLen);
            assert((unsigned int)bytes <= fileLen-(unsigned int)start);
        }
    }

    // Fuzz corrupted vis lumps, offsets and lengths under sanitizers.
    unsigned char payload[200];
    for(int i=0;i<30000;++i)
    {
        for(int k=0;k<200;++k) payload[k]=(unsigned char)Next();
        unsigned char guard[66];
        memset(guard,0x5A,sizeof(guard));
        int size=(int)(Next()%201);
        int offset=(int)(Next()%230)-20;
        int rowLen=(int)(Next()%65);
        (void)BSPVis_Decompress(payload,size,offset,12,guard+1,rowLen);
        assert(guard[0] == 0x5a);
        if(rowLen<65) assert(guard[rowLen+1] == 0x5a);
    }
    return 0;
}
'''

class BSPVisibilityTests(unittest.TestCase):
    def test_header_and_decoder_compiles_and_runs(self):
        cxx = shutil.which("clang++") or shutil.which("g++")
        if not cxx:
            self.skipTest("No C++ compiler")
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory)/"test.cpp"
            exe = Path(directory)/"test"
            cpp.write_text(CPP_TEST)
            cmd = [cxx, "-std=c++98", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(SOURCE.parent), str(cpp), "-o", str(exe)]
            if "clang++" in cxx:
                cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            subprocess.run(cmd,check=True,capture_output=True,text=True)
            subprocess.run([str(exe)],check=True,capture_output=True,text=True)

    def test_loader_validates_vis_table_before_indexing(self):
        src = (ROOT/"engine/cmodel_bsp.cpp").read_text(encoding="latin-1")
        func = src.rsplit("void CollisionBSPData_LoadVisibility",1)[1].split("void CollisionBSPData_LoadEntityString",1)[0]
        self.assertLess(func.index("BSPVis_ValidTable"),func.index("for (i=0 ; i<visClusters ; i++)"))
        self.assertIn("memcpy( &visClusters, lh.LumpBase()", func)

    def test_map_file_bounds_are_checked(self):
        src = (ROOT/"engine/modelloader.cpp").read_text(encoding="latin-1")
        self.assertIn('BSPMap_ValidRange( s_MapFileSize, lump->fileofs, lump->filelen )', src)
        self.assertIn('BSPMap_ValidSubrange( pLump->filelen, offset, size )', src)
        self.assertIn('Read( m_pData, lump->filelen, s_MapFile ) != lump->filelen', src)
        self.assertIn('m_pData[m_nLumpSize] = 0',src)
        self.assertIn('Read( &s_MapHeader, sizeof( dheader_t ), s_MapFile ) != sizeof( dheader_t )',src)

    def test_visibility_use_is_bounds_checked(self):
        src = (ROOT/"engine/cmodel.cpp").read_text(encoding="latin-1")
        func = src.split("void CM_DecompressVis",1)[1].split("byte *CM_Vis",1)[0]
        self.assertIn("BSPVis_Decompress(",func)
        self.assertIn("cluster >= visClusters",func)
        self.assertNotIn("__asm int 3",func)
        self.assertIn("CM_NullVis",func)

    def test_beta_hud_weapons_displacement_unchanged(self):
        """Ensure the patch hasn't touched the protected feature directories."""
        from zipfile import ZipFile
        baseline = Path('/mnt/data/GammaSource_Beta_Core_Stability_Round2_Full_Source.zip')
        if not baseline.exists():
            self.skipTest("Round 2 comparison archive not mounted")
        protected = ('GammaSource-main/cl_dll/', 'GammaSource-main/dlls/',
                     'GammaSource-main/gammasource_content/',
                     'GammaSource-main/engine/disp', 'GammaSource-main/engine/gl_model')
        with ZipFile(baseline) as z:
            for name in z.namelist():
                if name.endswith('/') or not name.startswith(protected):
                    continue
                # Converted VS build files gain dependency edges separately;
                # this guard protects beta gameplay and content, not metadata.
                if name.endswith(('.vcxproj', '.vcxproj.filters', '.vcxproj.user')):
                    continue
                path = ROOT/name.removeprefix('GammaSource-main/')
                self.assertTrue(path.is_file(),name)
                self.assertEqual(path.read_bytes(),z.read(name),name)

if __name__ == '__main__':
    unittest.main()
