#ifndef HL2_MODERN_STUDIO_SIDECAR_V21_H
#define HL2_MODERN_STUDIO_SIDECAR_V21_H
// On-disk, 32-bit LE header checks for the actual 2003 VTX v6 format.
// This is not a substitute for engine-level mesh/animation verification.
#include <stddef.h>
namespace HL2ModernStudioSidecar {
inline unsigned long U32(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
           ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
inline bool MdlHeader(const unsigned char *data, size_t bytes, size_t length) {
    if (!data || bytes<80 || length<80 || length>64u*1024u*1024u) return false;
    if (data[0]!='I' || data[1]!='D' || data[2]!='S' || data[3]!='T') return false;
    const unsigned long version=U32(data+4);
    if (version!=36 && version!=37) return false;
    if (U32(data+76)!=length) return false;
    bool terminated=false;
    for (size_t i=12;i<76;++i) if (data[i]==0) terminated=true;
    return terminated;
}
inline bool LegacyVtxHeader(const unsigned char *mdl, size_t mdlBytes,
                            const unsigned char *vtx, size_t vtxBytes,
                            size_t vtxLength) {
    // OptimizedModel::FileHeader_t in common/optimize.h, v6; the older
    // reader does not understand the later retail v7 VTX layout.
    if (!mdl || mdlBytes<80 || !vtx || vtxBytes<36 || vtxLength<36 ||
        vtxLength>64u*1024u*1024u) return false;
    if (U32(vtx)!=6 || U32(vtx+16)!=U32(mdl+8)) return false;
    const unsigned long lod=U32(vtx+20), parts=U32(vtx+28), offset=U32(vtx+32);
    // BodyPartHeader_t in v6 consists of two int32 values.
    return lod>=1 && lod<=8 && parts>=1 && parts<=256 && offset>=36 &&
           offset<=vtxLength && parts<=(vtxLength-offset)/8;
}
}
#endif
