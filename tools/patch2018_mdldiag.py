"""Temporary diagnostics: say why an .mdl read failed (exists? where?)."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\datacache\mdlcache.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
old = b'\t\tDevWarning( "Failed to load %s!\\n", pMDLFileName );'
new = (b'\t\tchar szFull[MAX_PATH] = "";\n'
       b'\t\tg_pFullFileSystem->RelativePathToFullPath( pFileName, "GAME", szFull, sizeof( szFull ) );\n'
       b'\t\tDevWarning( "Failed to load %s! (exists=%d full=\'%s\')\\n", pMDLFileName, g_pFullFileSystem->FileExists( pFileName, "GAME" ) ? 1 : 0, szFull );').replace(b"\n", nl)
if new not in d:
    assert d.count(old) == 1
    p.write_bytes(d.replace(old, new))
print("ok")
