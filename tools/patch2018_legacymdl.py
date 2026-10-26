"""2018 datacache: convert MDL v36/v37 at load time (legacymdl.cpp) and cache the result."""
from pathlib import Path
root = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\datacache")
p = root / "mdlcache.cpp"
d = p.read_bytes(); nl = b"\r\n" if b"\r\n" in d else b"\n"
def N(s): return s.replace(b"\n", nl)

if b"legacymdl.h" not in d:
    d = d.replace(N(b'#include "functors.h"\n'), N(b'#include "functors.h"\n#include "legacymdl.h"\n'), 1)

helper = N(b'''
//-----------------------------------------------------------------------------
// GammaSource: 2001-2003 models (MDL v36/v37) are converted to v48 the first time
// they load. The .mdl/.vvd/.vtx results go to <mod>/legacy_models_cache, which sits
// at the head of the GAME search path, so later loads (and the .vvd/.vtx reads that
// follow) find the converted files directly. Delete that folder to reconvert.
//-----------------------------------------------------------------------------
static ConVar mdl_legacy_convert( "mdl_legacy_convert", "1", 0, "Convert MDL v36/v37 models to v48 at load time" );
static char s_szLegacyCacheDir[MAX_PATH];
static CThreadFastMutex s_LegacyCacheMutex;

static void EnsureLegacyModelCachePath()
{
	AUTO_LOCK( s_LegacyCacheMutex );
	if ( s_szLegacyCacheDir[0] )
		return;
	char szMod[MAX_PATH * 4] = "";
	g_pFullFileSystem->GetSearchPath( "MOD", false, szMod, sizeof( szMod ) );
	char *pSemi = strchr( szMod, ';' );
	if ( pSemi )
		*pSemi = 0;
	if ( !szMod[0] )
		V_strncpy( szMod, ".", sizeof( szMod ) );
	V_ComposeFileName( szMod, "legacy_models_cache", s_szLegacyCacheDir, sizeof( s_szLegacyCacheDir ) );
	V_FixSlashes( s_szLegacyCacheDir );
	g_pFullFileSystem->CreateDirHierarchy( s_szLegacyCacheDir, NULL );
	g_pFullFileSystem->AddSearchPath( s_szLegacyCacheDir, "GAME", PATH_ADD_TO_HEAD );
}

static bool ConvertLegacyModel( const char *pFileName, CUtlBuffer &buf )
{
	char szBase[MAX_PATH];
	V_strncpy( szBase, pFileName, sizeof( szBase ) );
	V_StripExtension( szBase, szBase, sizeof( szBase ) );

	static const char *s_pVtxSuffix[] = { ".dx80.vtx", ".dx90.vtx", ".sw.vtx", ".dx7_2bone.vtx" };
	CUtlBuffer vtxBuf[ARRAYSIZE( s_pVtxSuffix )];
	LegacyVtxInput_t vtxIn[ARRAYSIZE( s_pVtxSuffix )];
	int nVtx = 0;
	for ( int i = 0; i < ARRAYSIZE( s_pVtxSuffix ); i++ )
	{
		char szVtx[MAX_PATH];
		V_snprintf( szVtx, sizeof( szVtx ), "%s%s", szBase, s_pVtxSuffix[i] );
		if ( !g_pFullFileSystem->ReadFile( szVtx, "GAME", vtxBuf[i] ) || vtxBuf[i].TellMaxPut() < 4 )
			continue;
		if ( *(int *)vtxBuf[i].Base() != 6 )
			continue;	// already VTX7 (or something else); not ours to convert
		vtxIn[nVtx].pSuffix = s_pVtxSuffix[i];
		vtxIn[nVtx].pData = vtxBuf[i].Base();
		vtxIn[nVtx].nSize = vtxBuf[i].TellMaxPut();
		nVtx++;
	}

	CUtlBuffer mdl48, vvd;
	CUtlVector<LegacyVtxOutput_t> vtxOut;
	CUtlString report;
	if ( !LegacyMDL_Convert( buf.Base(), buf.TellMaxPut(), vtxIn, nVtx, mdl48, vvd, vtxOut, report ) )
	{
		Warning( "Model %s (MDL v%d) could not be converted: %s\\n", pFileName, ((studiohdr_t *)buf.Base())->version, report.Get() );
		return false;
	}
	if ( report.Length() )
		DevMsg( "Converted legacy model %s (%s)\\n", pFileName, report.Get() );

	// write the converted set into the cache
	char szOut[MAX_PATH], szDir[MAX_PATH];
	V_ComposeFileName( s_szLegacyCacheDir, szBase, szOut, sizeof( szOut ) );
	V_FixSlashes( szOut );
	V_ExtractFilePath( szOut, szDir, sizeof( szDir ) );
	g_pFullFileSystem->CreateDirHierarchy( szDir, NULL );
	char szFile[MAX_PATH];
	// .vvd/.vtx first: the .mdl's presence marks a complete conversion
	V_snprintf( szFile, sizeof( szFile ), "%s.vvd", szOut );
	g_pFullFileSystem->WriteFile( szFile, NULL, vvd );
	for ( int i = 0; i < vtxOut.Count(); i++ )
	{
		V_snprintf( szFile, sizeof( szFile ), "%s%s", szOut, vtxOut[i].suffix );
		g_pFullFileSystem->WriteFile( szFile, NULL, vtxOut[i].data );
	}
	V_snprintf( szFile, sizeof( szFile ), "%s.mdl", szOut );
	g_pFullFileSystem->WriteFile( szFile, NULL, mdl48 );

	buf.Purge();
	buf.Put( mdl48.Base(), mdl48.TellMaxPut() );
	return true;
}

''')
anchor = N(b"bool CMDLCache::ReadMDLFile( MDLHandle_t handle, const char *pMDLFileName, CUtlBuffer &buf )\n")
if b"ConvertLegacyModel(" not in d:
    assert d.count(anchor) == 1
    i = d.index(anchor)
    # back up to the comment block preceding the function
    j = d.rfind(N(b"//-----------------------------------------------------------------------------\n//\n"), 0, i)
    j = j if j > 0 and i - j < 400 else i
    d = d[:j] + helper + d[j:]

    old = N(b"\tbool bOk = ReadFileNative( pFileName, \"GAME\", buf, 0, MDLCACHE_STUDIOHDR );\n")
    new = N(b"\tif ( mdl_legacy_convert.GetBool() )\n\t\tEnsureLegacyModelCachePath();\n\n"
            b"\tbool bOk = ReadFileNative( pFileName, \"GAME\", buf, 0, MDLCACHE_STUDIOHDR );\n")
    assert d.count(old) == 1
    d = d.replace(old, new)

    old2 = N(b"\t// GammaSource: MDL v36/v37 (2001-2003) must be converted to v44+ first; reading\n")
    new2 = N(b"\tif ( mdl_legacy_convert.GetBool() && LegacyMDL_IsLegacy( buf.Base(), buf.TellMaxPut() ) )\n\t{\n"
             b"\t\tif ( !ConvertLegacyModel( pFileName, buf ) )\n\t\t\treturn false;\n"
             b"\t\tpStudioHdr = (studiohdr_t*)buf.PeekGet();\n\t}\n\n") + old2
    assert d.count(old2) == 1
    d = d.replace(old2, new2)
    p.write_bytes(d)

w = root / "wscript"
t = w.read_text()
if "legacymdl.cpp" not in t:
    t = t.replace("\t\t'mdlcache.cpp',\n", "\t\t'mdlcache.cpp',\n\t\t'legacymdl.cpp',\n")
    w.write_text(t)
print("ok")
