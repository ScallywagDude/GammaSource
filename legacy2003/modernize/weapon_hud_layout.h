#ifndef HL2_MODERN_WEAPON_HUD_LAYOUT_H
#define HL2_MODERN_WEAPON_HUD_LAYOUT_H
// C++98-compatible, overflow-resistant texture fitting logic.
namespace HL2ModernHud {
struct Rect { int x,y,w,h; };
// Direct raster metadata wins. Legacy raster blocks prefer 640, then 320;
// font metadata is intentionally unsupported and this never recurses.
template<class Node> inline Node *SelectSpriteData(Node *role) {
    if(!role) return 0;
    if(role->FindKey("font")) return 0;
    const char *file=role->GetString("file","");
    if(file && *file) return role;
    Node *candidate=role->FindKey("640");
    if(candidate) {
        file=candidate->GetString("file","");
        if(file && *file) return candidate->FindKey("font") ? 0 : candidate;
    }
    candidate=role->FindKey("320");
    if(!candidate) return 0;
    file=candidate->GetString("file","");
    return file && *file && !candidate->FindKey("font") ? candidate : 0;
}
inline bool SpriteMaterialName(const char *raw,char *output,unsigned int capacity,bool legacyResolution) {
    if(!raw || !*raw || !output || !capacity) return false;
    bool leaf=true;
    unsigned int length=0;
    for(;raw[length];++length) {
        if(length>=capacity) return false;
        if(raw[length]=='/' || raw[length]=='\\' || raw[length]==':') leaf=false;
    }
    const unsigned int prefix=legacyResolution && leaf ? 8 : 0;
    if(prefix>=capacity || length>=capacity-prefix) return false;
    if(prefix) for(unsigned int j=0;j<prefix;++j) output[j]="sprites/"[j];
    for(unsigned int i=0;i<=length;++i) output[prefix+i]=raw[i];
    return true;
}
inline bool AtlasRect(int left,int top,int right,int bottom,int textureW,int textureH) {
    return textureW>0 && textureH>0 && left>=0 && top>=0 &&
        right>left && bottom>top && right<=textureW && bottom<=textureH;
}
inline bool FitIcon(int sourceW,int sourceH,int boxX,int boxY,int maxW,int maxH,Rect &out) {
    if(sourceW<=0 || sourceH<=0 || maxW<=0 || maxH<=0) return false;
    double sx=(double)maxW/(double)sourceW;
    double sy=(double)maxH/(double)sourceH;
    double scale=sx<sy?sx:sy;
    if(scale>1.0)scale=1.0; // never upscale low-res original icon art
    int width=(int)(sourceW*scale);
    int height=(int)(sourceH*scale);
    if(width<1)width=1;
    if(height<1)height=1;
    if(width>maxW)width=maxW;
    if(height>maxH)height=maxH;
    out.x=boxX+(maxW-width)/2;
    out.y=boxY+(maxH-height)/2;
    out.w=width;
    out.h=height;
    return true;
}
}
#endif
