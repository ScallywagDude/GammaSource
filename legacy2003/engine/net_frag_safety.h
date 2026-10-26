#ifndef GAMMASOURCE_NET_FRAG_SAFETY_H
#define GAMMASOURCE_NET_FRAG_SAFETY_H

// Keeps fragment payload lengths positive and within the message buffer.
// headerBytes is the filename plus its zero terminator on the first packet.
inline int Netchan_FileFragmentPayloadBytes( int remaining, int chunkBytes, int headerBytes, int capacityBytes )
{
    if ( remaining <= 0 || chunkBytes <= 0 || headerBytes < 0 ||
         capacityBytes <= 0 || headerBytes >= capacityBytes )
        return 0;
    int available = chunkBytes - headerBytes;
    if ( available > capacityBytes - headerBytes )
        available = capacityBytes - headerBytes;
    if ( available <= 0 )
        return 0;
    return remaining < available ? remaining : available;
}

// Check an append without overflowing signed size computations.
inline bool Netchan_CanAppendFragmentBytes( int current, int added, int capacity )
{
    return current >= 0 && added >= 0 && capacity >= 0 &&
           current <= capacity && added <= capacity - current;
}

#endif
