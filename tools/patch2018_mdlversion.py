"""Refuse model versions the 2018 cache cannot read (instead of crashing on them)."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\datacache\mdlcache.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
old = (b"\tif ( pStudioHdr->id != IDSTUDIOHEADER )\n\t{\n\t\tDevWarning( \"Model %s not a .MDL format file!\\n\", pMDLFileName );\n\t\treturn false;\n\t}\n").replace(b"\n", nl)
new = old + (b"\n\t// GammaSource: MDL v36/v37 (2001-2003) must be converted to v44+ first; reading\n"
             b"\t// them with the v48 layout crashes (e.g. in ConvertFlexData).\n"
             b"\tif ( pStudioHdr->version < 44 || pStudioHdr->version > STUDIO_VERSION )\n\t{\n"
             b"\t\tWarning( \"Model %s is MDL v%d; this engine reads v44-v%d\\n\", pMDLFileName, pStudioHdr->version, STUDIO_VERSION );\n"
             b"\t\treturn false;\n\t}\n").replace(b"\n", nl)
if new not in d:
    assert d.count(old) == 1
    p.write_bytes(d.replace(old, new))
print("ok")
