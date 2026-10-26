//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//=============================================================================

#include "mempool.h"

#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <stddef.h>
#include <limits.h>
#include "tier0/dbg.h"
#include <ctype.h>
#include "tier0/memdbgon.h"

#undef max
#define max(x,y) (((x)>(y)) ? (x) : (y))

MemoryPoolReportFunc_t CMemoryPool::g_ReportFunc = 0;

//-----------------------------------------------------------------------------
// Error reporting...  (debug only)
//-----------------------------------------------------------------------------

void CMemoryPool::SetErrorReportFunc( MemoryPoolReportFunc_t func )
{
	g_ReportFunc = func;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

CMemoryPool::CMemoryPool(int blockSize, int numElements, int growMode)
{
	// The free list stores a pointer inside every block. Round its stride up
	// to pointer alignment so neighboring blocks are safe to dereference.
	m_BlockSize = blockSize < (int)sizeof(void*) ? (int)sizeof(void*) : blockSize;
	int alignmentRemainder = m_BlockSize % (int)sizeof(void*);
	if ( alignmentRemainder )
	{
		int padding = (int)sizeof(void*) - alignmentRemainder;
		m_BlockSize = m_BlockSize > INT_MAX - padding ? 0 : m_BlockSize + padding;
	}
	m_BlocksPerBlob = numElements;
	m_PeakAlloc = 0;
	m_GrowMode = growMode;
	Init();
	AddNewBlob();
}

//-----------------------------------------------------------------------------
// Purpose: Frees the memory contained in the mempool, and invalidates it for
//			any further use.
// Input  : *memPool - the mempool to shutdown
//-----------------------------------------------------------------------------
CMemoryPool::~CMemoryPool()
{
	if (m_BlocksAllocated > 0)
	{
		ReportLeaks();
	}
	Clear();
}


//-----------------------------------------------------------------------------
// Resets the pool
//-----------------------------------------------------------------------------
void CMemoryPool::Init()
{
	m_NumBlobs = 0;
	m_BlocksAllocated = 0;
	m_pHeadOfFreeList = 0;
	m_BlobHead.m_pNext = m_BlobHead.m_pPrev = &m_BlobHead;
}


//-----------------------------------------------------------------------------
// Frees everything
//-----------------------------------------------------------------------------
void CMemoryPool::Clear()
{
	// Free everything..
	CBlob *pNext;
	for( CBlob *pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pNext )
	{
		pNext = pCur->m_pNext;
		free( pCur );
	}
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Reports memory leaks 
//-----------------------------------------------------------------------------

void CMemoryPool::ReportLeaks()
{
	if (!g_ReportFunc)
		return;

	g_ReportFunc("Memory leak: mempool blocks left in memory: %d\n", m_BlocksAllocated);

#ifdef _DEBUG
	// walk and destroy the free list so it doesn't intefere in the scan
	while (m_pHeadOfFreeList != NULL)
	{
		void *next = *((void**)m_pHeadOfFreeList);
		memset(m_pHeadOfFreeList, 0, m_BlockSize);
		m_pHeadOfFreeList = next;
	}

	g_ReportFunc("Dumping memory: \'");

	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		// scan the memory block and dump the leaks
		char *scanPoint = (char *)pCur->m_Data;
		char *scanEnd = pCur->m_Data + pCur->m_NumBytes;
		bool needSpace = false;

		while (scanPoint < scanEnd)
		{
			// search for and dump any strings
			if (isprint(*scanPoint))
			{
				g_ReportFunc("%c", *scanPoint);
				needSpace = true;
			}
			else if (needSpace)
			{
				needSpace = false;
				g_ReportFunc(" ");
			}

			scanPoint++;
		}
	}

	g_ReportFunc("\'\n");
#endif // _DEBUG
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMemoryPool::AddNewBlob()
{
	if ( m_BlockSize <= 0 || m_BlocksPerBlob <= 0 || m_NumBlobs == USHRT_MAX )
		return;

	if ( m_GrowMode == GROW_NONE && m_NumBlobs != 0 )
		return;

	// Slow and fixed pools allocate a constant number of blocks; fast pools
	// grow by one initial allocation's worth on each successive blob.
	const int sizeMultiplier = m_GrowMode == GROW_FAST ? (int)m_NumBlobs + 1 : 1;
	if ( m_BlocksPerBlob > INT_MAX / sizeMultiplier )
		return;
	const int nElements = m_BlocksPerBlob * sizeMultiplier;
	if ( m_BlockSize > INT_MAX / nElements )
		return;
	const int blobSize = m_BlockSize * nElements;

	// m_Data's offset (rather than sizeof(CBlob)-1) keeps the allocation
	// correct for both 32-bit and 64-bit pointer-aligned CBlob layouts.
	const size_t allocationSize = offsetof(CBlob, m_Data) + (size_t)blobSize;
	CBlob *pBlob = (CBlob*)malloc( allocationSize );
	if ( !pBlob )
		return;

	pBlob->m_NumBytes = blobSize;
	pBlob->m_pNext = &m_BlobHead;
	pBlob->m_pPrev = pBlob->m_pNext->m_pPrev;
	pBlob->m_pNext->m_pPrev = pBlob->m_pPrev->m_pNext = pBlob;

	m_pHeadOfFreeList = pBlob->m_Data;
	void **newBlob = (void**)m_pHeadOfFreeList;
	for ( int j = 0; j < nElements - 1; ++j )
	{
		newBlob[0] = (char*)newBlob + m_BlockSize;
		newBlob = (void**)newBlob[0];
	}
	newBlob[0] = NULL;
	++m_NumBlobs;
}

void* CMemoryPool::Alloc()
{
	return Alloc( m_BlockSize );
}


//-----------------------------------------------------------------------------
// Purpose: Allocs a single block of memory from the pool.  
// Input  : amount - 
//-----------------------------------------------------------------------------
void *CMemoryPool::Alloc( unsigned int amount )
{
	void *returnBlock;

	if ( amount > (unsigned int)m_BlockSize )
		return NULL;

	if( !m_pHeadOfFreeList )
	{
		// returning NULL is fine in GROW_NONE
		if( m_GrowMode == GROW_NONE )
		{
			//Assert( !"CMemoryPool::Alloc: tried to make new blob with GROW_NONE" );
			return NULL;
		}

		// overflow
		AddNewBlob();

		// still failure, error out
		if( !m_pHeadOfFreeList )
		{
			// Out-of-memory or invalid capacity: do not dereference a null list.
			return NULL;
		}
	}
	m_BlocksAllocated++;
	m_PeakAlloc = max(m_PeakAlloc, m_BlocksAllocated);

	returnBlock = m_pHeadOfFreeList;

	// move the pointer the next block
	m_pHeadOfFreeList = *((void**)m_pHeadOfFreeList);

	return returnBlock;
}

//-----------------------------------------------------------------------------
// Purpose: Frees a block of memory
// Input  : *memBlock - the memory to free
//-----------------------------------------------------------------------------
void CMemoryPool::Free( void *memBlock )
{
	if ( !memBlock )
		return;  // trying to delete NULL pointer, ignore

	if ( m_BlocksAllocated <= 0 )
	{
		Assert( !"CMemoryPool::Free: freeing an unallocated block" );
		return;
	}

#ifdef _DEBUG
	// check to see if the memory is from the allocated range
	bool bOK = false;
	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		if (memBlock >= pCur->m_Data && (char*)memBlock < (pCur->m_Data + pCur->m_NumBytes))
		{
			bOK = true;
		}
	}
	Assert (bOK);
#endif // _DEBUG

#ifdef _DEBUG	
	// invalidate the memory
	memset( memBlock, 0xDD, m_BlockSize );
#endif

	m_BlocksAllocated--;

	// make the block point to the first item in the list
	*((void**)memBlock) = m_pHeadOfFreeList;

	// the list head is now the new block
	m_pHeadOfFreeList = memBlock;
}



