// File bounds and read accounting for the original VTF streaming loader.
#ifndef GAMMASOURCE_TEXTURE_FILE_SAFETY_H
#define GAMMASOURCE_TEXTURE_FILE_SAFETY_H

// Texture sizes and offsets are signed ints in the original VTF interfaces.
// Check against the unsigned filesystem length without overflowing an int.
inline bool TextureFile_HeaderFits( unsigned int fileBytes, int headerBytes )
{
    return headerBytes > 0 && (unsigned int)headerBytes <= fileBytes;
}

inline bool TextureFile_ExpectedSizeFits( unsigned int fileBytes, int headerBytes, int expectedBytes )
{
    return TextureFile_HeaderFits( fileBytes, headerBytes ) &&
        expectedBytes >= headerBytes && (unsigned int)expectedBytes <= fileBytes;
}

inline bool TextureFile_ReadCompleted( int requestedBytes, int actualBytes )
{
    return requestedBytes >= 0 && actualBytes == requestedBytes;
}

#endif
