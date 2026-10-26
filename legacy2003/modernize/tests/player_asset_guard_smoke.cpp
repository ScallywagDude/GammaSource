#include "../hl2_player_asset_guard.h"
#include <assert.h>
#include <string.h>

static void put32(unsigned char *p, unsigned long v) {
    p[0]=(unsigned char)v;
    p[1]=(unsigned char)(v>>8);
    p[2]=(unsigned char)(v>>16);
    p[3]=(unsigned char)(v>>24);
}
int main() {
    unsigned char h[80]; memset(h,0,sizeof h);
    memcpy(h,"IDST",4); put32(h+4,36); put32(h+76,800);
    using HL2ModernPlayerAssets::StudioHeader;
    assert(StudioHeader(h,80,800,36,500));
    assert(!StudioHeader(h,79,800,36,500));
    assert(!StudioHeader(h,80,400,36,500));
    assert(!StudioHeader(h,80,799,36,500));
    assert(!StudioHeader(h,80,800,35,500));
    put32(h+4,37);
    assert(!StudioHeader(h,80,800,36,500)); // Installed retail model must not reach v36 loader.
    put32(h+4,35);
    assert(!StudioHeader(h,80,800,36,500)); // Preview only accepts v36; no silent legacy conversion.
    put32(h+4,36); put32(h+76,900);
    assert(!StudioHeader(h,80,800,36,500));
    put32(h+76,300);
    assert(!StudioHeader(h,80,800,36,500));
    put32(h+76,800); h[0]='X';
    assert(!StudioHeader(h,80,800,36,500));
    assert(!StudioHeader(NULL,0,0,36,500));
    return 0;
}
