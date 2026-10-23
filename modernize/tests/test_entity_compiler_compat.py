"""Execute compatibility fixtures against the actual rewritten entity macros."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
import patch_entity_compiler_compat as patcher


class EntityCompilerCompatibilityTests(unittest.TestCase):
    def native(self,source,extra=()):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('portable native compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory); unit=path/'test.cpp'
            binary=path/('test.exe' if os.name=='nt' else 'test')
            unit.write_text(source,encoding='utf-8')
            build=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                '-I',str(ROOT),str(unit),'-o',str(binary)]+list(extra),capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_callback_rewrite_preserves_comments_literals_and_existing_pointers(self):
        before='// SetThink(C::Fake)\r\nconst char *s="SetUse(C::Fake)";\r\n'
        before+='SetThink(C::Think); SetThink(&C::Think); SetTouch(NULL); SetThink(callback);\r\n'
        before+='SetThink(BurstThink); // caf\xe9\r\n'
        after=patcher.callbacks(before)
        self.assertEqual(after,before.replace('SetThink(C::Think)','SetThink(&C::Think)')
                         .replace('SetThink(BurstThink)','SetThink(&ThisClass::BurstThink)'))
        self.assertEqual(after,patcher.callbacks(after))
        self.assertEqual(after.count('\r\n'),before.count('\r\n'))

    def test_member_callbacks_execute_derived_member_and_clear_to_null(self):
        self.native(patcher.callbacks(r'''
#include <assert.h>
class CBaseEntity {
public:
 typedef void (CBaseEntity::*ThinkFn)();
 ThinkFn think;
 void ThinkSet(ThinkFn fn,int,void*) { think=fn; }
 void Run() { if(think) (this->*think)(); }
};
#define SetThink(a) ThinkSet(static_cast<void(CBaseEntity::*)()>(a),0,0)
class Derived: public CBaseEntity {
public:
 typedef Derived ThisClass;
 int calls;
 Derived(): calls(0) { SetThink(Derived::BurstThink); }
 void BurstThink() { ++calls; }
 void Reset() { SetThink(0); }
 void Resume() { SetThink(BurstThink); }
};
int main() { Derived value; value.Run(); assert(value.calls==1); value.Reset();
 value.Run(); assert(value.calls==1); value.Resume(); value.Run(); assert(value.calls==2); }
'''))

    def test_packet_component_offsets_match_real_storage(self):
        macros='\n'.join(line for line in (ROOT/'public/dt_send.h').read_text().splitlines()
                         if line.startswith('#define SENDINFO'))
        macros+='\n'+'\n'.join(line for line in (ROOT/'public/dt_recv.h').read_text().splitlines()
                               if line.startswith('#define RECVINFO'))
        self.native(r'''
#include <stddef.h>
#include <assert.h>
#include <string.h>
struct Vec { float x,y,z; float& operator[](int i) { return (&x)[i]; }
 const float& operator[](int i) const { return (&x)[i]; } };
struct Array { unsigned char m_Value[3];
 const unsigned char& operator[](int i) const { return m_Value[i]; } };
struct Vector { Vec m_Value; float operator[](int i) const { return m_Value[i]; } };
struct Entity { int prefix; Array color; Vector rotation; Vec plain;
 typedef Entity MakeANetworkVar_color; typedef Entity MakeANetworkVar_rotation; };
typedef Entity currentSendDTClass;
typedef Entity currentRecvDTClass;
struct Info { const char *name; size_t offset,size; };
Info info(const char *name,size_t offset,size_t size) { Info i={name,offset,size}; return i; }
'''+macros+r'''
int main() { Entity e;
 Info color=info(SENDINFO_ARRAYELEM(color,2));
 assert(color.offset==(size_t)((char*)&e.color.m_Value[2]-(char*)&e));
 assert(color.size==1 && strcmp(color.name,"color[2]")==0);
 Info angle=info(SENDINFO_VECTORELEM(rotation,1));
 assert(angle.offset==(size_t)((char*)&e.rotation.m_Value.y-(char*)&e));
 assert(angle.size==sizeof(float));
 Info plain=info(SENDINFO_NOCHECK(plain[2]));
 assert(plain.offset==(size_t)((char*)&e.plain.z-(char*)&e));
 Info received=info(RECVINFO(plain[2]));
 assert(received.offset==plain.offset && received.size==plain.size);
}
''')

    def test_input_keys_and_engine_map_bridge_keep_caller_memory_immutable(self):
        self.native(r'''
#include "modernize/legacy_entity_compat.h"
#include <assert.h>
#include <string.h>
struct Engine { int calls; int IsMapValid(char *name) { ++calls; name[0]='X'; return 1; } };
int main() { using namespace HL2ModernEntityCompat;
 char key[8]; const char *original="target#2";
 assert(CopyKeyWithoutSuffix(original,key,sizeof(key)) && strcmp(key,"target")==0);
 assert(strcmp(original,"target#2")==0);
 assert(!CopyKeyWithoutSuffix("verylongkey",key,sizeof(key)) && key[0]==0);
 Engine engine={0}; const char *map="d1_trainstation_01";
 assert(IsMapValid(&engine,map) && strcmp(map,"d1_trainstation_01")==0);
 char longName[257]; memset(longName,'x',256); longName[256]=0;
 assert(!IsMapValid(&engine,longName) && engine.calls==1);
 assert(!IsMapValid(&engine,(const char*)0)); }
''')

    def test_formatting_uses_real_variadic_parameters_and_explicit_forwarding(self):
        self.native(r'''
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
static int Q_vsnprintf(char *out,int length,const char *format,va_list args)
{ return vsnprintf(out,length,format,args); }
#include "public/fmtstr.h"
static void Forward(CFmtStr& out,const char *format,...) {
 va_list args; va_start(args,format); out.VSprintf(format,args); va_end(args); }
int main() { CFmtStr a("%s %d %.1f","value",42,2.5);
 assert(strcmp(a,"value 42 2.5")==0); a.sprintf("%d",-5); assert(strcmp(a,"-5")==0);
 Forward(a,"%s/%d","forward",9); assert(strcmp(a,"forward/9")==0);
 CFmtStrN<8> small("%s","abcdefghijkl"); assert(strlen(small)<=7);
 const char *format="%s:%d"; CFmtStr b(&format,"pointer",8);
 assert(strcmp(b,"pointer:8")==0); }
''')

    def test_vgui_message_map_dispatches_member_handlers(self):
        macros='\n'.join(line for line in (ROOT/'public/vgui/MessageMap.h').read_text().splitlines()
                         if line.startswith('#define MAP_MESSAGE(') or line.startswith('#define MAP_MESSAGE_INT('))
        self.native(r'''
#include <assert.h>
namespace vgui {
 class Panel {};
 typedef void (Panel::*MessageFunc_t)();
 enum { DATATYPE_INT=1 };
}
struct Item { const char *name; int id; vgui::MessageFunc_t fn; int count;
 int type; const char *argument; };
class Widget: public vgui::Panel {
public: int value; Widget():value(0){} void Click(){++value;} void Set(int i){value=i;}
};
'''+macros+r'''
int main() {
 Item click=MAP_MESSAGE(Widget,"click",Click);
 Item set=MAP_MESSAGE_INT(Widget,"set",Set,"value");
 Widget widget; (widget.*click.fn)(); assert(widget.value==1);
 typedef void (vgui::Panel::*IntFn)(int);
 IntFn fn=reinterpret_cast<IntFn>(set.fn); (widget.*fn)(8); assert(widget.value==8);
}
''',('-Wno-missing-field-initializers',))


if __name__=='__main__':unittest.main()
