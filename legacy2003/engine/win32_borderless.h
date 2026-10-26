#ifndef ENGINE_WIN32_BORDERLESS_H
#define ENGINE_WIN32_BORDERLESS_H
#if defined(_WIN32) && defined(_M_IX86)
#include <windows.h>
#include "vstdlib/icommandline.h"
#include "iregistry.h"

// -borderless/-noborder on the command line, or the Video options' "Borderless
// windowed" choice (stored like ScreenWindowed). -fullscreen/-full still win.
inline bool NativeBorderlessRequested()
{
    if (CommandLine()->FindParm("-borderless") || CommandLine()->FindParm("-noborder"))
        return true;
    if (CommandLine()->FindParm("-fullscreen") || CommandLine()->FindParm("-full"))
        return false;
    return registry && registry->ReadInt("GammaScreenBorderless", 0) != 0;
}

inline RECT NativeBorderlessBounds()
{
    POINT origin = {0, 0};
    MONITORINFO monitor = {0};
    monitor.cbSize = sizeof(monitor);
    if (GetMonitorInfo(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &monitor))
        return monitor.rcMonitor;
    RECT fallback = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    return fallback;
}
#endif
#endif
