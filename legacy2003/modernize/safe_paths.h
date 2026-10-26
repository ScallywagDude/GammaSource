#ifndef HL2MODERN_SAFE_PATHS_H
#define HL2MODERN_SAFE_PATHS_H

#include <cstddef>
#include <cstring>
// Standalone C++98 path boundary helper for legacy engine code.
namespace HL2ModernPaths
{
inline bool CopyDirectory(const char *source, char *destination, std::size_t capacity)
{
    if (!source || !destination || capacity == 0) return false;
    // Bounded scan: never run strlen over an attacker-supplied unbounded path.
    std::size_t length = 0;
    while (length < capacity && source[length]) ++length;
    if (length == 0 || length >= capacity) return false;
    std::memcpy(destination, source, length + 1);
    while (length > 1 && (destination[length - 1] == '/' || destination[length - 1] == '\\'))
    {
        // Do not turn a drive root (C:\\) into C:, which is a different path.
        if (length == 3 && destination[1] == ':' &&
            ((destination[0] >= 'A' && destination[0] <= 'Z') ||
             (destination[0] >= 'a' && destination[0] <= 'z'))) break;
        destination[--length] = 0;
    }
    return true;
}
} // namespace HL2ModernPaths
#endif
