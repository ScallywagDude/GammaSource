"""Run real GammaSource fragment cleanup functions under a portable C++98 harness."""
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / 'engine/net_chan.cpp').read_text(encoding='latin1')

def function(signature):
    start = CPP.index(signature)
    first = CPP.index('{', start)
    depth = 0
    for pos in range(first, len(CPP)):
        if CPP[pos] == '{':
            depth += 1
        elif CPP[pos] == '}':
            depth -= 1
            if depth == 0:
                return CPP[start:pos+1]
    raise AssertionError('Unclosed C++ function: ' + signature)

class NetchanFragmentLifetimeTests(unittest.TestCase):
    def test_cleanup_and_unlink_under_undefined_behavior_sanitizer(self):
        preamble = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
static int liveFragments=0, liveWaiting=0, flushes=0;
struct fragbuf_t {
 fragbuf_t *next;
 fragbuf_t():next(0) {++liveFragments;}
 ~fragbuf_t() {--liveFragments;}
};
struct fragbufwaiting_t {
 fragbufwaiting_t *next;
 fragbuf_t *fragbufs;
 fragbufwaiting_t():next(0),fragbufs(0){++liveWaiting;}
 ~fragbufwaiting_t(){--liveWaiting;}
};
#define MAX_STREAMS 2
struct netchan_t {
 fragbufwaiting_t *waitlist[MAX_STREAMS];
 fragbuf_t *fragbufs[MAX_STREAMS];
 netchan_t() { std::memset(this,0,sizeof(*this)); }
};
void Con_Printf(const char*,...){}
void Netchan_FlushIncoming(netchan_t*,int){++flushes;}
'''
        test = r'''
int main(){
 netchan_t chan;
 fragbuf_t *a=new fragbuf_t;
 Netchan_UnlinkFragment(a, &chan.fragbufs[0]);
 assert(liveFragments==1);
 delete a;
 assert(liveFragments==0);
 fragbuf_t *h=new fragbuf_t;
 fragbuf_t *tail=new fragbuf_t;
 h->next=tail;
 chan.fragbufs[0]=h;
 Netchan_UnlinkFragment(tail,&chan.fragbufs[0]);
 assert(liveFragments==1 && chan.fragbufs[0]==h && h->next==0);
 Netchan_UnlinkFragment(h,&chan.fragbufs[0]);
 assert(!chan.fragbufs[0] && liveFragments==0);
 for(int s=0;s<MAX_STREAMS;s++){
   chan.fragbufs[s]=new fragbuf_t;
   fragbufwaiting_t *first=new fragbufwaiting_t;
   fragbufwaiting_t *second=new fragbufwaiting_t;
   first->next=second;
   first->fragbufs=new fragbuf_t;
   first->fragbufs->next=new fragbuf_t;
   second->fragbufs=new fragbuf_t;
   chan.waitlist[s]=first;
 }
 assert(liveFragments==8 && liveWaiting==4);
 Netchan_ClearFragments(&chan);
 assert(liveFragments==0 && liveWaiting==0 && flushes==MAX_STREAMS);
 for(int s=0;s<MAX_STREAMS;s++) assert(!chan.fragbufs[s] && !chan.waitlist[s]);
 Netchan_ClearFragments(&chan);
 assert(liveFragments==0 && liveWaiting==0);
 return 0;
}
'''
        functions = '\n\n'.join([
            function('void Netchan_UnlinkFragment('),
            function('void Netchan_ClearFragbufs('),
            function('void Netchan_ClearFragments('),
        ])
        # C++98 compilation and sanitizer execution against the original method bodies.
        code = preamble + '\n' + 'void Netchan_ClearFragbufs(fragbuf_t**);\n' + functions + '\n' + test
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp)/'netchan_smoke.cpp'; cpp.write_text(code)
            exe = Path(tmp)/'netchan_smoke'
            built = subprocess.run([shutil.which('clang++') or 'g++','-std=c++98','-O1','-g','-Wall','-Wextra','-Werror',
                                    '-fsanitize=address,undefined','-fno-sanitize-recover=all',
                                    str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stderr)
            run = subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_file_payload_sizing_compiles_and_handles_long_headers(self):
        helper = ROOT / 'engine/net_frag_safety.h'
        self.assertTrue(helper.is_file())
        code = r'''
#include <cassert>
#include "net_frag_safety.h"
int main(){
 assert(Netchan_FileFragmentPayloadBytes(100,512,20,1400)==100);
 assert(Netchan_FileFragmentPayloadBytes(600,512,20,1400)==492);
 assert(Netchan_FileFragmentPayloadBytes(600,263,262,1400)==1);
 assert(Netchan_FileFragmentPayloadBytes(1,263,262,1400)==1);
 assert(Netchan_FileFragmentPayloadBytes(2000,2000,100,1400)==1300);
 assert(Netchan_FileFragmentPayloadBytes(500,16,16,1400)==0);
 assert(Netchan_FileFragmentPayloadBytes(500,16,17,1400)==0);
 assert(Netchan_FileFragmentPayloadBytes(500,512,1400,1400)==0);
 assert(Netchan_FileFragmentPayloadBytes(-1,512,10,1400)==0);
 assert(Netchan_CanAppendFragmentBytes(100,200,300));
 assert(!Netchan_CanAppendFragmentBytes(100,201,300));
 assert(!Netchan_CanAppendFragmentBytes(0,-1,300));
 assert(!Netchan_CanAppendFragmentBytes(-1,0,300));
 assert(!Netchan_CanAppendFragmentBytes(2147483647,1,2147483647));
 for(int len=1;len<260;len++) {
   int chunk = 16>(len+2)?16:len+2;
   int left = 200000;
   int steps=0;
   while(left>0){
      int header=steps==0?len+1:0;
      int sent=Netchan_FileFragmentPayloadBytes(left,chunk,header,1400);
      assert(sent>0 && sent<=1400-header);
      left-=sent;
      assert(++steps<200001);
   }
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'frag.cpp';cpp.write_text(code)
            exe=Path(tmp)/'frag'
            build=subprocess.run(['g++','-std=c++98','-Wall','-Wextra','-Werror',
                                  '-I',str(ROOT/'engine'),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_network_io_and_file_receive_guards(self):
        self.assertNotIn('delete[] buf;',CPP)
        self.assertNotIn('vsprintf (string, format,argptr);',CPP)
        self.assertNotIn('send -= buf->frag_message.GetNumBytesWritten();',CPP)
        self.assertIn('!buf || !list || !*list',CPP)
        self.assertIn('Netchan_FileFragmentPayloadBytes( remaining, chunksize, headerBytes, FRAGMENT_SIZE )',CPP)
        self.assertIn('Q_strlen( receivedName ) >= sizeof( filename )',CPP)
        self.assertIn('filename[0] ==',CPP)
        self.assertIn('Netchan_CanAppendFragmentBytes( assembledBytes, fragBytes, net_message.maxsize )',CPP)
        self.assertIn('Netchan_CanAppendFragmentBytes( nsize, fragBytes - skipped, kMaxIncomingFileBytes )',CPP)
        self.assertIn('Q_strncpy( chan->incomingfilename, filename, sizeof( chan->incomingfilename ) )',CPP)

    def test_legacy_features_remain(self):
        self.assertTrue((ROOT/'gammasource_content/materials/sprites/hud1.vtf').is_file())
        self.assertTrue((ROOT/'engine/terrainmod_functions.cpp').is_file())
        self.assertTrue((ROOT/'game_shared/hl2/weapon_smg1.cpp').is_file() or
                        (ROOT/'dlls/weapon_smg1.cpp').is_file() or
                        any(ROOT.glob('**/weapon_smg1.cpp')))

if __name__=='__main__':
    unittest.main()
