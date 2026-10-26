"""Execute the shared HUD policy and replay the customization on original files."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FILES = ('gameui/GameConsoleDialog.cpp', 'cl_dll/hud_numericdisplay.h', 'gameui/CreateMultiplayerGameServerPage.cpp', 'gameui/OptionsDialog.cpp', 'cl_dll/hud.cpp', 'cl_dll/hud_numericdisplay.cpp',
         'cl_dll/hl2_hud/hud_battery.cpp', 'cl_dll/hl2_hud/hud_suitpower.h',
         'cl_dll/hl2_hud/hud_suitpower.cpp')


def extract_method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening+1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class HudCustomizationTests(unittest.TestCase):
    def test_shared_energy_color_and_background_policy(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable in PATH')
        actual = (ROOT/'cl_dll/hud_numericdisplay.cpp').read_bytes().decode('latin1')
        background = extract_method(actual, 'void CHudNumericDisplay::PaintBackground( void )')
        with tempfile.TemporaryDirectory() as td:
            folder = Path(td)
            (folder/'ConVar.h').write_text('''
class ConVar { public: int value; explicit ConVar(int v=0):value(v) {};
bool GetBool() const { return value!=0; } int GetInt() const { return value; } };
''')
            (folder/'Color.h').write_text('''
#ifndef TEST_COLOR_H
#define TEST_COLOR_H
class Color { unsigned char rgba[4]; public:
Color(int r,int g,int b,int a) { rgba[0]=(unsigned char)r;rgba[1]=(unsigned char)g;
rgba[2]=(unsigned char)b;rgba[3]=(unsigned char)a; }
unsigned char &operator[](int i) { return rgba[i]; }
unsigned char operator[](int i) const { return rgba[i]; }
};
#endif
''')
            harness = r'''
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include "hud_color_client.h"
ConVar hud_color_custom,hud_color_r,hud_color_g,hud_color_b;
class BackgroundPanel { public:
Color stored; int drawnAlpha; BackgroundPanel():stored(7,8,9,200),drawnAlpha(-1) {}
Color GetBgColor() const { return stored; } void SetBgColor(Color c) { stored=c; }
void PaintBackground() { drawnAlpha=stored[3]; }
};
class CHudNumericDisplay:public BackgroundPanel { public:
typedef BackgroundPanel BaseClass; float m_flAlphaOverride;
CHudNumericDisplay():m_flAlphaOverride(128.0f) {}
void PaintBackground(void);
};
'''+background+r'''
float special(unsigned int bits) { float value; memcpy(&value,&bits,sizeof(value)); return value; }
int main() {
using namespace HL2ModernHud;
assert(ColorByte(INT_MIN)==0 && ColorByte(-1)==0 && ColorByte(0)==0);
assert(ColorByte(127)==127 && ColorByte(255)==255 && ColorByte(256)==255 && ColorByte(INT_MAX)==255);
assert(EnergyValue(-10.0f)==0 && EnergyValue(0.0f)==0 && EnergyValue(0.49f)==0);
assert(EnergyValue(0.5f)==1 && EnergyValue(12.49f)==12 && EnergyValue(12.5f)==13);
assert(EnergyValue(99.49f)==99 && EnergyValue(99.5f)==100 && EnergyValue(100.0f)==100);
assert(sizeof(float)==sizeof(unsigned int));
assert(EnergyValue(FLT_MAX)==100);
assert(EnergyValue(special(0x7f800000u))==100);
assert(EnergyValue(special(0xff800000u))==0);
assert(EnergyValue(special(0x7fc00000u))==0);
Color original(1,2,3,128); Color unchanged=HL2ModernHudColor(original);
for(int i=0;i<4;++i) assert(unchanged[i]==original[i]);
hud_color_custom.value=1;hud_color_r.value=-40;hud_color_g.value=82;hud_color_b.value=900;
for(int alpha=0;alpha<=255;alpha+=255) {
Color overridden=HL2ModernHudColor(Color(1,2,3,alpha));
assert(overridden[0]==0 && overridden[1]==82 && overridden[2]==255 && overridden[3]==alpha);
}
assert(HL2ModernHudColor(original)[3]==128);
CHudNumericDisplay panel;
for(int frame=0;frame<5;++frame) {
panel.PaintBackground();assert(panel.drawnAlpha==100);assert(panel.GetBgColor()[3]==200);
}
panel.m_flAlphaOverride=255.0f;panel.PaintBackground();assert(panel.drawnAlpha==200);
panel.m_flAlphaOverride=0.0f;panel.PaintBackground();assert(panel.drawnAlpha==0);
assert(panel.GetBgColor()[3]==200);
}
'''
            cpp, executable = folder/'hud.cpp', folder/'hud.exe'
            cpp.write_text(harness)
            built = subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',
                '-I'+str(folder),'-I'+str(ROOT/'modernize'),str(cpp),'-o',str(executable)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran = subprocess.run([str(executable)],capture_output=True,text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)

    def test_customization_replays_on_original_sources_and_is_idempotent(self):
        import sys
        originals = {name:subprocess.check_output(['git','show','HEAD:'+name],cwd=ROOT) for name in FILES}
        with tempfile.TemporaryDirectory() as td:
            folder = Path(td)
            script = folder/'modernize/patch_hud_customization.py'
            script.parent.mkdir()
            shutil.copyfile(ROOT/'modernize/patch_hud_customization.py',script)
            for newline in ('\n','\r\n'):
                for name,data in originals.items():
                    target=folder/name;target.parent.mkdir(parents=True,exist_ok=True)
                    source=data.decode('latin1').replace('\r\n','\n')
                    target.write_bytes(source.replace('\n',newline).encode('latin1'))
                first=subprocess.run([sys.executable,str(script)],capture_output=True,text=True)
                self.assertEqual(first.returncode,0,first.stdout+first.stderr)
                generated={name:(folder/name).read_bytes() for name in FILES}
                second=subprocess.run([sys.executable,str(script)],capture_output=True,text=True)
                self.assertEqual(second.returncode,0,second.stdout+second.stderr)
                self.assertEqual(generated,{name:(folder/name).read_bytes() for name in FILES})
                numeric=generated['cl_dll/hud_numericdisplay.cpp'].decode('latin1')
                self.assertIn('HL2MODERN_THREE_COUNTERS',numeric)
                self.assertIn('255.0f',numeric)
                energy=generated['cl_dll/hl2_hud/hud_suitpower.cpp'].decode('latin1')
                self.assertIn('HL2ModernHud::EnergyValue(local->m_HL2Local.m_flSuitPower)',energy)
                self.assertIn('SetLabelText(L"Energy")',energy)


if __name__=='__main__':
    unittest.main()
