#ifndef _WIN32
#define _WIN32 1
#endif
#include "../steam_client_bridge.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static bool g_dllPresent = true;
static bool g_exportPresent = true;
static bool g_initSucceeds = true;
static bool g_tooLongPath = false;
static int g_load = 0, g_free = 0, g_init = 0, g_shutdown = 0, g_tick = 0;
static char g_loadedPath[MAX_PATH] = {0};
static bool __cdecl SteamInit() { ++g_init; return g_initSucceeds; }
static void __cdecl SteamStop() { ++g_shutdown; }
static void __cdecl SteamTick() { ++g_tick; }
extern "C" DWORD GetModuleFileNameA(HMODULE, char *path, DWORD n)
{
    if (g_tooLongPath) return n;
    const char *exe = "C:\\Games\\HalfLife2\\hl2.exe";
    assert(std::strlen(exe)<n);
    std::memcpy(path,exe,std::strlen(exe)+1);
    return static_cast<DWORD>(std::strlen(exe));
}
extern "C" HMODULE LoadLibraryA(const char *path)
{
    assert(std::strlen(path)<sizeof(g_loadedPath));
    std::memcpy(g_loadedPath,path,std::strlen(path)+1);
    if (!g_dllPresent) return NULL;
    ++g_load;
    return reinterpret_cast<HMODULE>(1);
}
extern "C" FARPROC GetProcAddress(HMODULE, const char *name)
{
    if (!g_exportPresent && std::strcmp(name, "SteamAPI_RunCallbacks") == 0) return NULL;
    if (std::strcmp(name, "SteamAPI_Init") == 0) return reinterpret_cast<FARPROC>(&SteamInit);
    if (std::strcmp(name, "SteamAPI_Shutdown") == 0) return reinterpret_cast<FARPROC>(&SteamStop);
    if (std::strcmp(name, "SteamAPI_RunCallbacks") == 0) return reinterpret_cast<FARPROC>(&SteamTick);
    return NULL;
}
extern "C" int FreeLibrary(HMODULE) { ++g_free; return 1; }

int main()
{
    using HL2ModernSteam::ClientBridge;
    {   ClientBridge bridge;
        assert(bridge.Start());
        assert(bridge.IsActive());
        assert(std::strcmp(g_loadedPath,"C:\\Games\\HalfLife2\\steam_api.dll")==0);
        assert(bridge.Start() && g_init == 1); // idempotent
        bridge.Pump(); bridge.Pump();
        assert(g_tick == 2);
        bridge.Stop(); bridge.Stop(); bridge.Pump();
        assert(!bridge.IsActive() && g_shutdown == 1 && g_free == 1 && g_tick == 2);
    }
    {   ClientBridge bridge;
        g_initSucceeds = false;
        assert(!bridge.Start());
        assert(g_free == 2 && g_shutdown == 1); // never shut down on failed init
        bridge.Pump(); assert(g_tick == 2);
        g_initSucceeds = true;
        assert(bridge.Start());
        assert(bridge.IsActive());
    } // destructor calls Shutdown + FreeLibrary exactly once
    assert(g_shutdown == 2 && g_free == 3);
    {   ClientBridge bridge;
        g_exportPresent = false;
        assert(!bridge.Start());
        assert(g_free == 4 && g_init == 3); // missing callbacks => never initialize
        g_exportPresent = true;
        g_dllPresent = false;
        assert(!bridge.Start());
        assert(g_free == 4);
        g_dllPresent = true;
        g_tooLongPath = true;
        assert(!bridge.Start());
        assert(g_free == 4);
        g_tooLongPath = false;
    }
    std::puts("Steamworks bootstrap lifecycle smoke tests passed");
}
