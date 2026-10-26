"""Write the message that triggers SPEW_ABORT to fatal_error.log before exiting."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\tier0\dbg.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b"//\t\tConMsg( _T(\"Exiting on SPEW_ABORT\\n\") );\n\t\texit(1);").replace(b"\n", nl)
new = (b"//\t\tConMsg( _T(\"Exiting on SPEW_ABORT\\n\") );\n\t\t{\n\t\t\t// GammaSource: keep the aborting message on disk.\n\t\t\tFILE *fp = fopen( \"fatal_error.log\", \"a\" );\n\t\t\tif ( fp ) { fputs( pTempBuffer, fp ); fputs( \"\\n\", fp ); fclose( fp ); }\n\t\t}\n\t\texit(1);").replace(b"\n", nl)
if new not in d:
    assert d.count(old) == 1
    p.write_bytes(d.replace(old, new))
print("ok")
