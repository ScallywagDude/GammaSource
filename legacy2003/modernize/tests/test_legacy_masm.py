"""Original assembly builds stop on errors and never publish partial objects."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'modernize'))
import run_legacy_masm as runner


class LegacyMasmTests(unittest.TestCase):
    def run_assembly(self,failure=None):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);source=work/'sound.s';output=work/'sound.obj'
            source.write_text('.text\n');output.write_bytes(b'previous valid object')
            commands=[]
            def execute(command,cwd,input_bytes=None):
                commands.append(command)
                index=len(commands)
                if failure==index:raise ValueError('tool failure')
                if index==3:
                    return subprocess.CompletedProcess(command,0,b'.386\nEND\n',b'Error: rejected\n' if failure=='translator' else b'')
                if index==4:
                    Path(next(arg[3:] for arg in command if arg.startswith('/Fo'))).write_bytes(b'fresh COFF object')
                return subprocess.CompletedProcess(command,0,b'preprocessed source',b'')
            with mock.patch.object(runner.os,'name','nt'), \
                 mock.patch.object(runner.shutil,'which',side_effect=lambda tool:tool), \
                 mock.patch.object(runner,'checked',side_effect=execute):
                if failure:
                    with self.assertRaises(ValueError):runner.assemble(source,output,root=ROOT)
                    self.assertEqual(output.read_bytes(),b'previous valid object')
                else:
                    self.assertEqual(runner.assemble(source,output,root=ROOT),output)
                    self.assertEqual(output.read_bytes(),b'fresh COFF object')
                    self.assertIn('/TC',commands[0]);self.assertIn('/EP',commands[1])
                    self.assertIn('/coff',commands[3]);self.assertIn('/safeseh',commands[3])
                    self.assertTrue(str(commands[2][0]).endswith('gas2masm.exe'))
            self.assertEqual(list(work.glob('*-asm-*')),[])

    def test_translator_rebuilt_and_object_published_after_success(self):self.run_assembly()
    def test_tool_failure_retains_previous_object(self):
        for stage in (1,2,3,4):
            with self.subTest(stage=stage):self.run_assembly(stage)
    def test_legacy_translator_zero_exit_error_is_rejected(self):self.run_assembly('translator')

    @unittest.skipUnless(os.name=='nt' and os.environ.get('VSCMD_ARG_TGT_ARCH')=='x86'
                         and shutil.which('cl') and shutil.which('ml'),
                         'original sound assembly execution requires x86 Native Tools')
    def test_original_sound_assembly_matches_portable_reference(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);unit=work/'sound_test.cpp';binary=work/'sound_test.exe'
            obj=runner.assemble(ROOT/'engine/audio/private/snd_mixa.s',work/'sound.obj')
            unit.write_text(r'''
#include <assert.h>
struct Sample { int left,right; };
extern "C" {
int snd_scaletable[128][256]; Sample paintbuffer[32];
int snd_linear_count,*snd_p,snd_vol; short *snd_out;
void SND_PaintChannelFrom8(int*,unsigned char*,int);
void Snd_WriteLinearBlastStereo16();
}
int main() {
 for(int volume=0;volume<128;++volume)for(int sample=0;sample<256;++sample)
  snd_scaletable[volume][sample]=(sample<128?sample:sample-256)*volume*2;
 unsigned char samples[9]={0,1,127,128,255,13,240,64,192};
 int levels[2]={43,250};
 for(int count=1;count<=9;++count) {
  for(int i=0;i<32;++i){paintbuffer[i].left=100+i;paintbuffer[i].right=-100-i;}
  SND_PaintChannelFrom8(levels,samples,count);
  for(int i=0;i<32;++i) {
   assert(paintbuffer[i].left==100+i+(i<count?snd_scaletable[levels[0]>>1][samples[i]]:0));
   assert(paintbuffer[i].right==-100-i+(i<count?snd_scaletable[levels[1]>>1][samples[i]]:0));
  }
 }
 int input[8]={0,1,-1,32767,-32768,100000,-100000,8192};short output[10];
 snd_p=input;snd_out=output+1;snd_linear_count=8;
 for(int volume=1;volume<=256;volume*=2) {
  snd_vol=volume;output[0]=12345;output[9]=-12345;Snd_WriteLinearBlastStereo16();
  for(int i=0;i<8;++i){int expected=(input[i]*volume)>>8;
   if(expected>32767)expected=32767;if(expected<-32768)expected=-32768;
   assert(output[i+1]==expected);}
  assert(output[0]==12345 && output[9]==-12345);
 }
}
''',encoding='ascii')
            built=subprocess.run(['cl','/nologo','/EHsc','/MT','/W4','/WX',str(unit),str(obj),
                '/Fo'+str(work/'test.obj'),'/Fe'+str(binary)],cwd=work,capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)


if __name__=='__main__':unittest.main()
