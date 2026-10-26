"""2018 materialsystem: map 2002-2003 shader names onto the 2018 shaders."""
from pathlib import Path
p = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\materialsystem\cmaterial.cpp")
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
old = b"\t\tpShader = ShaderSystem()->FindShader( pShaderName );\n\t\tif ( !pShader )\n\t\t{\n".replace(b"\n", nl)
new = (b"\t\tpShader = ShaderSystem()->FindShader( pShaderName );\n"
       b"\t\tif ( !pShader )\n\t\t{\n"
       b"\t\t\t// GammaSource: 2002-2003 materials name per-DX-level and retired shaders.\n"
       b"\t\t\tconst char *pLegacy = GammaLegacyShaderName( pShaderName, pFallbackShaderNameBuf, sizeof( pFallbackShaderNameBuf ) );\n"
       b"\t\t\tif ( pLegacy )\n\t\t\t{\n"
       b"\t\t\t\tpShader = ShaderSystem()->FindShader( pLegacy );\n"
       b"\t\t\t\tif ( pShader )\n\t\t\t\t\tpShaderName = pLegacy;\n"
       b"\t\t\t}\n\t\t}\n"
       b"\t\tif ( !pShader )\n\t\t{\n").replace(b"\n", nl)
helper = (b"\n//-----------------------------------------------------------------------------\n"
          b"// GammaSource: 2002-2003 shader names -> 2018 shader names. Returns NULL if no mapping.\n"
          b"//-----------------------------------------------------------------------------\n"
          b"static const char *GammaLegacyShaderName( const char *pName, char *pBuf, int nBufLen )\n{\n"
          b"\tstatic const char *s_Aliases[][2] =\n\t{\n"
          b"\t\t{ \"basetimeslightmap\", \"LightmappedGeneric\" },\n"
          b"\t\t{ \"lightmappedtwotexture\", \"LightmappedGeneric\" },\n"
          b"\t\t{ \"vertexlittexture\", \"VertexLitGeneric\" },\n"
          b"\t\t{ \"unlittexture\", \"UnlitGeneric\" },\n"
          b"\t\t{ \"skin\", \"VertexLitGeneric\" },\n"
          b"\t\t{ \"basetimesvertexcoloralphablendnooverbright\", \"UnlitGeneric\" },\n"
          b"\t};\n"
          b"\tV_strncpy( pBuf, pName, nBufLen );\n"
          b"\t// strip a trailing _DX6/_DX7/_DX8/_DX80/_DX9/_DX90 level suffix\n"
          b"\tchar *pSuffix = V_stristr( pBuf, \"_dx\" );\n"
          b"\tif ( pSuffix && pSuffix[3] >= '0' && pSuffix[3] <= '9' )\n\t\t*pSuffix = 0;\n"
          b"\tfor ( int i = 0; i < ARRAYSIZE( s_Aliases ); ++i )\n\t{\n"
          b"\t\tif ( !V_stricmp( pBuf, s_Aliases[i][0] ) )\n\t\t{\n"
          b"\t\t\tV_strncpy( pBuf, s_Aliases[i][1], nBufLen );\n\t\t\tbreak;\n\t\t}\n\t}\n"
          b"\treturn V_stricmp( pBuf, pName ) ? pBuf : NULL;\n}\n").replace(b"\n", nl)
if b"GammaLegacyShaderName( const char" not in d:
    assert d.count(old) == 1
    d = d.replace(old, new)
    # insert helper before the function containing the lookup
    anchor = d.rfind(nl + nl, 0, d.index(new))
    # place helper at the start of the enclosing function definition
    fn = d.rfind(b"\nIShader *CMaterial::", 0, d.index(new))
    if fn < 0:
        fn = d.rfind(b"\nvoid CMaterial::", 0, d.index(new))
    d = d[:fn] + helper + d[fn:]
    p.write_bytes(d)
print("ok")
