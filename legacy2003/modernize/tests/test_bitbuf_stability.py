"""Exercise real legacy bit-buffer method bodies with minimal C++98 platform shims."""
from pathlib import Path
import hashlib
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / 'public/bitbuf.cpp').read_text(encoding='latin1')
HEADER = (ROOT / 'public/bitbuf.h').read_text(encoding='latin1')


def method(source, signature):
    marker = source.index(signature)
    brace = source.index('{', marker)
    level = 0
    for end in range(brace, len(source)):
        if source[end] == '{':
            level += 1
        elif source[end] == '}':
            level -= 1
            if level == 0:
                return source[marker:end+1]
    raise AssertionError('Unclosed C++ function: ' + signature)


class BitBufferStabilityTests(unittest.TestCase):
    def test_beta_files_untouched(self):
        self.assertEqual(hashlib.sha256((ROOT/'gammasource_content/materials/sprites/hud1.vtf').read_bytes()).hexdigest(),
                         'd93f0d907ba35fde2a9de0c23642b8976c988ebee89936337704a62e00362ed9')
        self.assertTrue((ROOT/'engine/terrainmod_functions.cpp').is_file())
        self.assertIn('terrainmod', (ROOT/'game_shared/tf2/tf_gamerules.cpp').read_text(encoding='latin1'))

    def test_legacy_buffer_behaviour_compiles_and_runs(self):
        methods = [
            method(CPP,'void bf_read::StartReading('),
            method(CPP,'bool bf_read::Seek('),
            method(CPP,'void bf_read::ExciseBits('),
            method(HEADER,'inline bool bf_read::CheckForOverflow('),
            method(HEADER,'inline bool\tbf_read::SeekRelative('),
            method(HEADER,'inline unsigned int bf_read::ReadUBitLong('),
            method(CPP,'void bf_write::StartWriting('),
            method(CPP,'void bf_write::SeekToBit('),
            method(HEADER,'inline bool bf_write::CheckForOverflow('),
        ]
        beginning = r'''
#include <climits>
#include <cassert>
#include <cstring>
#define Assert(v) assert(v)
#define CallErrorHandler(a,b) ((void)0)
#define BITBUFERROR_BUFFER_OVERRUN 1
unsigned long g_ExtraMasks[32];
class bf_read {
public:
 const unsigned char *m_pData;
 int m_nDataBytes, m_nDataBits, m_iCurBit;
 bool m_bOverflow, m_bAssertOnOverflow;
 bf_read() : m_pData(0),m_nDataBytes(0),m_nDataBits(0),m_iCurBit(0),m_bOverflow(false),m_bAssertOnOverflow(false) {}
 void StartReading(const void*, int, int, int);
 bool Seek(int);
 bool SeekRelative(int);
 void ExciseBits(int,int);
 bool CheckForOverflow(int);
 unsigned int ReadUBitLong(int);
 void SetOverflowFlag() {m_bOverflow=true;}
 bool IsOverflowed() const {return m_bOverflow;}
 const char *GetDebugName() const{return "test";}
};
class bf_write {
public:
 unsigned char *m_pData;
 int m_nDataBytes, m_nDataBits, m_iCurBit;
 bool m_bOverflow, m_bAssertOnOverflow;
 bf_write() : m_pData(0),m_nDataBytes(0),m_nDataBits(0),m_iCurBit(0),m_bOverflow(false),m_bAssertOnOverflow(false) {}
 void StartWriting(void*,int,int,int);
 void SeekToBit(int);
 bool CheckForOverflow(int);
 void SetOverflowFlag() {m_bOverflow=true;}
 bool IsOverflowed() const {return m_bOverflow;}
 const char *GetDebugName() const{return "test";}
};
'''
        testmain = r'''
static unsigned expected(const unsigned char* buf, int off, int len) {
 unsigned value=0;
 for(int i=0;i<len;i++)value |= ((unsigned)((buf[(off+i)>>3]>>((off+i)&7))&1u))<<i;
 return value;
}
int main() {
 for (int i=0;i<32;i++)g_ExtraMasks[i] = (1u<<i)-1;
 alignas(4) unsigned char packet[16]={0xf0,0x0f,0x8e,0x76,0};
 bf_read rd;
 rd.StartReading(packet, 3, 0, 24);
 assert(!rd.IsOverflowed());
 // Last dword crosses the 3-byte allocation; the guarded byte reader is used.
 unsigned check=expected(packet,7,17);
 assert(rd.Seek(7));
 assert(rd.ReadUBitLong(17)==check);
 assert(rd.m_iCurBit==24);
 assert(rd.ReadUBitLong(1)==0 && rd.IsOverflowed());
 rd.StartReading(packet, 4, 0, 32);
 assert(rd.Seek(0));
 assert(rd.ReadUBitLong(32)==0x768e0ff0u);
 rd.StartReading(packet, 3, 0, 24);
 assert(rd.ReadUBitLong(-1)==0 && rd.IsOverflowed());
 rd.StartReading(packet, 3, 0, 24);
 assert(rd.ReadUBitLong(33)==0 && rd.IsOverflowed());
 rd.StartReading(packet, 3, 0, 24);
 assert(!rd.SeekRelative(INT_MAX) && rd.IsOverflowed());
 rd.StartReading(packet, 3, 0, 24);
 assert(!rd.SeekRelative(INT_MIN) && rd.IsOverflowed());
 rd.StartReading(packet, -1, 0, -1);
 assert(rd.IsOverflowed() && rd.m_nDataBits==0);
 rd.StartReading(packet, 3, 0, 25);
 assert(rd.IsOverflowed() && rd.m_nDataBits==0);
 rd.StartReading(packet, 3, -2, 24);
 assert(rd.IsOverflowed());
 rd.StartReading(packet, 3, 0, 24);
 assert(rd.CheckForOverflow(INT_MAX) && rd.IsOverflowed());
 // Verify bit-for-bit excision, including a non-byte-aligned range.
 rd.StartReading(packet, 3, 0, 24);
 unsigned char original[4]={0}; std::memcpy(original,packet,4);
 int keep=19;
 int before[19];
 for (int i=0;i<keep;i++){
  int idx=i<3?i:i+5;
  before[i]=(original[idx>>3]>>(idx&7))&1;
 }
 rd.ExciseBits(3,5);
 assert(rd.m_nDataBits==19 && rd.m_nDataBytes==3 && rd.m_iCurBit==3);
 for(int i=0;i<keep;i++)assert(((packet[i>>3]>>(i&7))&1)==before[i]);
 unsigned char snapshot[4]={0};std::memcpy(snapshot,packet,4);
 rd.ExciseBits(20,1);assert(rd.IsOverflowed());
 assert(std::memcmp(packet,snapshot,4)==0);
 rd.StartReading(packet, 3, 0, 19);
 rd.ExciseBits(0,0);assert(!rd.IsOverflowed());
 bf_write wr;
 alignas(4) unsigned char writable[8]={0};
 wr.StartWriting(writable,8,0,64);
 assert(!wr.IsOverflowed());
 wr.SeekToBit(70);assert(wr.IsOverflowed() && wr.m_iCurBit==64);
 wr.StartWriting(writable,8,0,64);
 assert(wr.CheckForOverflow(INT_MAX) && wr.IsOverflowed());
 wr.StartWriting(writable,8,0,64);
 assert(wr.CheckForOverflow(-1) && wr.IsOverflowed());
 wr.StartWriting(writable,8,0,65);assert(wr.IsOverflowed());
 return 0;
}
'''
        # alignas is C++11; Win32 production methods themselves remain C++98.
        testmain=testmain.replace('alignas(4) ', '')
        code=beginning+'\n\n'.join(methods)+'\n'+testmain
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/'bitbuf_smoke.cpp';file.write_text(code)
            exe=Path(temp)/'bitbuf_smoke'
            build=subprocess.run(['g++','-std=c++98','-Wall','-Wextra','-Werror','-O1',
                                  '-fsanitize=undefined','-fno-sanitize-recover=undefined',str(file),'-o',str(exe)],
                                 capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+'\n'+run.stderr)


if __name__=='__main__':
    unittest.main()
