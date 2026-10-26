#ifndef STUDIORENDER_ALIGNED_ALLOC_COMPAT_H
#define STUDIORENDER_ALIGNED_ALLOC_COMPAT_H

#include <stddef.h>

#if defined(_WIN32) && !defined(_WIN64)
namespace StudioRenderLegacyAlignment
{
// Keep the cache-aligned matrices on the same heap as the module's allocator.
// The word immediately before the aligned address records the paired owner.
template <class Allocator>
inline void *Alloc(Allocator *allocator, size_t bytes, size_t alignment)
{
    if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0)
        return NULL;
    const size_t padding = alignment - 1 + sizeof(void *);
    if (bytes > static_cast<size_t>(-1) - padding)
        return NULL;
    void *raw = allocator->Alloc(bytes + padding);
    if (!raw)
        return NULL;
    const size_t address = (reinterpret_cast<size_t>(raw) + sizeof(void *) + alignment - 1)
        & ~(alignment - 1);
    void *aligned = reinterpret_cast<void *>(address);
    static_cast<void **>(aligned)[-1] = raw;
    return aligned;
}

template <class Allocator>
inline void Free(Allocator *allocator, void *aligned)
{
    if (aligned)
        allocator->Free(static_cast<void **>(aligned)[-1]);
}
}
#endif

#endif
