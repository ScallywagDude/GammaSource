#ifndef HL2_MODERN_WEAPON_GRANT_POLICY_H
#define HL2_MODERN_WEAPON_GRANT_POLICY_H
// Independent portable C++98 checks for developer weapon grants.
#include <string.h>
namespace HL2ModernWeapons {
inline char Lower(char c) { return c>='A' && c<='Z' ? (char)(c-'A'+'a') : c; }
inline bool WeaponClass(const char *name) {
    if (!name || strncmp(name,"weapon_",7)!=0 || !name[7]) return false;
    unsigned int i=0;
    for (; name[i]; ++i) {
        if (i>=63) return false;
        char c=name[i];
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return false;
    }
    return true;
}
inline bool ModelPath(const char *path) {
    if (!path) return false;
    const unsigned int length=(unsigned int)strlen(path);
    if (length<12 || length>=128) return false;
    const char *prefix="models/";
    for (unsigned int i=0;i<7;++i) if (Lower(path[i])!=prefix[i]) return false;
    const char *suffix=".mdl";
    for (unsigned int j=0;j<4;++j) if (Lower(path[length-4+j])!=suffix[j]) return false;
    // v23: reject paths where .mdl is the entire basename.
    if (path[length-5]=='/') return false;
    const char *segment=path;
    for (const char *p=path;;++p) {
        const char c=*p;
        if (c=='\\' || c==':' || c=='?' || c=='#' || (c && (unsigned char)c<32)) return false;
        if (!c || c=='/') {
            unsigned int n=(unsigned int)(p-segment);
            if (!n || (n==1 && segment[0]=='.') || (n==2 && segment[0]=='.' && segment[1]=='.')) return false;
            segment=p+1;
        }
        if (!c) break;
    }
    return true;
}
inline int AmmoNeeded(int maximum,int current) {
    if (maximum<=0 || current<0 || current>=maximum) return 0;
    return maximum-current;
}
inline unsigned long Little32(const unsigned char *data) {
    return (unsigned long)data[0] | ((unsigned long)data[1]<<8) |
        ((unsigned long)data[2]<<16) | ((unsigned long)data[3]<<24);
}
inline bool StudioHeader(const unsigned char *data,unsigned int bytes,unsigned long fileBytes,int expectedVersion,unsigned int minimumLength=80) {
    if (!data || bytes<80 || fileBytes<80 || data[0]!='I' || data[1]!='D' || data[2]!='S' || data[3]!='T') return false;
    const unsigned long version=Little32(data+4);
    const unsigned long declaredLength=Little32(data+76);
    return (version==(unsigned long)expectedVersion || (expectedVersion==36 && version==35)) &&
        declaredLength>=minimumLength && declaredLength>=80 && declaredLength<=fileBytes;
}
}
#endif
