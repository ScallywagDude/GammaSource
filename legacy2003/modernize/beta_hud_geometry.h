#ifndef GAMMASOURCE_BETA_HUD_GEOMETRY_H
#define GAMMASOURCE_BETA_HUD_GEOMETRY_H
// Pixel-based, bottom-anchored 2003 beta counter strip. All values are in
// the actual VGUI viewport, never the render/backbuffer dimensions.
namespace GammaBetaHud {
struct Rect { int x, y, w, h; };
inline int Clamp(int v,int low,int high) {return v<low?low:(v>high?high:v);}
inline Rect CounterRect(int screenW,int screenH,int which)
{
    // 0=HEALTH, 1=HEV, 2=primary ammo, 3=secondary weapon ammo, 4=ENERGY.
    // Use desktop pixels rather than XRES's 640-wide stretch: the reference
    // preserves tight margins even on widescreen displays.
    const int margin=12, h=56;
    Rect out={margin,screenH-h-6,110,h};
    if(which==1) out.x=150;
    else if(which==4) out.x=288;
    else if(which==2) {out.x=screenW-175;out.w=140;}
    else if(which==3) {out.x=screenW-42;out.w=32;}
    out.y=Clamp(out.y,0,screenH>h?screenH-h:0);
    out.x=Clamp(out.x,0,screenW>out.w?screenW-out.w:0);
    if(out.w>screenW)out.w=screenW;
    if(out.h>screenH)out.h=screenH;
    return out;
}
}
#endif
