#include "Info.h"
#include <string.h>

const char *Info_ValueForKey(const char *info, const char *key)
{
    static char values[4][512];
    static unsigned int next;
    char *result = values[next++ % 4];
    result[0] = '\0';
    if (!info || !key || !*key) return result;

    const char *p = info;
    if (*p == '\\') ++p;
    while (*p)
    {
        const char *begin_key = p;
        const char *end_key = strchr(begin_key, '\\');
        if (!end_key) break; // malformed key-only entry
        p = end_key + 1;
        const char *begin_val = p;
        while (*p && *p != '\\') ++p;
        if (static_cast<size_t>(end_key - begin_key) == strlen(key) &&
            !memcmp(begin_key, key, end_key - begin_key))
        {
            size_t len = static_cast<size_t>(p - begin_val);
            if (len >= sizeof(values[0])) len = sizeof(values[0]) - 1;
            memcpy(result, begin_val, len);
            result[len] = '\0';
            return result;
        }
        if (*p == '\\') ++p;
    }
    return result;
}
