#ifndef HL2MODERN_WINDOWS_TEST_SHIM_H
#define HL2MODERN_WINDOWS_TEST_SHIM_H
#define MAX_PATH 260
#ifndef __cdecl
#define __cdecl
#endif
// Win32 DWORD is always 32 bits, including an LP64 Unix test host.
typedef unsigned int DWORD;
typedef void *HMODULE;
typedef void (*FARPROC)();
// Avoid colliding with the Windows CRT's own kernel32 imports when this
// fake-runtime fixture is compiled on an actual Windows host.
#define GetModuleFileNameA HL2ModernTest_GetModuleFileNameA
#define LoadLibraryA HL2ModernTest_LoadLibraryA
#define GetProcAddress HL2ModernTest_GetProcAddress
#define FreeLibrary HL2ModernTest_FreeLibrary
extern "C" DWORD GetModuleFileNameA(HMODULE, char *, DWORD);
extern "C" HMODULE LoadLibraryA(const char *);
extern "C" FARPROC GetProcAddress(HMODULE, const char *);
extern "C" int FreeLibrary(HMODULE);
#endif
