"""2018 engine: load BSP v18 (2002-2003) maps natively by converting them to v19 in memory."""
from pathlib import Path
N = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master")

def patch(rel, edits):
    p = N / rel; d = p.read_bytes()
    nl = b"\r\n" if b"\r\n" in d else b"\n"
    for old, new in edits:
        old = old.replace(b"\n", nl); new = new.replace(b"\n", nl)
        if new in d: continue
        assert d.count(old) == 1, (rel, old[:70])
        d = d.replace(old, new)
    p.write_bytes(d)

patch('public/bspfile.h', [
(b"#define MINBSPVERSION 19", b"// GammaSource: v18 (2002-2003) maps are converted to v19 in memory as they load.\n#define MINBSPVERSION 18"),
])

helper = b"""
//-----------------------------------------------------------------------------
// GammaSource: convert a whole BSP v18 file to v19 in place.
// v18 faces start with m_AvgLightColor[4] (16 bytes) that v19 dropped; the
// shorter face lumps stay at their offsets so every other lump (and the game
// lump's absolute offsets) is untouched. v18 leaves carry no ambient cube
// (leaf lump version 1) and v18 world lights are dworldlight_version0_t.
//-----------------------------------------------------------------------------
static bool BSP_ConvertV18ToV19InPlace( byte *pFile, int nFileSize )
{
	if ( nFileSize < (int)sizeof( dheader_t ) )
		return false;
	dheader_t *pHeader = (dheader_t *)pFile;
	if ( pHeader->ident != IDBSPHEADER || pHeader->version != 18 )
		return false;

	const int nFace18 = 72;
	for ( int i = 0; i < HEADER_LUMPS; ++i )
	{
		lump_t &lump = pHeader->lumps[i];
		if ( lump.filelen < 0 || ( lump.filelen && ( lump.fileofs < 0 || lump.fileofs + lump.filelen > nFileSize ) ) )
			return false;
		if ( ( i == LUMP_FACES || i == LUMP_ORIGINALFACES ) && lump.filelen )
		{
			if ( lump.filelen % nFace18 || sizeof( dface_t ) != nFace18 - 16 )
				return false;
			int nFaces = lump.filelen / nFace18;
			byte *pBase = pFile + lump.fileofs;
			for ( int f = 0; f < nFaces; ++f )
				memmove( pBase + f * sizeof( dface_t ), pBase + f * nFace18 + 16, sizeof( dface_t ) );
			memset( pBase + nFaces * sizeof( dface_t ), 0, lump.filelen - nFaces * sizeof( dface_t ) );
			lump.filelen = nFaces * sizeof( dface_t );
		}
		if ( i == LUMP_LEAFS )
			lump.version = 1;
		if ( i == LUMP_WORLDLIGHTS )
			lump.version = 0;
		lump.uncompressedSize = 0;	// v18 stored a fourCC here
	}
	// BSP18 predates the per-leaf sky visibility bits. Leaving them zero
	// prevents the 2018 client from submitting its sky pass (hall of mirrors).
	// Legacy BSP visibility is still enforced by the visible sky surfaces; the
	// leaf flag conservatively enables that pass wherever the map has a sky.
	lump_t &texLump = pHeader->lumps[LUMP_TEXINFO];
	lump_t &leafLump = pHeader->lumps[LUMP_LEAFS];
	if ( texLump.filelen % sizeof( texinfo_t ) || leafLump.filelen % sizeof( dleaf_t ) )
		return false;
	texinfo_t *pTexInfo = (texinfo_t *)( pFile + texLump.fileofs );
	bool bHasSky = false;
	for ( int i = 0; i < texLump.filelen / (int)sizeof( texinfo_t ); ++i )
		bHasSky = bHasSky || ( pTexInfo[i].flags & SURF_SKY ) != 0;
	if ( bHasSky )
	{
		dleaf_t *pLeaves = (dleaf_t *)( pFile + leafLump.fileofs );
		int nSkyLeaves = 0;
		for ( int i = 0; i < leafLump.filelen / (int)sizeof( dleaf_t ); ++i )
		{
			if ( !( pLeaves[i].contents & CONTENTS_SOLID ) )
			{
				pLeaves[i].flags |= LEAF_FLAGS_SKY;
				++nSkyLeaves;
			}
		}
		ConDMsg( "GammaSource BSP18: restored sky visibility in %d leaves\\n", nSkyLeaves );
	}
	pHeader->version = 19;
	return true;
}

void CMapLoadHelper::Init( model_t *pMapModel, const char *loadname )
"""

patch('engine/modelloader.cpp', [
(b"\nvoid CMapLoadHelper::Init( model_t *pMapModel, const char *loadname )\n", helper),
(b"""		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\\n", s_szMapName );
		return;
	}

	if ( s_MapHeader.version < MINBSPVERSION || s_MapHeader.version > BSPVERSION )
	{
		g_pFileSystem->Close( s_MapFileHandle );""",
b"""		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\\n", s_szMapName );
		return;
	}

	if ( s_MapHeader.version == 18 )
	{
		// GammaSource: 2002-2003 map. Read it whole and convert it to v19 in memory;
		// lumps are then served from s_MapBuffer (freed in Shutdown).
		int nSize = g_pFileSystem->Size( s_MapFileHandle );
		byte *pBuf = (byte *)malloc( nSize );
		g_pFileSystem->Seek( s_MapFileHandle, 0, FILESYSTEM_SEEK_HEAD );
		int nRead = pBuf ? g_pFileSystem->Read( pBuf, nSize, s_MapFileHandle ) : 0;
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		if ( nRead != nSize || !BSP_ConvertV18ToV19InPlace( pBuf, nSize ) )
		{
			free( pBuf );
			Host_Error( "CMapLoadHelper::Init, could not convert v18 map %s\\n", s_szMapName );
			return;
		}
		s_MapBuffer.SetExternalBuffer( pBuf, nSize, nSize );
		V_memcpy( &s_MapHeader, pBuf, sizeof( dheader_t ) );
	}

	if ( s_MapHeader.version < MINBSPVERSION || s_MapHeader.version > BSPVERSION )
	{
		if ( s_MapFileHandle != FILESYSTEM_INVALID_HANDLE )
			g_pFileSystem->Close( s_MapFileHandle );"""),
])
print("ok")
