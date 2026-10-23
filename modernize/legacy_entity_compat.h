#ifndef HL2MODERN_LEGACY_ENTITY_COMPAT_H
#define HL2MODERN_LEGACY_ENTITY_COMPAT_H
#include <stddef.h>
namespace HL2ModernEntityCompat
{
    inline bool CopyKeyWithoutSuffix(const char *source, char *out, size_t capacity)
    {
        if (!source || !out || !capacity) return false;
        size_t i=0;
        for (; source[i] && source[i]!='#'; ++i)
        {
            if (i+1>=capacity) { out[0]=0; return false; }
            out[i]=source[i];
        }
        out[i]=0;
        return true;
    }

    template<class Engine> inline bool IsMapValid(Engine *engine, const char *name)
    {
        if (!engine || !name) return false;
        // The old engine ABI takes char*. Give it private writable storage.
        char writable[256];
        size_t i=0;
        for (; name[i]; ++i)
        {
            if (i+1>=sizeof(writable)) return false;
            writable[i]=name[i];
        }
        writable[i]=0;
        return engine->IsMapValid(writable)!=0;
    }
}
#endif
