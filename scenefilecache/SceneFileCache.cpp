//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Cache for VCDs. PC async loads and uses the datacache to manage.
// 360 uses a baked resident image of aggregated compiled VCDs.
//
//=============================================================================

#include "scenefilecache/ISceneFileCache.h"
#include "filesystem.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzmaDecoder.h"
#include "scenefilecache/SceneImageFile.h"
#include "choreoscene.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IFileSystem	*filesystem = NULL;
																				
bool IsBufferBinaryVCD( char *pBuffer, int bufferSize )
{	
	if ( bufferSize > 4 && *(int *)pBuffer == SCENE_BINARY_TAG )
	{
		return true;	
	}

	return false;
}

class CSceneFileCache : public CBaseAppSystem< ISceneFileCache >
{
public:
	// IAppSystem
	virtual bool			Connect( CreateInterfaceFn factory );
	virtual void			Disconnect();
	virtual InitReturnVal_t Init();
	virtual void			Shutdown();

	// ISceneFileCache
	// Physically reloads image from disk
	virtual void			Reload();

	virtual size_t			GetSceneBufferSize( char const *pFilename );
	virtual bool			GetSceneData( char const *pFilename, byte *buf, size_t bufsize );

	// alternate resident image implementation
	virtual bool			GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData );
	virtual short			GetSceneCachedSound( int iScene, int iSound );
	virtual const char		*GetSceneString( short stringId );

private:
	// alternate implementation - uses a resident baked image of the file cache, contains all the compiled VCDs
	// single i/o read at startup to mount the image
	int						FindSceneInImage( const char *pSceneName );
	bool					GetSceneDataFromImage( const char *pSceneName, int iIndex, byte *pData, size_t *pLength );

	// GammaSource: loose (text) .vcd files on the GAME path, ahead of the image
	size_t					GetLooseSceneSize( const char *pFilename );
	bool					GetLooseSceneData( const char *pFilename, byte *buf, size_t bufsize );

private:
	CUtlBuffer						m_SceneImageFile;
};

bool CSceneFileCache::Connect( CreateInterfaceFn factory )
{
	if ( (filesystem = (IFileSystem *)factory( FILESYSTEM_INTERFACE_VERSION,NULL )) == NULL )
	{
		return false;
	}
	
	return true;
}

void CSceneFileCache::Disconnect()
{
}

InitReturnVal_t CSceneFileCache::Init()
{
	const char *pSceneImageName = IsX360() ? "scenes/scenes.360.image" : "scenes/scenes.image";

	if ( m_SceneImageFile.TellMaxPut() == 0 )
	{
		MEM_ALLOC_CREDIT();

		if ( filesystem->ReadFile( pSceneImageName, "GAME", m_SceneImageFile ) )
		{
			SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
			if ( pHeader->nId != SCENE_IMAGE_ID || 
				pHeader->nVersion != SCENE_IMAGE_VERSION )
			{
				Error( "CSceneFileCache: Bad scene image file %s\n", pSceneImageName );
			}
		}
		else
		{
			if ( IsX360() )
			{
				if ( filesystem->GetDVDMode() == DVDMODE_STRICT )
				{
					// mandatory
					Error( "CSceneFileCache: Failed to load %s\n", pSceneImageName );
				}
				else
				{
					// relaxed
					Warning( "CSceneFileCache: Failed to load %s, scene playback disabled.\n", pSceneImageName );
					return INIT_OK;
				}
			}

			m_SceneImageFile.Purge();
		}
	}

	return INIT_OK;
}

void CSceneFileCache::Shutdown()
{
	m_SceneImageFile.Purge();
}

// Physically reloads image from disk
void CSceneFileCache::Reload()
{
	Shutdown();
	Init();
}

size_t CSceneFileCache::GetSceneBufferSize( char const *pFilename )
{
	size_t returnSize = 0;

	char fn[MAX_PATH];
	Q_strncpy( fn, pFilename, sizeof( fn ) );
	Q_FixSlashes( fn );
	Q_strlower( fn );

	// GammaSource: the 2002-2003 scenes ship as loose text .vcd files and some share a name with
	// a retail scene in scenes.image; the loose file wins
	returnSize = GetLooseSceneSize( fn );
	if ( returnSize > 0 )
		return returnSize;

	GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), NULL, &returnSize );
	return returnSize;
}

size_t CSceneFileCache::GetLooseSceneSize( const char *pFilename )
{
	char fn[MAX_PATH];
	V_strncpy( fn, pFilename, sizeof( fn ) );
	V_SetExtension( fn, ".vcd", sizeof( fn ) );
	if ( !filesystem->FileExists( fn, "GAME" ) )
		return 0;
	// One extra byte for a terminator: callers hand text scenes straight to the
	// tokenizer, which reads until NUL.
	return (size_t)filesystem->Size( fn, "GAME" ) + 1;
}

bool CSceneFileCache::GetLooseSceneData( const char *pFilename, byte *buf, size_t bufsize )
{
	char fn[MAX_PATH];
	V_strncpy( fn, pFilename, sizeof( fn ) );
	V_SetExtension( fn, ".vcd", sizeof( fn ) );
	CUtlBuffer file;
	if ( !filesystem->ReadFile( fn, "GAME", file ) || !bufsize )
		return false;
	size_t nCopy = MIN( bufsize - 1, (size_t)file.TellMaxPut() );
	V_memcpy( buf, file.Base(), nCopy );
	memset( buf + nCopy, 0, bufsize - nCopy );
	return true;
}

bool CSceneFileCache::GetSceneData( char const *pFilename, byte *buf, size_t bufsize )
{
	Assert( pFilename );
	Assert( buf );
	Assert( bufsize > 0 );

	char fn[MAX_PATH];
	Q_strncpy( fn, pFilename, sizeof( fn ) );
	Q_FixSlashes( fn );
	Q_strlower( fn );

	if ( GetLooseSceneSize( fn ) > 0 )
		return GetLooseSceneData( fn, buf, bufsize );

	size_t nLength = bufsize;
	return GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), buf, &nLength );
}

bool CSceneFileCache::GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData )
{
	int iScene = FindSceneInImage( pFilename );
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// not available
		pData->sceneId = -1;
		pData->msecs = 0;
		pData->numSounds = 0;
		return false;
	}

	// get scene summary
	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	SceneImageSummary_t *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	
	pData->sceneId = iScene;
	pData->msecs = pSummary->msecs;
	pData->numSounds = pSummary->numSounds;

	return true;
}

short CSceneFileCache::GetSceneCachedSound( int iScene, int iSound )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// huh?, image file not present or bad index
		return -1;
	}

	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	SceneImageSummary_t *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	if ( iSound < 0 || iSound >= pSummary->numSounds )
	{
		// bad index
		Assert( 0 );
		return -1;
	}

	return pSummary->soundStrings[iSound];
}

const char *CSceneFileCache::GetSceneString( short stringId )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
	if ( !pHeader || stringId < 0 || stringId >= pHeader->nNumStrings )
	{
		// huh?, image file not present, or index bad
		return NULL;
	}

	return pHeader->String( stringId );
}

//-----------------------------------------------------------------------------
//  Returns -1 if not found, otherwise [0..n] index.
//-----------------------------------------------------------------------------
int CSceneFileCache::FindSceneInImage( const char *pSceneName )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
	if ( !pHeader )
	{
		return -1;
	}
	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );

	char szCleanName[MAX_PATH];

	V_strncpy( szCleanName, pSceneName, sizeof( szCleanName ) );
	V_strlower( szCleanName );
#ifdef POSIX
	V_FixSlashes( szCleanName, '\\' );
#else
	V_FixSlashes( szCleanName );
#endif
	V_SetExtension( szCleanName, ".vcd", sizeof( szCleanName ) );

	CRC32_t crcFilename = CRC32_ProcessSingleBuffer( szCleanName, strlen( szCleanName ) );

	// use binary search, entries are sorted by ascending crc
	int nLowerIdx = 1;
	int nUpperIdx = pHeader->nNumScenes;
	for ( ;; )
	{
		if ( nUpperIdx < nLowerIdx )
		{
			return -1;
		}
		else
		{
			int nMiddleIndex = ( nLowerIdx + nUpperIdx )/2;
			CRC32_t nProbe = pEntries[nMiddleIndex-1].crcFilename;
			if ( crcFilename < nProbe )
			{
				nUpperIdx = nMiddleIndex - 1;
			}
			else
			{
				if ( crcFilename > nProbe )
				{
					nLowerIdx = nMiddleIndex + 1;
				}
				else
				{
					return nMiddleIndex - 1;
				}
			}
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
//  Returns true if success, false otherwise. Caller must free ouput scene data
//-----------------------------------------------------------------------------
bool CSceneFileCache::GetSceneDataFromImage( const char *pFileName, int iScene, byte *pSceneData, size_t *pSceneLength )
{
	SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		if ( pSceneData )
		{
			*pSceneData = NULL;
		}
		if ( pSceneLength )
		{
			*pSceneLength = 0;
		}
		return false;
	}

	SceneImageEntry_t *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	unsigned char *pData = (unsigned char *)pHeader + pEntries[iScene].nDataOffset;
	bool bIsCompressed;
	bIsCompressed = CLZMA::IsCompressed( pData );
	if ( bIsCompressed )
	{
		int originalSize = CLZMA::GetActualSize( pData );
		if ( pSceneData )
		{
			int nMaxLen = *pSceneLength;
			if ( originalSize <= nMaxLen )
			{
				CLZMA::Uncompress( pData, pSceneData );
			}
			else
			{
				unsigned char *pOutputData = (unsigned char *)malloc( originalSize );
				CLZMA::Uncompress( pData, pOutputData );
				V_memcpy( pSceneData, pOutputData, nMaxLen );
				free( pOutputData );
			}
		}
		if ( pSceneLength )
		{
			*pSceneLength = originalSize;
		}
	}
	else
	{
		if ( pSceneData )
		{
			size_t nCountToCopy = min(*pSceneLength, (size_t)pEntries[iScene].nDataLength );
			V_memcpy( pSceneData, pData, nCountToCopy );
		}
		if ( pSceneLength )
		{
			*pSceneLength = (size_t)pEntries[iScene].nDataLength;
		}
	}
	return true;
}

static CSceneFileCache g_SceneFileCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSceneFileCache, ISceneFileCache, SCENE_FILE_CACHE_INTERFACE_VERSION, g_SceneFileCache );
