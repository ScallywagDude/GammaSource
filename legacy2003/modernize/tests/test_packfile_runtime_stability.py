"""Regression tests for the actual 2003 filesystem ZIP/pack handle methods."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / 'filesystem/filesystem_stdio/BaseFileSystem.h').read_bytes().decode('latin1')
SOURCE = (ROOT / 'filesystem/filesystem_stdio/BaseFileSystem.cpp').read_bytes().decode('latin1')


def function(signature):
    start = SOURCE.index(signature)
    begin = SOURCE.index('{', start)
    depth = 0
    for i in range(begin, len(SOURCE)):
        if SOURCE[i] == '{':
            depth += 1
        if SOURCE[i] == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:i + 1]
    raise AssertionError('Unclosed method ' + signature)


def compile_test(text):
    cc = shutil.which('clang++') or shutil.which('g++')
    if not cc:
        raise unittest.SkipTest('No C++ compiler')
    with tempfile.TemporaryDirectory() as folder:
        inp, out = Path(folder)/'test.cpp', Path(folder)/'test'
        inp.write_text(text)
        result = subprocess.run([cc, '-std=c++98', '-O0', '-g', '-Wall', '-Wextra',
                                 '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                 str(inp), '-o', str(out)], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(out)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


class PackfileRuntimeStabilityTests(unittest.TestCase):
    def test_pack_member_methods_handle_interleaved_reads_and_seek_boundaries(self):
        assert 'm_nPackPosition;' in HEADER
        methods = '\n\n'.join(function(x) for x in [
            'void CBaseFileSystem::Seek(', 'unsigned int CBaseFileSystem::Tell(',
            'bool CBaseFileSystem::EndOfFile(', 'int CBaseFileSystem::Read('])
        pre = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cstdarg>
#define VPROF_BUDGET(a,b)
#define VPROF_BUDGETGROUP_OTHER_FILESYSTEM 0
#define FILESYSTEM_WARNING 0
#define FILESYSTEM_SEEK_HEAD 0
#define FILESYSTEM_SEEK_CURRENT 1
#define FILESYSTEM_SEEK_TAIL 2
typedef int FileSystemSeek_t;
typedef void* FileHandle_t;
struct FileStats {int nReads; unsigned int nBytesRead; FileStats():nReads(0),nBytesRead(0){} };
class CBaseFileSystem {
public:
 struct CFileHandle {
    FILE *m_pFile; int m_nStartOffset, m_nLength, m_nPackPosition; bool m_bPack;
    CFileHandle():m_pFile(0),m_nStartOffset(0),m_nLength(0),m_nPackPosition(0),m_bPack(false){}
 };
 FileStats m_Stats;
 void Seek(FileHandle_t,int,FileSystemSeek_t);
 unsigned int Tell(FileHandle_t);
 bool EndOfFile(FileHandle_t);
 int Read(void*,int,FileHandle_t);
 void FS_fseek(FILE* f,long p,int w){std::fseek(f,p,w);}
 long FS_ftell(FILE* f){return std::ftell(f);}
 size_t FS_fread(void* x,size_t s,size_t c,FILE* f){return std::fread(x,s,c,f);}
 int FS_feof(FILE* f){return std::feof(f);}
 void Trace_FRead(int, FILE*) {}
 void Warning(int,const char*,...) {}
};
'''
        test = r'''
int main() {
    CBaseFileSystem fs;
    FILE *backing = std::tmpfile();
    assert(backing);
    assert(std::fwrite("<<<<ABCDE/12345>>>>", 1, 19, backing)==19);
    std::fflush(backing);
    CBaseFileSystem::CFileHandle a,b;
    a.m_pFile=b.m_pFile=backing;
    a.m_bPack=b.m_bPack=true;
    a.m_nStartOffset=4;a.m_nLength=5;
    b.m_nStartOffset=10;b.m_nLength=5;
    char buf[32] = {0};
    assert(fs.Read(buf,3,&a)==3 && std::memcmp(buf,"ABC",3)==0);
    assert(fs.Tell(&a)==3 && fs.Tell(&b)==0);
    assert(fs.Read(buf,2,&b)==2 && std::memcmp(buf,"12",2)==0);
    assert(fs.Read(buf,32,&a)==2 && std::memcmp(buf,"DE",2)==0);
    assert(fs.EndOfFile(&a) && fs.Tell(&a)==5);
    assert(fs.Read(buf,5,&a)==0);
    fs.Seek(&b,-1,FILESYSTEM_SEEK_TAIL);
    assert(fs.Tell(&b)==4);
    assert(fs.Read(buf,10,&b)==1 && buf[0]=='5');
    fs.Seek(&a,1,FILESYSTEM_SEEK_HEAD);
    fs.Seek(&b,0,FILESYSTEM_SEEK_HEAD);
    assert(fs.Read(buf,2,&a)==2 && std::memcmp(buf,"BC",2)==0);
    assert(fs.Read(buf,5,&b)==5 && std::memcmp(buf,"12345",5)==0);
    fs.Seek(&b,-1000,FILESYSTEM_SEEK_CURRENT);
    assert(fs.Tell(&b)==0);
    fs.Seek(&b,1000,FILESYSTEM_SEEK_CURRENT);
    assert(fs.Tell(&b)==5 && fs.EndOfFile(&b));
    assert(fs.Read(buf,-100,&a)==0);
    assert(fs.Read(0,5,&a)==0);
    assert(fs.Read(buf,0,&a)==0);
    a.m_nPackPosition=4;a.m_nStartOffset=INT_MAX;
    assert(fs.Read(buf,1,&a)==0);
    CBaseFileSystem::CFileHandle plain;
    plain.m_pFile=backing;
    plain.m_bPack=false;
    fs.Seek(&plain,0,FILESYSTEM_SEEK_HEAD);
    assert(fs.Read(buf,4,&plain)==4 && std::memcmp(buf,"<<<<",4)==0);
    assert(fs.Tell(&plain)==4);
    std::fclose(backing);
}
'''
        compile_test(pre + methods + test)

    def test_mount_calls_use_handles_not_file_pointers(self):
        section = SOURCE[SOURCE.index('void CBaseFileSystem::AddPackFiles('):SOURCE.index('void CBaseFileSystem::RemoveAllMapSearchPaths(')]
        self.assertIn('Seek( (FileHandle_t)sp->m_hPackFile, 0, FILESYSTEM_SEEK_TAIL );', section)
        self.assertNotIn('Seek( ( FILE * )sp->m_hPackFile->m_pFile', section)
        self.assertNotIn('Trace_FClose( sp->m_hPackFile->m_pFile );', section)
        self.assertIn('if ( !sp->m_hPackFile->m_pFile )', section)
        self.assertIn('delete sp->m_hPackFile;', section)

    def test_pack_index_rejects_oversized_member_names(self):
        index = SOURCE[SOURCE.index('bool CBaseFileSystem::PreparePackFile('):SOURCE.index('void CBaseFileSystem::AddPackFiles(')]
        self.assertRegex(index, r'fileHeader\.fileNameLength\s*>=\s*sizeof\(tmpString\)')
        self.assertIn('MAX_FILES_IN_PACK', index)
        self.assertIn('65535', index)

    def test_real_zip_index_with_valid_and_corrupted_archives(self):
        # Exercise the actual parser and I/O methods rather than a reimplementation.
        bsp = (ROOT / 'public/bspfile.h').read_bytes().decode('latin1')
        types = bsp[bsp.index('#pragma pack(1)'):bsp.index('#pragma pack()')+len('#pragma pack()')]
        methods = '\n\n'.join(function(x) for x in [
            'void CBaseFileSystem::Seek(', 'unsigned int CBaseFileSystem::Tell(',
            'int CBaseFileSystem::Read(', 'bool CBaseFileSystem::PreparePackFile('])
        pre = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <string>
#include <vector>
#define VPROF_BUDGET(a,b)
#define VPROF_BUDGETGROUP_OTHER_FILESYSTEM 0
#define FILESYSTEM_WARNING 0
#define FILESYSTEM_SEEK_HEAD 0
#define FILESYSTEM_SEEK_CURRENT 1
#define FILESYSTEM_SEEK_TAIL 2
#define Assert(x) assert(x)
typedef int FileSystemSeek_t;
typedef void* FileHandle_t;
struct Table { std::string AddString(const char *p){ return std::string(p); } } g_PathIDTable;
typedef std::string CUtlSymbol;
struct FileStats { int nReads; unsigned int nBytesRead; FileStats():nReads(0),nBytesRead(0){} };
void strlwr(char *s) {while (*s) { *s=(char)tolower((unsigned char)*s); ++s;}}
''' + types + r'''
typedef struct {
 CUtlSymbol m_Name; int filepos; int filelen;
} TmpFileInfo_t;
class CBaseFileSystem {
public:
 struct CFileHandle {
   FILE *m_pFile; int m_nStartOffset, m_nLength, m_nPackPosition; bool m_bPack;
   CFileHandle():m_pFile(0),m_nStartOffset(0),m_nLength(0),m_nPackPosition(0),m_bPack(false){}
 };
 struct CPackFileEntry {CUtlSymbol m_Name; int m_nPosition,m_nLength;};
 struct PackEntries {std::vector<CPackFileEntry> items;void Insert(const CPackFileEntry &e){items.push_back(e);}};
 struct CSearchPath {bool m_bIsPackFile; CFileHandle *m_hPackFile;int m_nNumPackFiles;PackEntries m_PackFiles;
 CSearchPath():m_bIsPackFile(true),m_hPackFile(0),m_nNumPackFiles(0) {}};
 enum {MAX_FILES_IN_PACK=32768};
 FileStats m_Stats;
 int m_SeekCount;
 CBaseFileSystem():m_SeekCount(0) {}
 void Seek(FileHandle_t,int,FileSystemSeek_t);
 unsigned int Tell(FileHandle_t);
 int Read(void*,int,FileHandle_t);
 bool PreparePackFile(CSearchPath &,int,int);
 void FS_fseek(FILE *f,long p,int w){++m_SeekCount;std::fseek(f,p,w);}
 long FS_ftell(FILE *f){return std::ftell(f);}
 size_t FS_fread(void *p,size_t s,size_t n,FILE *f){return std::fread(p,s,n,f);}
 void Trace_FRead(int,FILE*) {}
 void Warning(int,const char*,...) {}
};
'''
        main = r'''
int main(int argc,char **argv) {
 assert(argc==5);
 FILE *f=std::fopen(argv[1],"rb"); assert(f);
 int base=std::atoi(argv[2]), length=std::atoi(argv[3]);
 int expected=std::atoi(argv[4]);
 CBaseFileSystem fs;
 CBaseFileSystem::CFileHandle backing;
 backing.m_pFile=f;
 CBaseFileSystem::CSearchPath path;
 path.m_hPackFile=&backing;
 bool ok=fs.PreparePackFile(path,base,length);
 assert(ok==(expected!=0));
 assert(fs.m_SeekCount<=66000*2+100);
 if(ok) {
  assert(path.m_PackFiles.items.size()==2);
  assert(path.m_nNumPackFiles==2);
  CBaseFileSystem::CFileHandle a,b;
  a.m_bPack=b.m_bPack=true;
  a.m_pFile=b.m_pFile=f;
  a.m_nStartOffset=path.m_PackFiles.items[0].m_nPosition;
  b.m_nStartOffset=path.m_PackFiles.items[1].m_nPosition;
  a.m_nLength=path.m_PackFiles.items[0].m_nLength;
  b.m_nLength=path.m_PackFiles.items[1].m_nLength;
  char buf[10]={0};
  assert(fs.Read(buf,2,&a)==2 && std::memcmp(buf,"HE",2)==0);
  assert(fs.Read(buf,5,&b)==5 && std::memcmp(buf,"WORLD",5)==0);
  assert(fs.Read(buf,10,&a)==3 && std::memcmp(buf,"LLO",3)==0);
 }
 std::fclose(f);
}
'''
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            raise unittest.SkipTest('No compiler')
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            inp, out = folder / 'zip_test.cpp', folder / 'zip_test'
            inp.write_text(pre + methods + main)
            proc = subprocess.run([cc, '-std=c++98', '-O1', '-g', '-Wall', '-Wextra',
                                   '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                   str(inp), '-o', str(out)], capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            import io
            buf=io.BytesIO()
            with zipfile.ZipFile(buf,'w',compression=zipfile.ZIP_STORED) as z:
                z.writestr('FIRST.BIN','HELLO')
                z.writestr('second.bin','WORLD')
            good=buf.getvalue()
            variants = [(good,0,len(good),1),(b'X'*7 + good+b'TRAILER',7,len(good),1)]
            broken=bytearray(good)
            loc=good.index(b'PK\x01\x02')
            broken[loc+28:loc+30]=(4000).to_bytes(2,'little')
            variants.append((bytes(broken),0,len(broken),0))
            broken=bytearray(good)
            broken[0:4]=b'BAD!'
            variants.append((bytes(broken),0,len(broken),0))
            broken=bytearray(good)
            end=good.rindex(b'PK\x05\x06')
            broken[end+16:end+20]=(0xFFFFFFFF).to_bytes(4,'little')
            variants.append((bytes(broken),0,len(broken),0))
            variants.append((b'bad-stream' * 100000,0,1000000,0))
            deflated=io.BytesIO()
            with zipfile.ZipFile(deflated,'w',compression=zipfile.ZIP_DEFLATED) as z:
                z.writestr('FIRST.BIN','HELLO'*200)
                z.writestr('second.bin','WORLD')
            variants.append((deflated.getvalue(),0,len(deflated.getvalue()),0))
            for n,(payload,offset,length,valid) in enumerate(variants):
                name=folder/f'archive{n}.zip'
                name.write_bytes(payload)
                result=subprocess.run([str(out),str(name),str(offset),str(length),str(valid)],
                                      capture_output=True,text=True)
                self.assertEqual(result.returncode,0, f'case {n}: '+result.stdout+result.stderr)


if __name__ == '__main__':
    unittest.main()
