"""GammaSource on the 2018 engine: static 2003 menu background, no live background maps."""
from pathlib import Path
N = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")

def patch(rel, edits):
    p = N / rel; d = p.read_bytes()
    nl = b"\r\n" if b"\r\n" in d else b"\n"
    for old, new in edits:
        old = old.replace(b"\n", nl); new = new.replace(b"\n", nl)
        if new in d: continue
        assert d.count(old) == 1, (rel, old[:60])
        d = d.replace(old, new)
    p.write_bytes(d)

patch('engine/cl_main.cpp', [
(b'#define DEFAULT_BACKGROUND_NAME\t"background01"',
 b'// GammaSource: the 2003 static menu art (materials/console/console_background[_widescreen]).\n#define DEFAULT_BACKGROUND_NAME\t"console_background"'),
(b"bool CL_ShouldLoadBackgroundLevel( const CCommand &args )\n{\n",
 b"bool CL_ShouldLoadBackgroundLevel( const CCommand &args )\n{\n\t// GammaSource: the menu uses the static 2003 image, never a live background map.\n\treturn false;\n\n"),
])
patch('engine/sys_getmodes.cpp', [
(b'Q_strncpy( pBuf, ( aspectRatio >= 1.6f ) ? "materials/console/background01_widescreen.vtf" : "materials/console/background01.vtf", nBufLen );',
 b'Q_strncpy( pBuf, ( aspectRatio >= 1.6f ) ? "materials/console/console_background_widescreen.vtf" : "materials/console/console_background.vtf", nBufLen );'),
(b'm_pBackgroundTexture = LoadVTF( buf, ( aspectRatio >= 1.6f ) ? "materials/console/background01_widescreen.vtf" : "materials/console/background01.vtf" );',
 b'm_pBackgroundTexture = LoadVTF( buf, ( aspectRatio >= 1.6f ) ? "materials/console/console_background_widescreen.vtf" : "materials/console/console_background.vtf" );'),
(b'strcpy( pStartupGraphicName, "materials/console/background01.vtf");',
 b'strcpy( pStartupGraphicName, "materials/console/console_background.vtf");'),
])
print("ok")
