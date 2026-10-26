#ifndef HL2_MODERN_PLAYER_ASSET_GUARD_H
#define HL2_MODERN_PLAYER_ASSET_GUARD_H
// Source v36 cannot consume installed retail v37 models just because their
// filenames match the multiplayer allowlist. This is a cheap preflight only;
// geometry/animation/vertex-companion validation remains the loader's job.
#include <string.h>
#include <stddef.h>
namespace HL2ModernPlayerAssets {
// HL2MODERN_VERTEX_COMPANION_V20: derive only safe, bounded v36 vertex paths.
// The embedded studio name is relative to models/ and must end in .mdl.
inline bool VertexCompanionPath(const unsigned char *prefix, int bytes,
                               const char *suffix, char *out, size_t capacity)
{
    if (!prefix || bytes < 76 || !suffix || !out || !capacity) return false;
    out[0] = 0;
    static const char *const permitted[] = {
        ".dx80.vtx", ".dx7_3bone.vtx", ".dx7_2bone.vtx"
    };
    bool allowed = false;
    for (unsigned int s=0; s<3; ++s)
        if (!strcmp(suffix, permitted[s])) allowed = true;
    if (!allowed) return false;
    // Studio model name occupies exactly 64 bytes starting at offset 12.
    const unsigned char *name = prefix + 12;
    const unsigned char *ending = (const unsigned char *)memchr(name, 0, 64);
    if (!ending || ending == name) return false;
    const size_t length = (size_t)(ending-name);
    if (length < 5 || name[length-5] == '/' || name[length-4] != '.' ||
        (name[length-3]|32) != 'm' || (name[length-2]|32) != 'd' ||
        (name[length-1]|32) != 'l') return false;
    // The Source 2003 loader prepends models/ to this internal name.
    size_t segment = 0;
    for (size_t i=0; i<length; ++i)
    {
        const unsigned char ch = name[i];
        if (ch == '/' || i == length-1)
        {
            const size_t end = (ch=='/' ? i : i+1);
            const size_t span = end-segment;
            if (!span || (span==1 && name[segment]=='.') ||
                (span==2 && name[segment]=='.' && name[segment+1]=='.')) return false;
            segment=i+1;
        }
        if (ch=='\\' || ch==':' || ch < 32 || ch >= 127) return false;
    }
    const size_t suffixLen = strlen(suffix);
    const size_t total = 7 + length-4 + suffixLen;
    if (total >= capacity) return false;
    memcpy(out, "models/", 7);
    memcpy(out+7, name, length-4);
    memcpy(out+7+length-4, suffix, suffixLen+1);
    return true;
}

inline unsigned long ReadLE32(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
           ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
inline bool StudioHeader(const unsigned char *p, int n, unsigned long total,
                        unsigned long supportedVersion, unsigned long minSize) {
    if (!p || n<80 || total<80 || total<minSize) return false;
    if (p[0]!='I' || p[1]!='D' || p[2]!='S' || p[3]!='T') return false;
    if (ReadLE32(p+4)!=supportedVersion) return false;
    const unsigned long declared=ReadLE32(p+76);
    return declared>=80 && declared>=minSize && declared<=total;
}
}
#endif
