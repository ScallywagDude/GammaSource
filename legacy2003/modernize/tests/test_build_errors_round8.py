"""Compile and project-order regressions for reported Round 8 Win32 failures."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}


def extract_function(source, marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{': depth += 1
        if source[index] == '}':
            depth -= 1
            if depth == 0: return source[start:index+1]
    raise AssertionError('Unclosed source function '+marker)


class BuildErrorsRound8(unittest.TestCase):
    def test_bitbuf_compiles_with_real_reader_constness_and_checks_bounds(self):
        compiler = shutil.which('g++')
        if compiler is None: self.skipTest('C++98 compiler not available')
        cpp = (ROOT/'public/bitbuf.cpp').read_text(encoding='latin1')
        method = extract_function(cpp, 'bool bf_write::WriteBitsFromBuffer(')
        code = r'''
#include <cassert>
#include <climits>
class bf_read {
 public:
 const unsigned char *m_pData;
 int available;
 unsigned index;
 bool overflow;
 bf_read(const unsigned char *p, int n) : m_pData(p), available(n), index(0), overflow(false) {}
 bool IsOverflowed() {return overflow;}
 int GetNumBitsLeft() {return available - (int)index;}
 void SetOverflowFlag() {overflow = true;}
 unsigned ReadUBitLong(int n) { if(n<0 || n>GetNumBitsLeft()){overflow=true;return 0;} index+=n;return 0x5; }
};
class bf_write {
 public:
 bool overflow;
 int available;
 int written;
 bf_write(int n):overflow(false),available(n),written(0){}
 bool CheckForOverflow(int n){if(n<0 || n>available-written){overflow=true;return true;}return false;}
 bool IsOverflowed(){return overflow;}
 void SetOverflowFlag(){overflow=true;}
 void WriteUBitLong(unsigned, int n){written+=n;}
 bool WriteBitsFromBuffer(bf_read *pIn, int nBits);
};
''' + method + r'''
int main(){
 unsigned char msg[8]={0};
 bf_read good(msg,64);bf_write target(64);
 assert(target.WriteBitsFromBuffer(&good,41));assert(target.written==41 && good.index==41);
 bf_read shortbuf(msg,8);bf_write target2(64);
 assert(!target2.WriteBitsFromBuffer(&shortbuf,16));
 assert(shortbuf.overflow && target2.overflow && target2.written==0);
 bf_read empty(msg,0);bf_write target3(64);
 assert(target3.WriteBitsFromBuffer(&empty,0));
 bf_read neg(msg,64);bf_write target4(64);
 assert(!target4.WriteBitsFromBuffer(&neg,-1));
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)/'test.cpp'; source.write_text(code)
            binary=Path(temp)/'test'
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_missing_read_write_access_errors_resolved(self):
        source=(ROOT/'public/bitbuf.cpp').read_text(encoding='latin1')
        body=extract_function(source,'bool bf_write::WriteBitsFromBuffer(')
        self.assertNotIn('pIn->CheckForOverflow(',body)
        excise=extract_function(source,'void bf_read::ExciseBits(')
        self.assertNotRegex(excise, r'\bm_pData\s*\[[^]]+\]\s*(?:\|=|&=|=)')
        self.assertIn('pWritableData',excise)
        self.assertIn('strchr( filename', (ROOT/'engine/net_chan.cpp').read_text(encoding='latin1'))

    def test_local_dependencies_are_declared_and_buildable(self):
        from sys import path
        path.insert(0,str(ROOT/'modernize'))
        from repair_project_dependencies_round8 import DEPENDENCIES, patch
        self.assertEqual(patch(), [])
        count=0
        for vcxproj in ROOT.rglob('*.vcxproj'):
            r=ET.parse(vcxproj).getroot()
            linked=set()
            for n in r.findall('.//m:AdditionalDependencies',NS):
                for item in (n.text or '').split(';'):
                    name=item.strip().replace('/','\\').split('\\')[-1].lower()
                    if name in DEPENDENCIES: linked.add(name)
            project_refs=[ref for ref in r.findall('.//m:ProjectReference',NS)]
            for lib in linked:
                target=DEPENDENCIES[lib]
                if target.resolve()==vcxproj.resolve(): continue
                matches=[ref for ref in project_refs if
                    (vcxproj.parent / ref.attrib['Include'].replace('\\',os.sep)).resolve()==target.resolve()]
                self.assertEqual(len(matches),1,str(vcxproj)+': '+lib)
                self.assertEqual(matches[0].findtext('m:LinkLibraryDependencies',namespaces=NS),'false')
                count+=1
        self.assertGreater(count,100)

    def test_dx9_header_search_path_preserved(self):
        self.assertTrue((ROOT/'dx9sdk/include/d3dx9.h').is_file())
        for project in ('shaderdx8.vcxproj','shaderdx8_ihvtest.vcxproj'):
            root=ET.parse(ROOT/'materialsystem/shaderdx8'/project).getroot()
            dirs=[node.text for node in root.findall('.//m:AdditionalIncludeDirectories',NS)]
            self.assertTrue(all('dx9sdk\\include' in (s or '').lower() for s in dirs),project)

if __name__=='__main__': unittest.main()
