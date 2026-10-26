from pathlib import Path
p = Path(r"C:\Users\doge6\Videos\GammaSource-main\engine\sys_getmodes.cpp")
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
edits = [
    (b'#include "win32_borderless.h"',
     b'#include "win32_borderless.h"' + nl + b'#include "convar.h"' + nl + nl +
     b'// Read-only report for GameUI: the engine treats borderless as windowed.' + nl +
     b'static ConVar vid_borderless( "vid_borderless", "0", 0, "1 when the window is borderless windowed (set from Video options or -borderless)" );'),
    (b'#if defined(_WIN32) && defined(_M_IX86)' + nl + b'\tif ( NativeBorderlessRequested() )' + nl + b'\t{' + nl +
     b'\t\t// Keep desktop pixel coordinates and the windowed D3D presentation path.' + nl +
     b'\t\tSetProcessDPIAware();' + nl + b'\t\twindowed = true;' + nl + b'\t}' + nl + b'#endif',
     b'#if defined(_WIN32) && defined(_M_IX86)' + nl + b'\tif ( NativeBorderlessRequested() )' + nl + b'\t{' + nl +
     b'\t\t// Keep desktop pixel coordinates and the windowed D3D presentation path.' + nl +
     b'\t\tSetProcessDPIAware();' + nl + b'\t\twindowed = true;' + nl + b'\t}' + nl +
     b'\tvid_borderless.SetValue( NativeBorderlessRequested() ? 1 : 0 );' + nl + b'#endif'),
]
for old, new in edits:
    assert d.count(old) == 1, old
    d = d.replace(old, new)
p.write_bytes(d)
print("ok")
