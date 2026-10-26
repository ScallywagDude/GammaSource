#ifndef HL2_MODERN_STUDIO_SIDECAR_FS_V21_H
#define HL2_MODERN_STUDIO_SIDECAR_FS_V21_H
// Include only after the engine's filesystem types are declared.
#include "studio_sidecar_v21.h"
namespace HL2ModernStudioSidecar {
// Fail closed on corrupt/retail VTX data; the 2003 renderer expects v6.
template<class FileSystem> inline bool LegacyVertexFile(FileSystem *fs,
    const char *path, const unsigned char *mdl, size_t mdlBytes)
{
    if (!fs || !path || !mdl || mdlBytes<80) return false;
    FileHandle_t f=fs->Open(path,"rb");
    if (f==FILESYSTEM_INVALID_HANDLE) return false;
    const int size=fs->Size(f);
    unsigned char prefix[36];
    const int bytes=(size>=36 && size<=64*1024*1024) ?
        fs->Read(prefix,sizeof(prefix),f) : 0;
    fs->Close(f);
    return bytes==(int)sizeof(prefix) && LegacyVtxHeader(
        mdl,mdlBytes,prefix,sizeof(prefix),(size_t)size);
}
}
#endif
