#include "../weapon_hud_layout.h"
#include <cassert>
#include <cstdio>
#include <string.h>
// Minimal KeyValues-shaped nodes exercise the actual production selection
// template, including malformed/empty layouts and unsupported font metadata.
struct SpriteNode {
    const char *file;
    SpriteNode *high,*low;
    bool font;
    SpriteNode(const char *f=0):file(f),high(0),low(0),font(false) {}
    SpriteNode *FindKey(const char *key) {
        if(!strcmp(key,"font")) return font ? this : 0;
        if(!strcmp(key,"640")) return high;
        if(!strcmp(key,"320")) return low;
        return 0;
    }
    const char *GetString(const char *key,const char *fallback) {
        return !strcmp(key,"file") && file ? file : fallback;
    }
};
int main(){
    HL2ModernHud::Rect r;
    SpriteNode role,high("a_icons1"),low("crosshairs");
    role.high=&high;role.low=&low;
    assert(HL2ModernHud::SelectSpriteData(&role)==&high);
    high.file="";assert(HL2ModernHud::SelectSpriteData(&role)==&low);
    role.file="direct_root_material";assert(HL2ModernHud::SelectSpriteData(&role)==&role);
    role.font=true;assert(!HL2ModernHud::SelectSpriteData(&role));
    role.font=false;role.file=0;high.file="a_icons1";high.font=true;
    assert(!HL2ModernHud::SelectSpriteData(&role));
    high.font=false;role.high=0;assert(HL2ModernHud::SelectSpriteData(&role)==&low);
    role.low=0;assert(!HL2ModernHud::SelectSpriteData(&role));
    assert(!HL2ModernHud::SelectSpriteData((SpriteNode*)0));
    char material[64];
    assert(HL2ModernHud::SpriteMaterialName("a_icons1",material,sizeof(material),true));
    assert(!strcmp(material,"sprites/a_icons1"));
    assert(HL2ModernHud::SpriteMaterialName("crosshairs",material,sizeof(material),false));
    assert(!strcmp(material,"crosshairs"));
    assert(HL2ModernHud::SpriteMaterialName("Sprites/crosshairs",material,sizeof(material),true));
    assert(!strcmp(material,"Sprites/crosshairs"));
    assert(!HL2ModernHud::SpriteMaterialName("crosshairs",material,8,true));
    assert(!HL2ModernHud::SpriteMaterialName("",material,sizeof(material),true));
    assert(HL2ModernHud::AtlasRect(0,0,64,32,64,32));
    assert(!HL2ModernHud::AtlasRect(0,0,65,32,64,32));
    assert(!HL2ModernHud::AtlasRect(0,0,64,32,0,0));
    assert(!HL2ModernHud::AtlasRect(-1,0,64,32,64,32));
    assert(!HL2ModernHud::AtlasRect(4,4,4,8,64,32));
    assert(!HL2ModernHud::FitIcon(0,1,0,0,10,10,r));
    assert(!HL2ModernHud::FitIcon(1,10,0,0,-1,10,r));
    assert(HL2ModernHud::FitIcon(200,100,0,0,70,40,r));
    assert(r.w==70 && r.h==35 && r.y==2);
    assert(HL2ModernHud::FitIcon(32,16,100,200,100,100,r));
    assert(r.w==32 && r.h==16 && r.x==134 && r.y==242);
    assert(HL2ModernHud::FitIcon(1,100000,0,0,20,10,r));
    assert(r.w>=1 && r.h<=10);
    std::puts("weapon HUD icon fit passed");
}
