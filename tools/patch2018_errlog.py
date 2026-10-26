"""Make fatal launcher errors leave their text in fatal_error.log (the message box is easy to miss)."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\launcher\launcher.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b"\tcase SPEW_ERROR:\n\tdefault:\n#if defined( WIN32 ) || defined( USE_SDL )\n\t\t::MessageBox( NULL, pMsg, \"Error!\", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );").replace(b"\n", nl)
new = (b"\tcase SPEW_ERROR:\n\tdefault:\n\t\t{\n\t\t\t// GammaSource: keep the fatal error text on disk.\n\t\t\tFILE *fp = fopen( \"fatal_error.log\", \"a\" );\n\t\t\tif ( fp ) { fputs( pMsg, fp ); fputs( \"\\n\", fp ); fclose( fp ); }\n\t\t}\n#if defined( WIN32 ) || defined( USE_SDL )\n\t\t::MessageBox( NULL, pMsg, \"Error!\", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );").replace(b"\n", nl)
if new not in d:
    assert d.count(old) == 1
    d = d.replace(old, new)
    p.write_bytes(d)
print("ok")
