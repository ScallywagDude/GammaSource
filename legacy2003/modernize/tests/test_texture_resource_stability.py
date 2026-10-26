"""Runtime C++98 regression tests of GammaSource VTF read accounting and buffer bounds."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / 'public/utlbuffer.cpp').read_bytes().decode('latin1')
TEX = (ROOT / 'materialsystem/ctexture.cpp').read_bytes().decode('latin1')
VTF = (ROOT / 'vtf/vtf.cpp').read_bytes().decode('latin1')


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if not depth:
                return source[start:i + 1]
    raise AssertionError('Unclosed: ' + signature)


def compile_and_run(code):
    cc = shutil.which('clang++') or shutil.which('g++')
    if not cc:
        raise unittest.SkipTest('No C++ compiler')
    with tempfile.TemporaryDirectory() as d:
        src, exe = Path(d) / 'resource_test.cpp', Path(d) / 'resource_test'
        src.write_text(code)
        proc = subprocess.run([cc, '-std=c++98', '-O1', '-g', '-Wall', '-Wextra',
                               '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                               '-I', str(ROOT / 'materialsystem'), str(src), '-o', str(exe)],
                              capture_output=True, text=True)
        assert proc.returncode == 0, proc.stderr
        proc = subprocess.run([str(exe)], capture_output=True, text=True)
        assert proc.returncode == 0, proc.stdout + proc.stderr


class TextureResourceStabilityTests(unittest.TestCase):
    def test_utlbuffer_actual_methods_are_bounded(self):
        methods = '\n\n'.join([extract(CPP, 'void CUtlBuffer::Get( void* pMem, int size )'),
                                 extract(CPP, 'bool CUtlBuffer::CheckGet( int size )'),
                                 extract(CPP, 'bool CUtlBuffer::CheckPut( int size )')])
        code = r'''
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
struct TestMemory {
  std::vector<unsigned char> data;
  bool external;
  TestMemory():data(4,0),external(false) {for(int i=0;i<4;i++) data[i]=(unsigned char)(i+10);}
  int NumAllocated() const {return (int)data.size();}
  unsigned char& operator[](int i) {return data[(unsigned int)i];}
  bool IsExternallyAllocated() const {return external;}
  void Grow(){ data.resize(data.size()+8); }
};
class CUtlBuffer {
public:
  enum {GET_OVERFLOW=2, PUT_OVERFLOW=1};
  TestMemory m_Memory;
  int m_Get,m_Put,m_Error;
  CUtlBuffer():m_Get(0),m_Put(0),m_Error(0){}
  bool IsValid()const{return m_Error==0;}
  bool CheckGet(int size);
  bool CheckPut(int size);
  void Get(void* dst,int size);
};
''' + methods + r'''
int main() {
  CUtlBuffer b;
  unsigned char out[8]={0};
  b.Get(out,4);
  for(int i=0;i<4;i++)assert(out[i] == i+10);
  assert(b.m_Get==4 && b.IsValid());
  b.Get(out,2);
  assert(!b.IsValid() && b.m_Get==4);
  assert(out[0]==10); // failed read does not touch target
  CUtlBuffer c;
  c.m_Get=INT_MAX;
  assert(!c.CheckGet(1));
  CUtlBuffer d;
  assert(!d.CheckGet(-1));
  CUtlBuffer e;
  e.m_Put=INT_MAX-2;
  assert(!e.CheckPut(3) && !e.IsValid());
  CUtlBuffer f;
  assert(!f.CheckPut(-4));
  CUtlBuffer g;
  g.m_Put=4;
  assert(g.CheckPut(100));
  assert(g.m_Memory.NumAllocated()>=104);
  CUtlBuffer h;
  h.m_Memory.external=true;
  assert(!h.CheckPut(5));
}
'''
        compile_and_run(code)

    def test_vtf_actual_loader_handles_short_and_complete_reads(self):
        method = extract(TEX, 'IVTFTexture *CTexture::LoadTextureBitsFromFile( )')
        code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <vector>
#include "texture_file_safety.h"
#define MATERIAL_MAX_PATH 256
#define IMAGE_FORMAT_BGRA8888 12
#define TEXTUREFLAGS_ERROR 0x1000000
#define TEXTUREFLAGS_PROCEDURAL 0x800
#define Q_snprintf snprintf
#define Q_strnicmp strncasecmp
struct Vector {float a,b,c; void Init(float x,float y,float z){a=x;b=y;c=z;}};
#define VectorCopy(from,to) ((to)=(from))
struct Name {const char* String()const{return "test";}};
class CUtlBuffer {
 std::vector<unsigned char> bytes;
 int put, get;
public:
 enum SeekType_t { SEEK_HEAD=0 };
 CUtlBuffer():bytes(),put(0),get(0){}
 void EnsureCapacity(int n){bytes.resize((unsigned int)n);}
 void* Base(){return &bytes[0];}
 void* PeekPut(){return &bytes[(unsigned int)put];}
 void SeekPut(SeekType_t,int v){put=v;}
 void SeekGet(SeekType_t,int v){get=v;}
 int TellPut()const{return put;}
 const unsigned char* Data()const{return &bytes[0];}
};
struct FakeVTF {
 int width,height,frames,flags,version;
 Vector refl;
 int readFull;
 FakeVTF():width(8),height(8),frames(1),flags(0),version(0),readFull(0){}
 bool Unserialize(CUtlBuffer& b,bool headerOnly,int skip=0) {
   (void)skip;
   if(b.Data()[0]!='V')return false;
   if(!headerOnly) {assert(b.TellPut()==32); readFull++;}
   return true;
 }
 int Width()const{return width;} int Height()const{return height;}
 int Format()const{return IMAGE_FORMAT_BGRA8888;}
 int Flags()const{return flags;} int FrameCount()const{return frames;}
 Vector Reflectivity()const{return refl;}
 int FileSize(int)const{return 32;}
 void Init(int w,int h,int,int f,int n){width=w;height=h;flags=f;frames=n;}
};
typedef FakeVTF IVTFTexture;
typedef int FileHandle_t;
const FileHandle_t FILESYSTEM_INVALID_HANDLE=-1;
class MockFS {
 public:
 std::vector<unsigned char> bytes;
 int offset,closeCount,readCount,shortRead;
 MockFS():bytes(32,'V'),offset(0),closeCount(0),readCount(0),shortRead(0){}
 FileHandle_t Open(const char*,const char*){offset=0;return 1;}
 unsigned int Size(FileHandle_t)const{return (unsigned int)bytes.size();}
 int Read(void* dst,int len,FileHandle_t){
   int count=len;
   if(shortRead && readCount==1)count=len-1;
   if((unsigned int)(offset+count)>bytes.size())count=(int)bytes.size()-offset;
   if(count>0) std::memcpy(dst,&bytes[(unsigned int)offset],(unsigned int)count);
   offset+=count;readCount++;return count;
 }
 void Close(FileHandle_t){closeCount++;}
};
MockFS fs; MockFS* g_pFileSystem=&fs;
FakeVTF scratch;
struct MockManager {int errors;MockManager():errors(0){}void GenerateErrorTexture(void*,FakeVTF*){errors++;}};
MockManager manager;
MockManager* TextureManager(){return &manager;}
void Warning(const char*,...){ }
int VTFFileHeaderSize(){return 8;}
class CTexture {
public:
 Name m_Name; Vector m_vecReflectivity;
 int m_nActualWidth,m_nActualHeight,m_nActualMipCount,m_nFlags;
 CTexture():m_nActualWidth(0),m_nActualHeight(0),m_nActualMipCount(0),m_nFlags(0){}
 IVTFTexture* GetScratchVTFTexture(){return &scratch;}
 void Init(int,int,int,int,int){}
 int ComputeActualSize(){return 0;}
 void BuildLowResTexture(IVTFTexture*){}
 bool IsProcedural(){return false;}
 void SetupDebuggingTextures(IVTFTexture*){}
 void ConvertToActualFormat(IVTFTexture*){}
 IVTFTexture* LoadTextureBitsFromFile();
};
''' + method + r'''
int main(){
 CTexture t;
 assert(t.LoadTextureBitsFromFile()==&scratch);
 assert(scratch.readFull==1 && fs.closeCount==1 && manager.errors==0);
 fs.bytes.resize(12); fs.readCount=fs.closeCount=0;
 assert(t.LoadTextureBitsFromFile()==&scratch);
 assert(fs.closeCount==1 && manager.errors==1);
 fs.bytes.resize(32,'V');fs.readCount=fs.closeCount=0;fs.shortRead=1;
 assert(t.LoadTextureBitsFromFile()==&scratch);
 assert(fs.closeCount==1 && manager.errors==2);
 fs.shortRead=0; fs.readCount=fs.closeCount=0;
 fs.bytes.resize(7);
 assert(t.LoadTextureBitsFromFile()==&scratch);
 assert(fs.closeCount==1 && manager.errors==3);
}
'''
        compile_and_run(code)

    def test_source_retains_geometry_and_format_checks(self):
        self.assertIn('header.imageFormat >= NUM_IMAGE_FORMATS', VTF)
        self.assertIn('header.numFrames == 0', VTF)
        self.assertIn('!IsPowerOfTwo( header.width )', VTF)
        self.assertIn('nStoredFaces = 6', VTF)
        self.assertIn('TextureFile_ExpectedSizeFits', TEX)
        self.assertIn('buf.SeekPut( CUtlBuffer::SEEK_HEAD, nFileSize );', TEX)

    def test_protection_helpers_boundary_behavior(self):
        compile_and_run(r'''
#include <cassert>
#include <climits>
#include "texture_file_safety.h"
int main(){
 assert(TextureFile_HeaderFits(32,8));
 assert(!TextureFile_HeaderFits(7,8));
 assert(!TextureFile_HeaderFits(UINT_MAX,0));
 assert(TextureFile_ExpectedSizeFits(32,8,32));
 assert(TextureFile_ExpectedSizeFits(32,8,8));
 assert(!TextureFile_ExpectedSizeFits(32,8,33));
 assert(!TextureFile_ExpectedSizeFits(UINT_MAX,8,-1));
 assert(!TextureFile_ExpectedSizeFits(10,8,INT_MAX));
 assert(TextureFile_ReadCompleted(0,0));
 assert(TextureFile_ReadCompleted(32,32));
 assert(!TextureFile_ReadCompleted(32,31));
 assert(!TextureFile_ReadCompleted(-1,-1));
}
''')


if __name__ == '__main__':
    unittest.main()
