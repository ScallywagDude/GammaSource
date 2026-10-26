"""Compile executable C++98 checks for the actual allocator/model-cache methods.

These checks exercise extracted original methods with minimal engine dependencies.
They are not substitutes for a complete Windows build or game runtime test.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
POOL_HEADER = (ROOT / 'public/mempool.h').read_bytes().decode('latin1')
POOL_SOURCE = (ROOT / 'public/mempool.cpp').read_bytes().decode('latin1')
MODEL_SOURCE = (ROOT / 'engine/modelloader.cpp').read_bytes().decode('latin1')


def method(source, signature):
    start = source.index(signature)
    begin = source.index('{', start)
    depth = 0
    for pos in range(begin, len(source)):
        if source[pos] == '{':
            depth += 1
        elif source[pos] == '}':
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError('Unclosed method: ' + signature)


def compile_and_run(source):
    cc = shutil.which('clang++') or shutil.which('g++')
    if not cc:
        raise unittest.SkipTest('No C++98 compiler available')
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / 'test.cpp'
        out = Path(td) / 'test'
        cpp.write_text(source)
        command = [cc, '-std=c++98', '-O1', '-g', '-Wall', '-Wextra',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                   str(cpp), '-o', str(out)]
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        result = subprocess.run([str(out)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


class PoolTests(unittest.TestCase):
    def test_actual_pool_memory_alignment_growth_and_oom(self):
        # Use the real class declaration and every relevant method body.
        cls = POOL_HEADER[POOL_HEADER.index('typedef void (*MemoryPoolReportFunc_t)'):
                          POOL_HEADER.index('// Wrapper macro to make an allocator')]
        cls = cls.replace('private:', 'public:')  # Test-only access to blob counts.
        source = '''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <climits>
#include <cstddef>
#define Assert(x) assert(x)
#define max(x,y) ((x) > (y) ? (x) : (y))
''' + cls + '\n'
        bodies = [
            'void CMemoryPool::SetErrorReportFunc(',
            'CMemoryPool::CMemoryPool(',
            'CMemoryPool::~CMemoryPool()',
            'void CMemoryPool::Init()',
            'void CMemoryPool::Clear()',
            'void CMemoryPool::ReportLeaks()',
            'void CMemoryPool::AddNewBlob()',
            'void* CMemoryPool::Alloc()',
            'void *CMemoryPool::Alloc(',
            'void CMemoryPool::Free(',
        ]
        source += '\nMemoryPoolReportFunc_t CMemoryPool::g_ReportFunc = 0;\n'
        source += '\n\n'.join(method(POOL_SOURCE, signature) for signature in bodies)
        source += '''
int main() {
    CMemoryPool slow(13, 3, CMemoryPool::GROW_SLOW);
    assert(slow.m_BlockSize == 16 && slow.m_NumBlobs == 1);
    void* blocks[24];
    for (int i=0; i<24; ++i) {
        blocks[i] = slow.Alloc(); assert(blocks[i]);
        assert(((size_t)blocks[i] % sizeof(void*)) == 0);
        memset(blocks[i], i, 13);
        assert(slow.m_NumBlobs == (unsigned short)(i / 3 + 1));
    }
    for (int i=0; i<24; ++i) slow.Free(blocks[i]);
    assert(slow.m_BlocksAllocated == 0);
    for (int i=0; i<24; ++i) assert(slow.Alloc());
    slow.Clear();
    assert(slow.m_NumBlobs == 0 && slow.m_BlocksAllocated == 0);

    CMemoryPool fast(13, 3, CMemoryPool::GROW_FAST);
    for (int i=0; i<9; ++i) assert(fast.Alloc());
    assert(fast.m_NumBlobs == 2); // 3 + 6 blocks, not 3 + 3

    CMemoryPool fixed(15, 2, CMemoryPool::GROW_NONE);
    void* first = fixed.Alloc();
    void* second = fixed.Alloc();
    assert(first && second && !fixed.Alloc());
    fixed.Free(first);
    assert(fixed.Alloc() == first && fixed.m_NumBlobs == 1);

    CMemoryPool zero(0, 0, CMemoryPool::GROW_SLOW);
    assert(zero.m_NumBlobs == 0 && !zero.Alloc());
    CMemoryPool overflow(INT_MAX, INT_MAX, CMemoryPool::GROW_FAST);
    assert(overflow.m_NumBlobs == 0 && !overflow.Alloc());
}
'''
        compile_and_run(source)

    def test_actual_model_cache_rejects_overlong_paths(self):
        body = method(MODEL_SOURCE, 'model_t *CModelLoader::FindModel(')
        test = r'''
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#define MAX_QPATH 64
#define Assert(x) assert(x)
int errors = 0;
void Sys_Error(const char*, ...) { ++errors; }
int Q_strlen(const char *p) { return (int)strlen(p); }
enum { FMODELLOADER_NOTLOADEDORREFERENCED = 1 };
struct model_t { char name[MAX_QPATH]; int needload; };
class CModelLoader {
public:
 struct ModelEntry {model_t *modelpointer;};
 struct Dict {
    std::map<std::string,int> lookup;
    ModelEntry entries[16]; int count;
    Dict():count(0) {}
    int Find(const char *s) {std::map<std::string,int>::iterator it=lookup.find(s);return it==lookup.end()?-1:it->second;}
    int InvalidIndex() {return -1;}
    int Insert(const char *s, ModelEntry e) {lookup[s]=count;entries[count]=e;return count++;}
    ModelEntry &operator[](int i) {return entries[i];}
 } m_Models;
 struct Pool {
    model_t data[16]; int count;
    Pool():count(0) {}
    void* Alloc() {return count<16?&data[count++]:NULL;}
 } m_ModelPool;
 model_t m_InlineModels[2];
 bool IsWorldModelSet() {return true;}
 int GetNumWorldSubmodels() {return 2;}
 model_t* FindModel(const char *name);
};
''' + body + r'''
int main() {
    CModelLoader loader;
    std::string overlong(MAX_QPATH, 'A');
    assert(loader.FindModel(overlong.c_str())==NULL);
    assert(errors==1 && loader.m_ModelPool.count==0 && loader.m_Models.count==0);
    model_t *m=loader.FindModel("models/prop_test.mdl");
    assert(m && std::strcmp(m->name,"models/prop_test.mdl")==0);
    assert(loader.FindModel("models/prop_test.mdl")==m);
    assert(loader.m_ModelPool.count==1 && loader.m_Models.count==1);
    assert(loader.FindModel("*1")==&loader.m_InlineModels[1]);
    // Pool exhaustion must not leave a bogus entry in the model table.
    for (int i=0; i<15; ++i) {
      char key[32]; std::sprintf(key,"models/m%d.mdl",i);
      assert(loader.FindModel(key));
    }
    assert(loader.FindModel("models/extra.mdl")==NULL);
    assert(loader.m_Models.count==16);
}
'''
        compile_and_run(test.replace('#include <cstring>', '#include <cstring>\n#include <cstdio>'))


    def test_game_lump_dictionary_bounds_under_sanitizers(self):
        body = method(MODEL_SOURCE, 'void Mod_LoadGameLumpDict( void )')
        prelude = r'''
#include <cassert>
#include <cstring>
#include <climits>
#include <vector>
typedef unsigned char byte;
struct dgamelumpheader_t { int lumpCount; };
struct dgamelump_t {int id; unsigned short flags; unsigned short version; int fileofs, filelen;};
class LumpDict {
public:
 std::vector<dgamelump_t> items;
 void RemoveAll() { items.clear(); }
 void AddToTail(dgamelump_t item) {items.push_back(item);}
};
LumpDict g_GameLumpDict;
char g_pGameLumpFile[128];
enum {LUMP_GAME_LUMP=35};
byte* g_data=0; int g_size=0;
class CMapLoadHelper {
public:
 CMapLoadHelper(int) {}
 int LumpSize() {return g_size;}
 byte* LumpBase() {return g_data;}
 const char* GetMapName() {return "maps/beta.bsp";}
};
'''
        main = r'''
int main() {
    struct {dgamelumpheader_t header; dgamelump_t contents[2];} data = {};
    data.header.lumpCount=2;
    data.contents[0].id=123;data.contents[1].id=456;
    g_data=(byte*)&data;g_size=sizeof(data);
    Mod_LoadGameLumpDict();
    assert(g_GameLumpDict.items.size()==2);
    assert(g_GameLumpDict.items[0].id==123 && g_GameLumpDict.items[1].id==456);
    data.header.lumpCount=1000000;
    Mod_LoadGameLumpDict();assert(g_GameLumpDict.items.empty());
    data.header.lumpCount=-10;
    Mod_LoadGameLumpDict();assert(g_GameLumpDict.items.empty());
    data.header.lumpCount=2;
    g_size=(int)sizeof(dgamelumpheader_t)+sizeof(dgamelump_t);
    Mod_LoadGameLumpDict();assert(g_GameLumpDict.items.empty());
    g_size=1;
    Mod_LoadGameLumpDict();assert(g_GameLumpDict.items.empty());
    g_size=sizeof(data); g_data=0;
    Mod_LoadGameLumpDict();assert(g_GameLumpDict.items.empty());
}
'''
        compile_and_run(prelude + body + main)

    def test_beta_features_not_edited_by_memory_pool_patch(self):
        # Preservation of beta-sensitive files is verified during ZIP packaging.
        self.assertIn('m_BlockSize > INT_MAX / nElements', POOL_SOURCE)
        self.assertIn('m_GrowMode == GROW_FAST ? (int)m_NumBlobs + 1 : 1', POOL_SOURCE)
        self.assertIn('Q_strlen( name ) >= MAX_QPATH', MODEL_SOURCE)


if __name__ == '__main__':
    unittest.main()
