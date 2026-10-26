#ifndef HL2MODERN_STEAM_CLIENT_BRIDGE_H
#define HL2MODERN_STEAM_CLIENT_BRIDGE_H

// Optional Steamworks bootstrap for the 32-bit Windows client.
// Install steam_api.dll from the *authorized* Steamworks SDK next to the
// executable, supply your own app identity, and opt in with -steamworks.
// This deliberately does not ship Valve binaries or Steam SDK headers.
// It only provides Init / RunCallbacks / Shutdown. Auth, overlay rendering,
// Steam Input and networking require the official SDK and further integration.
#ifdef _WIN32
#include <windows.h>
#include <cstring>

namespace HL2ModernSteam
{
class ClientBridge
{
public:
    ClientBridge() : module_(NULL), initialized_(false), run_(NULL), shutdown_(NULL) {}
    ~ClientBridge() { Stop(); }

    bool Start()
    {
        if (initialized_) return true;
        if (module_) Stop();

        // Fully-qualified executable-directory path prevents DLL search-path
        // hijacking through the working directory or game data directories.
        char path[MAX_PATH];
        const DWORD size = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (!size || size >= MAX_PATH) return false;
        char *slash = std::strrchr(path, '\\');
        if (!slash) return false;
        const char name[] = "steam_api.dll"; // 2003 Source projects are x86.
        if (static_cast<size_t>(slash - path) + sizeof(name) >= MAX_PATH) return false;
        std::memcpy(slash + 1, name, sizeof(name));

        module_ = LoadLibraryA(path);
        if (!module_) return false;
        typedef bool (__cdecl *InitFn)();
        InitFn init = reinterpret_cast<InitFn>(GetProcAddress(module_, "SteamAPI_Init"));
        run_ = reinterpret_cast<VoidFn>(GetProcAddress(module_, "SteamAPI_RunCallbacks"));
        shutdown_ = reinterpret_cast<VoidFn>(GetProcAddress(module_, "SteamAPI_Shutdown"));
        if (!init || !run_ || !shutdown_ || !init())
        {
            Stop();
            return false;
        }
        initialized_ = true;
        return true;
    }

    void Pump()
    {
        // Must be invoked on the game/main thread, not from a worker thread.
        if (initialized_ && run_) run_();
    }

    void Stop()
    {
        if (initialized_ && shutdown_) shutdown_();
        initialized_ = false;
        run_ = shutdown_ = NULL;
        if (module_) { FreeLibrary(module_); module_ = NULL; }
    }

    bool IsActive() const { return initialized_; }

private:
    typedef void (__cdecl *VoidFn)();
    HMODULE module_;
    bool initialized_;
    VoidFn run_;
    VoidFn shutdown_;
    ClientBridge(const ClientBridge &);
    ClientBridge &operator=(const ClientBridge &);
};
} // namespace HL2ModernSteam
#endif // _WIN32
#endif // HL2MODERN_STEAM_CLIENT_BRIDGE_H
