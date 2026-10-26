#ifndef HL2MODERN_UCRT_ALLOCATOR_COMPAT_H
#define HL2MODERN_UCRT_ALLOCATOR_COMPAT_H

// Modern UCRT helpers must use the same allocator as the legacy entry points.
// Otherwise importing one helper also imports conflicting CRT heap overrides.
#include <errno.h>
#include <wchar.h>
#include <windows.h>

static void HL2ModernWideDiagnostic(const wchar_t *source, char *target, int capacity)
{
    target[0] = 0;
    if (!source) return;
    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, capacity, NULL, NULL))
    {
        const char fallback[] = "(wide diagnostic conversion failed)";
        const unsigned int count = sizeof(fallback) < (unsigned int)capacity ?
            sizeof(fallback) : (unsigned int)capacity;
        memcpy(target, fallback, count);
        target[capacity - 1] = 0;
    }
}

extern "C" {

void *__cdecl _expand_base(void *memory, size_t bytes) _CRT_NOEXCEPT
{
    return g_pMemAlloc->Expand(memory, bytes);
}

void *__cdecl _recalloc_dbg(void *memory, size_t count, size_t bytes,
                           int nBlockUse, const char *file, int line)
{
    if (bytes && count > ((size_t)-1) / bytes)
    {
        errno = ENOMEM;
        return NULL;
    }
    AttribIfCrt();
    const size_t total = count * bytes;
    const size_t previous = memory ? g_pMemAlloc->GetSize(memory) : 0;
    void *result = g_pMemAlloc->Realloc(memory, total, file, line);
    if (result && total > previous)
        memset((char *)result + previous, 0, total - previous);
    return result;
}

int __cdecl _CrtDbgReportW(int kind, const wchar_t *file, int line,
                          const wchar_t *module, const wchar_t *format, ...)
{
    wchar_t wide[1024];
    wide[0] = 0;
    if (format)
    {
        va_list args;
        va_start(args, format);
        _vsnwprintf(wide, sizeof(wide) / sizeof(wide[0]) - 1, format, args);
        va_end(args);
        wide[sizeof(wide) / sizeof(wide[0]) - 1] = 0;
    }
    char message[4096], source[1024], binary[1024];
    HL2ModernWideDiagnostic(wide, message, sizeof(message));
    HL2ModernWideDiagnostic(file, source, sizeof(source));
    HL2ModernWideDiagnostic(module, binary, sizeof(binary));
    return g_pMemAlloc->CrtDbgReport(kind, file ? source : NULL, line,
                                   module ? binary : NULL, message);
}

} // extern "C"
#endif
