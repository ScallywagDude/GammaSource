#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <crtdbg.h>
#include <errno.h>
#include "tier0/memalloc.h"
#include "vstdlib/IKeyValuesSystem.h"

extern "C" void *_recalloc_dbg(void *, size_t, size_t, int, const char *, int);
extern "C" void *_expand_base(void *, size_t) throw();
extern "C" size_t _msize_base(void *) throw();
extern "C" int __cdecl _CrtDbgReportW(int, const wchar_t *, int, const wchar_t *, const wchar_t *, ...);

static int reports = 0;
static int failures = 0;
static int ReportHook(int kind, char *text, int *result)
{
    if (kind != _CRT_WARN) ++failures;
    if (text && strstr(text, "allocator-wide-message")) ++reports;
    *result = 0;
    return 1;
}

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "allocator check failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main()
{
    REQUIRE(g_pMemAlloc != NULL);
    unsigned char *memory = (unsigned char *)malloc(8);
    REQUIRE(memory != NULL);
    memset(memory, 0x3c, 8);
    memory = (unsigned char *)_recalloc_dbg(memory, 32, 1, _NORMAL_BLOCK, __FILE__, __LINE__);
    REQUIRE(memory != NULL);
    for (int i = 0; i < 8; ++i) REQUIRE(memory[i] == 0x3c);
    for (int i = 8; i < 32; ++i) REQUIRE(memory[i] == 0);
    REQUIRE(_msize_base(memory) >= 32);
    errno = 0;
    REQUIRE(_recalloc_dbg(memory, (size_t)-1, 2, _NORMAL_BLOCK, __FILE__, __LINE__) == NULL);
    REQUIRE(errno == ENOMEM && memory[0] == 0x3c);
    REQUIRE(_expand_base(memory, 16) == memory);
    free(memory);

    // vstdlib and this executable must both use tier0's allocator.
    IKeyValuesSystem *system = KeyValuesSystem();
    REQUIRE(system != NULL);
    system->RegisterSizeofKeyValues(64);
    void *key = system->AllocKeyValuesMemory(64);
    REQUIRE(key != NULL);
    memset(key, 0x71, 64);
    system->FreeKeyValuesMemory(key);

    _CRT_REPORT_HOOK previous = _CrtSetReportHook(ReportHook);
    _CrtDbgReportW(_CRT_WARN, L"source", 17, L"module", L"%ls", L"allocator-wide-message");
    _CrtSetReportHook(previous);
    // The release tier0 implementation intentionally has no CRT debug hook.
#ifdef _DEBUG
    REQUIRE(reports == 1 && failures == 0);
#endif
    puts("Allocator DLL boundary, growth zeroing, overflow and diagnostic checks passed.");
    return 0;
}
