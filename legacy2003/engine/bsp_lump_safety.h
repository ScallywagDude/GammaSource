#ifndef GAMMASOURCE_BSP_LUMP_SAFETY_H
#define GAMMASOURCE_BSP_LUMP_SAFETY_H

#include <limits.h>

// Avoid signed offset arithmetic overflow when loading variable-size BSP lumps.
inline bool BSPMap_ValidRange( unsigned int fileSize, int offset, int length )
{
    if ( offset < 0 || length < 0 )
        return false;
    return (unsigned int)offset <= fileSize &&
        (unsigned int)length <= fileSize - (unsigned int)offset &&
        (unsigned int)length <= (unsigned int)INT_MAX - (unsigned int)offset;
}

inline bool BSPMap_ValidSubrange( int lumpSize, int offset, int length )
{
    if ( lumpSize < 0 || offset < 0 || length < 0 )
        return false;
    return offset <= lumpSize && length <= lumpSize - offset;
}

#endif // GAMMASOURCE_BSP_LUMP_SAFETY_H
