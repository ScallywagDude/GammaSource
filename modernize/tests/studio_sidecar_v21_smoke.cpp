#include "../studio_sidecar_v21.h"
#include <assert.h>
#include <string.h>
using namespace HL2ModernStudioSidecar;
static void Put(unsigned char *p,unsigned long v) {for (int i=0;i<4;++i) p[i]=(unsigned char)(v>>(8*i));}
int main() {
    unsigned char mdl[80]={0}, vtx[80]={0};
    memcpy(mdl,"IDST",4); memcpy(mdl+12,"player.mdl",11);
    Put(mdl+4,36);Put(mdl+8,0x12345678);Put(mdl+76,80);
    Put(vtx,6);Put(vtx+16,0x12345678);Put(vtx+20,1);Put(vtx+28,1);Put(vtx+32,36);
    assert(MdlHeader(mdl,80,80)); assert(LegacyVtxHeader(mdl,80,vtx,80,80));
    Put(mdl+4,37);assert(MdlHeader(mdl,80,80)); assert(LegacyVtxHeader(mdl,80,vtx,80,80));
    Put(mdl+4,38);assert(!MdlHeader(mdl,80,80));Put(mdl+4,37);
    Put(vtx,7);assert(!LegacyVtxHeader(mdl,80,vtx,80,80));Put(vtx,6);
    Put(vtx+16,1);assert(!LegacyVtxHeader(mdl,80,vtx,80,80));Put(vtx+16,0x12345678);
    Put(vtx+32,79);assert(!LegacyVtxHeader(mdl,80,vtx,80,80));Put(vtx+32,36);
    assert(!LegacyVtxHeader(mdl,80,vtx,12,80));
    Put(mdl+76,79);assert(!MdlHeader(mdl,80,80));Put(mdl+76,80);
    memset(mdl+12,'A',64);assert(!MdlHeader(mdl,80,80));
    return 0;
}
