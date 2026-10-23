#ifndef HL2MODERN_HL1_WEAPON_BUCKETS_H
#define HL2MODERN_HL1_WEAPON_BUCKETS_H
namespace HL2ModernHud {
struct BucketLayout { int x,y,compact,expanded,header,row,gap,visible,first; };
inline BucketLayout WeaponBuckets(int width,int height,int slots,int count,int selected)
{
    BucketLayout out={12,4,30,144,28,66,6,0,0};
    if (slots<1 || width<slots*8 || height<40) return out;
    if (width<480) { out.x=4;out.compact=22;out.expanded=112;out.gap=3; }
    int available=width-out.x*2-(slots-1)*(out.compact+out.gap);
    if (out.expanded>available) out.expanded=available;
    if (out.expanded<8) out.expanded=8;
    if (height<400) out.row=48;
    out.visible=(height-out.y-out.header-12)/(out.row+out.gap);
    if (out.visible<1) out.visible=1;
    if (out.visible>count) out.visible=count;
    if (selected>=out.visible) out.first=selected-out.visible+1;
    if (out.first>count-out.visible) out.first=count-out.visible;
    if (out.first<0) out.first=0;
    return out;
}
inline int WeaponBucketX(const BucketLayout &layout,int slot,int active)
{
    return layout.x+slot*(layout.compact+layout.gap)+(slot>active?layout.expanded-layout.compact:0);
}
}
#endif
