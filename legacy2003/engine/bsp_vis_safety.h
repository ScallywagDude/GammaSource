#ifndef GAMMASOURCE_BSP_VIS_SAFETY_H
#define GAMMASOURCE_BSP_VIS_SAFETY_H

#include <string.h>

// BSP visibility tables have a four-byte cluster count followed by two
// four-byte offsets for each cluster. Validate lengths before indexing.
inline bool BSPVis_ValidTable( int blobBytes, int clusters, int maxClusters )
{
    return blobBytes >= 4 && clusters >= 0 && clusters <= maxClusters &&
        clusters <= ( blobBytes - 4 ) / 8;
}

// Decompress the original BSP visibility run-length format. Invalid or absent
// rows are conservatively treated as fully visible; don't cull valid geometry
// based on malformed visibility data.
inline bool BSPVis_Decompress( const unsigned char *blob, int blobBytes,
    int dataOffset, int headerBytes, unsigned char *out, int rowBytes )
{
    if ( !out || rowBytes < 0 )
        return false;

    if ( rowBytes == 0 )
        return true;

    if ( !blob || blobBytes < 0 || dataOffset < headerBytes ||
         headerBytes < 4 || dataOffset >= blobBytes )
    {
        memset( out, 0xff, rowBytes );
        return false;
    }

    int src = dataOffset;
    int dest = 0;
    while ( dest < rowBytes )
    {
        if ( src >= blobBytes )
            break;

        const unsigned char value = blob[ src++ ];
        if ( value )
        {
            out[ dest++ ] = value;
            continue;
        }

        if ( src >= blobBytes )
            break;

        const int zeroCount = blob[ src++ ];
        if ( zeroCount == 0 )
            break;

        // Original maps may encode a zero run that ends beyond the last
        // padding bits of a row. Match the original truncation behavior.
        const int writtenZeros = zeroCount < rowBytes - dest ? zeroCount : rowBytes - dest;
        memset( out + dest, 0, writtenZeros );
        dest += writtenZeros;
    }

    if ( dest != rowBytes )
    {
        memset( out, 0xff, rowBytes );
        return false;
    }
    return true;
}

#endif // GAMMASOURCE_BSP_VIS_SAFETY_H
