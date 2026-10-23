"""Run the actual HL1-style bucket painter with instrumented surface/icon stubs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method
ROOT=Path(__file__).resolve().parents[2]
HARNESS=r'''
#include <cassert>
#include "modernize/hl1_weapon_buckets.h"
#include "modernize/weapon_hud_layout.h"
struct Color{int rgba[4];Color(int r=255,int g=160,int b=0,int a=255){rgba[0]=r;rgba[1]=g;rgba[2]=b;rgba[3]=a;}int&operator[](int n){return rgba[n];}};
enum{MAX_WEAPON_SLOTS=6,MAX_WEAPON_POSITIONS=20};
int screenWidth=640,screenHeight=480,iconDraws=0,selectedOutlines=0,missingLabels=0,localized=0,converted=0;
namespace vgui {
struct Localize {
 const wchar_t*Find(const char*name){if(name[0]=='#'){++localized;return L"LOCALIZED SMG";}return 0;}
 void ConvertANSIToUnicode(const char*name,wchar_t*out,unsigned int bytes){++converted;unsigned int i=0;for(;name[i]&&i+1<bytes/sizeof(wchar_t);++i)out[i]=(unsigned char)name[i];out[i]=0;}
}localizeObject;Localize*localize(){return &localizeObject;}
}
struct Surface{
 void DrawSetColor(int,int,int,int){}void DrawSetColor(Color){}void DrawFilledRect(int x,int y,int right,int bottom){assert(x>=0&&y>=0&&right<=screenWidth&&bottom<=screenHeight);}
 void DrawOutlinedRect(int,int,int,int){++selectedOutlines;}void DrawSetTextColor(Color){}void DrawSetTextFont(int){}void DrawSetTextPos(int,int){}
 void DrawUnicodeChar(wchar_t){++missingLabels;}int GetCharacterWidth(int,wchar_t){return 7;}
}surfaceObject;Surface*surface(){return &surfaceObject;}
struct CHudTexture{
 int width,height;CHudTexture():width(256),height(64){}int Width()const{return width;}int Height()const{return height;}
 void DrawSelf(int x,int y,int w,int h,Color)const{assert(x>=0&&y>=0&&w<=width&&h<=height&&x+w<=screenWidth&&y+h<=screenHeight);++iconDraws;}
};
struct FileWeaponInfo_t{const char*szPrintName;FileWeaponInfo_t():szPrintName("LONG PROTOTYPE WEAPON NAME THAT MUST FIT"){}};
struct C_BaseCombatWeapon{int slot;bool ammo,icon;CHudTexture sprite;FileWeaponInfo_t data;C_BaseCombatWeapon():slot(2),ammo(true),icon(true){}
 int GetSlot(){return slot;}bool HasAnyAmmo(){return ammo;}const CHudTexture*GetSpriteActive(){return icon?&sprite:0;}const CHudTexture*GetSpriteInactive(){return icon?&sprite:0;}const FileWeaponInfo_t&GetWpnData(){return data;}
};
struct C_BasePlayer{static C_BasePlayer*local;static C_BasePlayer*GetLocalPlayer(){return local;}};C_BasePlayer*C_BasePlayer::local=0;
struct HUD{Color m_clrNormal,m_clrCaution;HUD():m_clrNormal(200,140,35),m_clrCaution(255,30,20){}}gHUD;
struct CHudWeaponSelection{
 bool draw;C_BaseCombatWeapon*selected,*m_hLastPickedUpWeapon,*weapons[6][20];float m_flSelectionAlphaOverride;int m_hTextFont,headers;
 CHudWeaponSelection():draw(true),selected(0),m_hLastPickedUpWeapon(0),m_flSelectionAlphaOverride(255),m_hTextFont(0),headers(0){for(int i=0;i<6;++i)for(int p=0;p<20;++p)weapons[i][p]=0;}
 bool ShouldDraw(){return draw;}int GetWide(){return screenWidth;}int GetTall(){return screenHeight;}C_BaseCombatWeapon*GetSelectedWeapon(){return selected;}
 C_BaseCombatWeapon*GetWeaponInSlot(int slot,int pos){return weapons[slot][pos];}
 void DrawBox(int x,int y,int w,int h,Color,float,int number){assert(x>=0&&y>=0&&x+w<=screenWidth&&y+h<=screenHeight);if(number>0)++headers;}
 void Paint();
};
'''
SCENARIOS=r'''
int main(){
 int resolutions[][2]={{320,240},{640,480},{1280,720},{1920,1080}};
 for(int r=0;r<4;++r)for(int active=0;active<6;++active){
  screenWidth=resolutions[r][0];screenHeight=resolutions[r][1];
  HL2ModernHud::BucketLayout layout=HL2ModernHud::WeaponBuckets(screenWidth,screenHeight,6,20,19);
  assert(layout.first+layout.visible==20&&layout.visible>0);
  int end=HL2ModernHud::WeaponBucketX(layout,5,active)+(active==5?layout.expanded:layout.compact);assert(end<=screenWidth);
  C_BasePlayer player;C_BasePlayer::local=&player;CHudWeaponSelection hud;C_BaseCombatWeapon inventory[20],other;
  for(int i=0;i<20;++i){inventory[i].slot=active;inventory[i].ammo=i%2!=0;hud.weapons[active][i]=&inventory[i];}
  other.slot=(active+1)%6;hud.weapons[other.slot][0]=&other;
  inventory[19].data.szPrintName="#HL2_SMG";
  hud.selected=&inventory[19];iconDraws=0;selectedOutlines=0;hud.Paint();
  assert(hud.headers==6&&selectedOutlines==1&&iconDraws==layout.visible&&localized>0&&converted>0);
  inventory[19].icon=false;missingLabels=0;hud.Paint();assert(missingLabels>=3);
  int before=iconDraws;hud.draw=false;hud.Paint();assert(iconDraws==before);
 }
 return 0;
}
'''

class HL1WeaponBucketTests(unittest.TestCase):
    def test_actual_painter_and_selection_window(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('no C++ compiler')
        source=(ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'buckets.cpp';exe=Path(td)/'buckets.exe';cpp.write_text(HARNESS+method(source,'void CHudWeaponSelection::Paint()')+SCENARIOS)
            result=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic','-I'+str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_repeatable_overlay_preserves_input_and_icon_policy(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        import patch_hl1_weapon_buckets as patch
        source=(ROOT/'cl_dll/hl2_hud/hud_weaponselection.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        self.assertEqual(patch.restore(source),source)
        self.assertIn('if ( iSlot<0 || iSlot>=MAX_WEAPON_SLOTS )',source)
        self.assertIn('Color normal=gHUD.m_clrNormal;',source)
        self.assertIn('pWeapon->GetSpriteInactive()',source)
        self.assertIn('patch_hl1_weapon_buckets.py',(ROOT/'modernize/apply_modernization.py').read_text())

if __name__=='__main__':unittest.main()
