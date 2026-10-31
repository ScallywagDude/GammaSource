//========= GammaSource ============//
//
// Native Direct3D 12 renderer for shaderapidx12 (kept free of Source headers): device, swap
// chain, textures, render targets, pipelines and the textured, vertex-coloured, lightmapped
// draw path the materials' passes go through. It implements the same functions as the DX11
// and Vulkan backends (dx12device.h, dx12render.h) and runs the same shader
// (rendershared.inl), compiled at run time.
//
// d3d12.dll is loaded at run time, so the game still starts where Direct3D 12 is missing
// (the launcher then falls back to DirectX 9).
//
// Frames: the game draws into an offscreen back buffer that is copied into the swap chain
// buffer at Present. Each texture's resource state is tracked and changed where it is used.
// Uploads, per-draw constants and vertices used once come from per-frame upload memory.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <math.h>
#include "dx12device.h"
#include "dx12render.h"

#pragma comment( lib, "user32.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

//-----------------------------------------------------------------------------
// Device state
//-----------------------------------------------------------------------------
enum { FRAMES = 2, SWAP_BUFFERS = 2, STREAM_SIZE = 16 * 1024 * 1024, BLOCK_SIZE = 64 * 1024 * 1024, MAX_QUERIES = 4096,
	DRAW_VIEWS = 7, VIEW_RING = DRAW_VIEWS * 32768, HEAP_CHUNK = 1024 };

static HMODULE s_hD3D12;
static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE s_pfnSerializeRootSignature;
static IDXGIFactory4 *s_pFactory;
static ID3D12Device *s_pDevice;
static ID3D12CommandQueue *s_pQueue;
static IDXGISwapChain3 *s_pSwapChain;
static ID3D12Resource *s_pSwapBuffers[SWAP_BUFFERS];
static int s_nSwapWidth, s_nSwapHeight;
static ID3D12CommandAllocator *s_pAllocators[FRAMES];
static ID3D12GraphicsCommandList *s_pCmd;
static ID3D12Fence *s_pFence;
static HANDLE s_hFenceEvent;
static UINT64 s_nFenceValue;			// last value signalled
static UINT64 s_SlotFence[FRAMES];		// what each slot's last submission signals
static int s_nFrame;					// current slot
static bool s_bRecording;				// the slot's command list is open
static HWND s_hWnd;
static float s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static char s_szAdapterName[256] = "Direct3D 12";
static const DXGI_FORMAT s_ColorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
static const DXGI_FORMAT s_DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
static const D3D12_RESOURCE_STATES s_SampleState = (D3D12_RESOURCE_STATES)( D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );

#define SAFE_RELEASE( p ) if ( p ) { ( p )->Release(); ( p ) = NULL; }

void Dx12_DebugLog( const char *pszMsg, int n )
{
	static char s_szFile[MAX_PATH];
	static const char *s_pszFile = GetEnvironmentVariableA( "GAMMA_DX12_LOG", s_szFile, MAX_PATH ) ? s_szFile : NULL;
	if ( !s_pszFile )
		return;
	FILE *f = fopen( s_pszFile, "a" );
	if ( f ) { fprintf( f, "%s %d\n", pszMsg, n ); fclose( f ); }
}

static void WaitForFence( UINT64 nValue )
{
	if ( !s_pFence || s_pFence->GetCompletedValue() >= nValue )
		return;
	s_pFence->SetEventOnCompletion( nValue, s_hFenceEvent );
	WaitForSingleObject( s_hFenceEvent, INFINITE );
}

static void WaitIdle()
{
	if ( !s_pQueue || !s_pFence )
		return;
	s_pQueue->Signal( s_pFence, ++s_nFenceValue );
	WaitForFence( s_nFenceValue );
}

//-----------------------------------------------------------------------------
// CPU descriptors (textures' views), handed out 1-based: 0 is none
//-----------------------------------------------------------------------------
struct DescriptorPool_t
{
	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	UINT m_nIncrement;
	std::vector< ID3D12DescriptorHeap * > m_Heaps;
	std::vector< int > m_Free;
	int m_nNext;
};
static DescriptorPool_t s_SRVPool, s_RTVPool, s_DSVPool;

static int AllocDescriptor( DescriptorPool_t &pool )
{
	if ( !pool.m_Free.empty() )
	{
		int n = pool.m_Free.back();
		pool.m_Free.pop_back();
		return n;
	}
	if ( pool.m_nNext >= (int)pool.m_Heaps.size() * HEAP_CHUNK )
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd = {};
		hd.Type = pool.m_Type;
		hd.NumDescriptors = HEAP_CHUNK;
		ID3D12DescriptorHeap *pHeap = NULL;
		if ( FAILED( s_pDevice->CreateDescriptorHeap( &hd, IID_PPV_ARGS( &pHeap ) ) ) )
			return 0;
		pool.m_Heaps.push_back( pHeap );
		pool.m_nIncrement = s_pDevice->GetDescriptorHandleIncrementSize( pool.m_Type );
	}
	return ++pool.m_nNext;
}

static D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle( DescriptorPool_t &pool, int n )
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = pool.m_Heaps[( n - 1 ) / HEAP_CHUNK]->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)( ( n - 1 ) % HEAP_CHUNK ) * pool.m_nIncrement;
	return h;
}

static void FreeDescriptor( DescriptorPool_t &pool, int &n )
{
	if ( n > 0 )
		pool.m_Free.push_back( n );
	n = 0;
}

static void DestroyPool( DescriptorPool_t &pool )
{
	for ( size_t i = 0; i < pool.m_Heaps.size(); ++i )
		pool.m_Heaps[i]->Release();
	pool.m_Heaps.clear();
	pool.m_Free.clear();
	pool.m_nNext = 0;
}

//-----------------------------------------------------------------------------
// Upload memory: big mapped buffers carved up first-fit (cached static vertices)
//-----------------------------------------------------------------------------
struct Allocation_t
{
	ID3D12Resource *m_pBuffer;
	UINT64 m_nOffset, m_nSize;
	int m_nBlock;			// -1: a buffer of its own
	unsigned char *m_pMapped;
};

struct Block_t
{
	ID3D12Resource *m_pBuffer;
	unsigned char *m_pMapped;
	std::vector< std::pair< UINT64, UINT64 > > m_Free;	// offset, size
};
static std::vector< Block_t > s_Blocks;

static ID3D12Resource *CreateUploadBuffer( UINT64 nSize, unsigned char **ppMapped )
{
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = nSize;
	rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ID3D12Resource *pBuffer = NULL;
	if ( FAILED( s_pDevice->CreateCommittedResource( &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
		IID_PPV_ARGS( &pBuffer ) ) ) )
		return NULL;
	D3D12_RANGE none = { 0, 0 };
	if ( FAILED( pBuffer->Map( 0, &none, (void **)ppMapped ) ) )
	{
		pBuffer->Release();
		return NULL;
	}
	return pBuffer;
}

static bool Allocate( UINT64 nSize, Allocation_t &out )
{
	memset( &out, 0, sizeof( out ) );
	nSize = ( nSize + 255 ) & ~(UINT64)255;
	if ( nSize > BLOCK_SIZE / 2 )
	{
		out.m_pBuffer = CreateUploadBuffer( nSize, &out.m_pMapped );
		out.m_nSize = nSize;
		out.m_nBlock = -1;
		return out.m_pBuffer != NULL;
	}
	for ( int pass = 0; pass < 2; ++pass )
	{
		for ( size_t b = 0; b < s_Blocks.size(); ++b )
		{
			Block_t &block = s_Blocks[b];
			for ( size_t f = 0; f < block.m_Free.size(); ++f )
			{
				if ( block.m_Free[f].second < nSize )
					continue;
				out.m_pBuffer = block.m_pBuffer;
				out.m_nOffset = block.m_Free[f].first;
				out.m_nSize = nSize;
				out.m_nBlock = (int)b;
				out.m_pMapped = block.m_pMapped + out.m_nOffset;
				block.m_Free[f].first += nSize;
				block.m_Free[f].second -= nSize;
				if ( !block.m_Free[f].second )
					block.m_Free.erase( block.m_Free.begin() + f );
				return true;
			}
		}
		if ( pass == 1 )
			break;
		Block_t block;
		block.m_pBuffer = CreateUploadBuffer( BLOCK_SIZE, &block.m_pMapped );
		if ( !block.m_pBuffer )
			return false;
		block.m_Free.push_back( std::make_pair( (UINT64)0, (UINT64)BLOCK_SIZE ) );
		s_Blocks.push_back( block );
	}
	return false;
}

static void Free( Allocation_t &a )
{
	if ( !a.m_pBuffer )
		return;
	if ( a.m_nBlock < 0 )
	{
		a.m_pBuffer->Release();
	}
	else if ( a.m_nBlock < (int)s_Blocks.size() )
	{
		Block_t &block = s_Blocks[a.m_nBlock];
		UINT64 nStart = a.m_nOffset, nEnd = a.m_nOffset + a.m_nSize;
		// merge with neighbours
		for ( size_t f = 0; f < block.m_Free.size(); )
		{
			UINT64 fs = block.m_Free[f].first, fe = fs + block.m_Free[f].second;
			if ( fe == nStart || fs == nEnd )
			{
				if ( fe == nStart ) nStart = fs; else nEnd = fe;
				block.m_Free.erase( block.m_Free.begin() + f );
				f = 0;
				continue;
			}
			++f;
		}
		block.m_Free.push_back( std::make_pair( nStart, nEnd - nStart ) );
	}
	memset( &a, 0, sizeof( a ) );
}

//-----------------------------------------------------------------------------
// Deferred destruction: freed once the frame that used the object has finished
//-----------------------------------------------------------------------------
struct Garbage_t
{
	ID3D12Resource *m_pResource;
	Allocation_t m_Memory;
};
static std::vector< Garbage_t > s_Garbage[FRAMES];

static void CollectGarbage( int nSlot )
{
	for ( size_t i = 0; i < s_Garbage[nSlot].size(); ++i )
	{
		SAFE_RELEASE( s_Garbage[nSlot][i].m_pResource );
		Free( s_Garbage[nSlot][i].m_Memory );
	}
	s_Garbage[nSlot].clear();
}

static void ReleaseLater( ID3D12Resource *pResource )
{
	if ( !pResource )
		return;
	Garbage_t g = {};
	g.m_pResource = pResource;
	s_Garbage[s_nFrame].push_back( g );
}

//-----------------------------------------------------------------------------
// Per-frame streaming memory (vertices/indices used once, constants, upload staging)
//-----------------------------------------------------------------------------
struct StreamBuffer_t
{
	ID3D12Resource *m_pBuffer;
	unsigned char *m_pMapped;
	UINT64 m_nSize, m_nUsed;
};
static std::vector< StreamBuffer_t > s_Stream[FRAMES];

// mapped memory for this frame; valid until the slot comes around again
static unsigned char *StreamAlloc( UINT64 nBytes, UINT64 nAlign, ID3D12Resource *&pBuffer, UINT64 &nOffset )
{
	std::vector< StreamBuffer_t > &stream = s_Stream[s_nFrame];
	for ( size_t i = 0; i < stream.size(); ++i )
	{
		StreamBuffer_t &sb = stream[i];
		UINT64 nStart = ( sb.m_nUsed + nAlign - 1 ) / nAlign * nAlign;
		if ( nStart + nBytes <= sb.m_nSize )
		{
			sb.m_nUsed = nStart + nBytes;
			pBuffer = sb.m_pBuffer;
			nOffset = nStart;
			return sb.m_pMapped + nStart;
		}
	}
	StreamBuffer_t sb = {};
	sb.m_nSize = nBytes > STREAM_SIZE ? ( nBytes + 255 ) & ~(UINT64)255 : STREAM_SIZE;
	sb.m_pBuffer = CreateUploadBuffer( sb.m_nSize, &sb.m_pMapped );
	if ( !sb.m_pBuffer )
		return NULL;
	sb.m_nUsed = nBytes;
	stream.push_back( sb );
	pBuffer = sb.m_pBuffer;
	nOffset = 0;
	return sb.m_pMapped;
}

static unsigned char *StreamAlloc( UINT64 nBytes, D3D12_GPU_VIRTUAL_ADDRESS &address )
{
	ID3D12Resource *pBuffer;
	UINT64 nOffset;
	unsigned char *p = StreamAlloc( nBytes, 256, pBuffer, nOffset );
	address = p ? pBuffer->GetGPUVirtualAddress() + nOffset : 0;
	return p;
}

// big frames (level load) would grow the stream without bound: past this, wait for the GPU
static UINT64 StreamUsed()
{
	UINT64 n = 0;
	for ( size_t i = 0; i < s_Stream[s_nFrame].size(); ++i )
		n += s_Stream[s_nFrame][i].m_nUsed;
	return n;
}

//-----------------------------------------------------------------------------
// Textures and render targets
//-----------------------------------------------------------------------------
struct Texture_t
{
	ID3D12Resource *m_pResource;
	D3D12_RESOURCE_STATES m_State;
	int m_nSRV;						// CPU descriptor (1-based)
	int m_nWidth, m_nHeight, m_nMips;
	bool m_bClampS, m_bClampT;
	bool m_bUsed;
	bool m_bHasData;
	bool m_bRenderTarget;
	bool m_bCube;
	unsigned char *m_pPending[16];	// mip levels given before the device existed
	unsigned char *m_pShadow;		// see Dx12R_LockShadow
	// render targets
	ID3D12Resource *m_pDepth;
	int m_nRTV, m_nDSV;
};
static std::vector< Texture_t > s_Textures;	// handle = index + 1
static Texture_t s_BackBuffer;			// what the game draws into; copied to the swap chain
static int s_hWhite;
static int s_hWhiteCube;				// bound when a pass has no envmap
static int s_hRenderTarget;				// 0: the back buffer
static ID3D12Resource *s_pBoundTarget;	// bound with OMSetRenderTargets in the open command list
int g_nDx12Binds;

static Texture_t *GetTexture( int hTexture )
{
	if ( hTexture <= 0 || hTexture > (int)s_Textures.size() || !s_Textures[hTexture - 1].m_bUsed )
		return NULL;
	return &s_Textures[hTexture - 1];
}

static int MipSize( int n, int nLevel )
{
	n >>= nLevel;
	return n > 0 ? n : 1;
}

static void BeginRecording();
static void Transition( ID3D12Resource *pResource, D3D12_RESOURCE_STATES &state, D3D12_RESOURCE_STATES newState )
{
	if ( state == newState || !pResource )
		return;
	D3D12_RESOURCE_BARRIER b = {};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = pResource;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	b.Transition.StateBefore = state;
	b.Transition.StateAfter = newState;
	s_pCmd->ResourceBarrier( 1, &b );
	state = newState;
}

static void UploadRegion( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch, int nLayer = 0 );

// creates the GPU texture (and for render targets the depth buffer and views)
static bool EnsureResource( Texture_t *pTex )
{
	if ( pTex->m_pResource )
		return true;
	if ( !s_pDevice )
		return false;
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = pTex->m_nWidth;
	rd.Height = pTex->m_nHeight;
	rd.DepthOrArraySize = pTex->m_bCube ? 6 : 1;
	rd.MipLevels = (UINT16)pTex->m_nMips;
	rd.Format = s_ColorFormat;
	rd.SampleDesc.Count = 1;
	rd.Flags = pTex->m_bRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES initial = pTex->m_bRenderTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_DEST;
	D3D12_CLEAR_VALUE clear = {};
	clear.Format = s_ColorFormat;
	if ( FAILED( s_pDevice->CreateCommittedResource( &hp, D3D12_HEAP_FLAG_NONE, &rd, initial, pTex->m_bRenderTarget ? &clear : NULL,
		IID_PPV_ARGS( &pTex->m_pResource ) ) ) )
	{
		pTex->m_pResource = NULL;
		return false;
	}
	pTex->m_State = initial;
	D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
	sd.Format = s_ColorFormat;
	sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if ( pTex->m_bCube )
	{
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		sd.TextureCube.MipLevels = pTex->m_nMips;
	}
	else
	{
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = pTex->m_nMips;
	}
	pTex->m_nSRV = AllocDescriptor( s_SRVPool );
	if ( pTex->m_nSRV )
		s_pDevice->CreateShaderResourceView( pTex->m_pResource, &sd, DescriptorHandle( s_SRVPool, pTex->m_nSRV ) );
	if ( pTex->m_bRenderTarget )
	{
		rd.Format = s_DepthFormat;
		rd.MipLevels = 1;
		rd.DepthOrArraySize = 1;
		rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE depthClear = {};
		depthClear.Format = s_DepthFormat;
		depthClear.DepthStencil.Depth = 1.0f;
		if ( FAILED( s_pDevice->CreateCommittedResource( &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
			IID_PPV_ARGS( &pTex->m_pDepth ) ) ) )
		{
			pTex->m_pDepth = NULL;
			return false;
		}
		pTex->m_nRTV = AllocDescriptor( s_RTVPool );
		pTex->m_nDSV = AllocDescriptor( s_DSVPool );
		if ( !pTex->m_nRTV || !pTex->m_nDSV )
			return false;
		s_pDevice->CreateRenderTargetView( pTex->m_pResource, NULL, DescriptorHandle( s_RTVPool, pTex->m_nRTV ) );
		s_pDevice->CreateDepthStencilView( pTex->m_pDepth, NULL, DescriptorHandle( s_DSVPool, pTex->m_nDSV ) );
		// start out black with a cleared depth buffer
		BeginRecording();
		s_pCmd->ClearRenderTargetView( DescriptorHandle( s_RTVPool, pTex->m_nRTV ), clear.Color, 0, NULL );
		s_pCmd->ClearDepthStencilView( DescriptorHandle( s_DSVPool, pTex->m_nDSV ), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0, 0, NULL );
		pTex->m_bHasData = true;
	}
	// data that arrived before the device existed
	for ( int i = 0; i < 16 && i < pTex->m_nMips; ++i )
	{
		if ( !pTex->m_pPending[i] )
			continue;
		UploadRegion( pTex, i, 0, 0, MipSize( pTex->m_nWidth, i ), MipSize( pTex->m_nHeight, i ), pTex->m_pPending[i], MipSize( pTex->m_nWidth, i ) * 4 );
		free( pTex->m_pPending[i] );
		pTex->m_pPending[i] = NULL;
	}
	return true;
}

static void DestroyTexture( Texture_t *pTex, bool bDeferred )
{
	if ( pTex->m_pResource == s_pBoundTarget )
		s_pBoundTarget = NULL;
	if ( bDeferred )
	{
		ReleaseLater( pTex->m_pResource );
		ReleaseLater( pTex->m_pDepth );
	}
	else
	{
		SAFE_RELEASE( pTex->m_pResource );
		SAFE_RELEASE( pTex->m_pDepth );
	}
	FreeDescriptor( s_SRVPool, pTex->m_nSRV );
	FreeDescriptor( s_RTVPool, pTex->m_nRTV );
	FreeDescriptor( s_DSVPool, pTex->m_nDSV );
	for ( int i = 0; i < 16; ++i ) free( pTex->m_pPending[i] );
	free( pTex->m_pShadow );
	memset( pTex, 0, sizeof( *pTex ) );
}

//-----------------------------------------------------------------------------
// Pipelines
//-----------------------------------------------------------------------------
static ID3D12RootSignature *s_pRootSignature;
static ID3DBlob *s_pVS, *s_pPS;
static ID3D12DescriptorHeap *s_pViewHeaps[FRAMES];	// shader-visible ring of each draw's textures
static UINT s_nViewUsed[FRAMES];
static UINT s_nViewIncrement;
static ID3D12DescriptorHeap *s_pSamplerHeap;		// 4 samplers: bit 0 clamp S, bit 1 clamp T
static UINT s_nSamplerIncrement;
static std::map< unsigned int, ID3D12PipelineState * > s_Pipelines;
static ID3D12PipelineState *s_pBoundPipeline;
static bool s_bRootBound;
int g_nDx12Draws, g_nDx12Meshes, g_nDx12Passes, g_nDx12EmptyMeshes;
static D3D12_VIEWPORT s_Viewport;
static bool s_bViewportSet;

#include "rendershared.inl"

static const D3D12_BLEND s_BlendMap[] =
{
	D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_SRC_ALPHA_SAT, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR
};
static const D3D12_COMPARISON_FUNC s_DepthMap[] =
{
	D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_LESS, D3D12_COMPARISON_FUNC_EQUAL, D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER, D3D12_COMPARISON_FUNC_NOT_EQUAL, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_ALWAYS
};
static const D3D12_PRIMITIVE_TOPOLOGY s_Topology[] =
{
	D3D_PRIMITIVE_TOPOLOGY_POINTLIST, D3D_PRIMITIVE_TOPOLOGY_LINELIST, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, D3D_PRIMITIVE_TOPOLOGY_LINESTRIP
};
static const D3D12_PRIMITIVE_TOPOLOGY_TYPE s_TopologyType[] =
{
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
};

static ID3D12PipelineState *GetPipeline( Dx12Primitive_t type )
{
	int nSrc = ( s_State.m_nSrcBlend >= 0 && s_State.m_nSrcBlend <= 10 ) ? s_State.m_nSrcBlend : 1;
	int nDst = ( s_State.m_nDstBlend >= 0 && s_State.m_nDstBlend <= 10 ) ? s_State.m_nDstBlend : 0;
	int nDepthFunc = ( s_State.m_nDepthFunc >= 0 && s_State.m_nDepthFunc <= 7 ) ? s_State.m_nDepthFunc : 3;
	// pipelines differ only in topology type, not in points/lists/strips of it
	int nTopologyType = (int)s_TopologyType[type];
	unsigned int nKey = ( s_State.m_bBlend ? 1 : 0 ) | ( nSrc << 1 ) | ( nDst << 5 ) | ( s_State.m_bDepthTest ? 1 << 9 : 0 ) |
		( s_State.m_bDepthWrite ? 1 << 10 : 0 ) | ( nDepthFunc << 11 ) | ( s_State.m_bCull ? 1 << 14 : 0 ) | ( (unsigned)nTopologyType << 15 ) |
		( s_State.m_bDecalOffset ? 1 << 18 : 0 ) | ( s_State.m_bNoColorWrites ? 1 << 19 : 0 ) | ( s_State.m_bNoAlphaWrites ? 1 << 20 : 0 );
	std::map< unsigned int, ID3D12PipelineState * >::iterator it = s_Pipelines.find( nKey );
	if ( it != s_Pipelines.end() )
		return it->second;

	static const D3D12_INPUT_ELEMENT_DESC s_Layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 68, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, 80, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
	pd.pRootSignature = s_pRootSignature;
	pd.VS.pShaderBytecode = s_pVS->GetBufferPointer(); pd.VS.BytecodeLength = s_pVS->GetBufferSize();
	pd.PS.pShaderBytecode = s_pPS->GetBufferPointer(); pd.PS.BytecodeLength = s_pPS->GetBufferSize();
	D3D12_RENDER_TARGET_BLEND_DESC &bd = pd.BlendState.RenderTarget[0];
	bd.BlendEnable = s_State.m_bBlend;
	bd.SrcBlend = s_BlendMap[nSrc];
	bd.DestBlend = s_BlendMap[nDst];
	bd.BlendOp = D3D12_BLEND_OP_ADD;
	bd.SrcBlendAlpha = D3D12_BLEND_ONE;
	bd.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	bd.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	bd.LogicOp = D3D12_LOGIC_OP_NOOP;
	bd.RenderTargetWriteMask = (UINT8)( ( s_State.m_bNoColorWrites ? 0 : ( D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN |
		D3D12_COLOR_WRITE_ENABLE_BLUE ) ) | ( s_State.m_bNoAlphaWrites ? 0 : D3D12_COLOR_WRITE_ENABLE_ALPHA ) );
	pd.SampleMask = UINT_MAX;
	pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pd.RasterizerState.CullMode = s_State.m_bCull ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
	pd.RasterizerState.FrontCounterClockwise = FALSE;	// as D3D9: front faces are clockwise
	pd.RasterizerState.DepthClipEnable = TRUE;
	if ( s_State.m_bDecalOffset )
	{
		// as D3D9's decal bias (mat_depthbias_decal / mat_slopescaledepthbias_decal)
		pd.RasterizerState.DepthBias = -84;
		pd.RasterizerState.SlopeScaledDepthBias = -0.5f;
	}
	pd.DepthStencilState.DepthEnable = s_State.m_bDepthTest || s_State.m_bDepthWrite;
	pd.DepthStencilState.DepthWriteMask = s_State.m_bDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	pd.DepthStencilState.DepthFunc = s_State.m_bDepthTest ? s_DepthMap[nDepthFunc] : D3D12_COMPARISON_FUNC_ALWAYS;
	D3D12_DEPTH_STENCILOP_DESC keep = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	pd.DepthStencilState.FrontFace = pd.DepthStencilState.BackFace = keep;
	pd.InputLayout.pInputElementDescs = s_Layout;
	pd.InputLayout.NumElements = 7;
	pd.PrimitiveTopologyType = s_TopologyType[type];
	pd.NumRenderTargets = 1;
	pd.RTVFormats[0] = s_ColorFormat;
	pd.DSVFormat = s_DepthFormat;
	pd.SampleDesc.Count = 1;
	ID3D12PipelineState *pPipeline = NULL;
	if ( FAILED( s_pDevice->CreateGraphicsPipelineState( &pd, IID_PPV_ARGS( &pPipeline ) ) ) )
	{
		Dx12_DebugLog( "CreateGraphicsPipelineState failed, key", (int)nKey );
		pPipeline = NULL;
	}
	s_Pipelines[nKey] = pPipeline;
	return pPipeline;
}

//-----------------------------------------------------------------------------
// Frames
//-----------------------------------------------------------------------------
static void BeginRecording()
{
	if ( s_bRecording || !s_pDevice )
		return;
	WaitForFence( s_SlotFence[s_nFrame] );
	CollectGarbage( s_nFrame );
	for ( size_t i = 0; i < s_Stream[s_nFrame].size(); ++i )
		s_Stream[s_nFrame][i].m_nUsed = 0;
	s_nViewUsed[s_nFrame] = 0;
	s_pAllocators[s_nFrame]->Reset();
	s_pCmd->Reset( s_pAllocators[s_nFrame], NULL );
	ID3D12DescriptorHeap *heaps[2] = { s_pViewHeaps[s_nFrame], s_pSamplerHeap };
	s_pCmd->SetDescriptorHeaps( 2, heaps );
	s_pBoundTarget = NULL;
	s_pBoundPipeline = NULL;
	s_bRootBound = false;
	s_bRecording = true;
}

static int s_nOpenQuery;
void Dx12R_EndQuery( int hQuery );

static void Submit()
{
	if ( !s_bRecording )
		return;
	if ( s_nOpenQuery )
		Dx12R_EndQuery( s_nOpenQuery );
	s_pCmd->Close();
	ID3D12CommandList *lists[1] = { s_pCmd };
	s_pQueue->ExecuteCommandLists( 1, lists );
	s_pQueue->Signal( s_pFence, ++s_nFenceValue );
	s_SlotFence[s_nFrame] = s_nFenceValue;
	s_bRecording = false;
}

// submit what is recorded and wait for it: used when a frame's streaming memory gets
// large (level loads upload hundreds of megabytes before the first Present)
static void SubmitAndWait()
{
	if ( !s_bRecording )
		return;
	Submit();
	BeginRecording();	// waits on the fence and recycles the slot
}

static Texture_t *CurrentTarget()
{
	Texture_t *pTex = GetTexture( s_hRenderTarget );
	return ( pTex && pTex->m_nRTV ) ? pTex : &s_BackBuffer;
}

// the current target, ready to be drawn into
static Texture_t *BindTarget()
{
	Texture_t *pTarget = CurrentTarget();
	if ( !pTarget->m_pResource || !pTarget->m_nRTV )
		return NULL;
	Transition( pTarget->m_pResource, pTarget->m_State, D3D12_RESOURCE_STATE_RENDER_TARGET );
	if ( s_pBoundTarget != pTarget->m_pResource )
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = DescriptorHandle( s_RTVPool, pTarget->m_nRTV );
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = DescriptorHandle( s_DSVPool, pTarget->m_nDSV );
		s_pCmd->OMSetRenderTargets( 1, &rtv, FALSE, &dsv );
		s_pBoundTarget = pTarget->m_pResource;
	}
	return pTarget;
}

//-----------------------------------------------------------------------------
// Uploads
//-----------------------------------------------------------------------------
static void UploadRegion( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch, int nLayer )
{
	if ( nWidth <= 0 || nHeight <= 0 )
		return;
	BeginRecording();
	if ( StreamUsed() > 256 * 1024 * 1024 )
		SubmitAndWait();
	UINT nRowPitch = ( nWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1 ) & ~( D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1 );
	ID3D12Resource *pBuffer;
	UINT64 nOffset;
	unsigned char *pDst = StreamAlloc( (UINT64)nRowPitch * nHeight, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, pBuffer, nOffset );
	if ( !pDst )
		return;
	for ( int row = 0; row < nHeight; ++row )
		memcpy( pDst + row * nRowPitch, pBGRA + row * nPitch, nWidth * 4 );
	Transition( pTex->m_pResource, pTex->m_State, D3D12_RESOURCE_STATE_COPY_DEST );
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = pTex->m_pResource;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = nLevel + nLayer * pTex->m_nMips;
	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = pBuffer;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = nOffset;
	src.PlacedFootprint.Footprint.Format = s_ColorFormat;
	src.PlacedFootprint.Footprint.Width = nWidth;
	src.PlacedFootprint.Footprint.Height = nHeight;
	src.PlacedFootprint.Footprint.Depth = 1;
	src.PlacedFootprint.Footprint.RowPitch = nRowPitch;
	s_pCmd->CopyTextureRegion( &dst, x, y, 0, &src, NULL );
	pTex->m_bHasData = true;
}

static void KeepPending( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	if ( nLevel >= 16 )
		return;
	int nLevelW = MipSize( pTex->m_nWidth, nLevel ), nLevelH = MipSize( pTex->m_nHeight, nLevel );
	if ( !pTex->m_pPending[nLevel] )
		pTex->m_pPending[nLevel] = (unsigned char *)calloc( nLevelW * nLevelH, 4 );
	for ( int row = 0; row < nHeight && y + row < nLevelH; ++row )
	{
		int nCopy = nWidth;
		if ( x + nCopy > nLevelW ) nCopy = nLevelW - x;
		if ( nCopy > 0 )
			memcpy( pTex->m_pPending[nLevel] + ( ( y + row ) * nLevelW + x ) * 4, pBGRA + row * nWidth * 4, nCopy * 4 );
	}
}

//-----------------------------------------------------------------------------
// Device
//-----------------------------------------------------------------------------
static bool CreateBackBuffer( int nWidth, int nHeight )
{
	if ( s_BackBuffer.m_pResource )
		DestroyTexture( &s_BackBuffer, false );
	s_BackBuffer.m_nWidth = nWidth > 0 ? nWidth : 1;
	s_BackBuffer.m_nHeight = nHeight > 0 ? nHeight : 1;
	s_BackBuffer.m_nMips = 1;
	s_BackBuffer.m_bRenderTarget = true;
	s_BackBuffer.m_bUsed = true;
	return EnsureResource( &s_BackBuffer );
}

static void ReleaseSwapBuffers()
{
	for ( int i = 0; i < SWAP_BUFFERS; ++i )
		SAFE_RELEASE( s_pSwapBuffers[i] );
}

static bool GetSwapBuffers()
{
	for ( int i = 0; i < SWAP_BUFFERS; ++i )
		if ( FAILED( s_pSwapChain->GetBuffer( i, IID_PPV_ARGS( &s_pSwapBuffers[i] ) ) ) )
			return false;
	return true;
}

// the swap chain follows the window's client size (called with the GPU idle)
static bool CreateSwapchain()
{
	RECT rc; GetClientRect( s_hWnd, &rc );
	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;
	if ( nWidth <= 0 || nHeight <= 0 )
		return false;	// minimised; try again later
	ReleaseSwapBuffers();
	if ( s_pSwapChain )
	{
		if ( FAILED( s_pSwapChain->ResizeBuffers( SWAP_BUFFERS, nWidth, nHeight, s_ColorFormat, 0 ) ) )
			return false;
	}
	else
	{
		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width = nWidth;
		sd.Height = nHeight;
		sd.Format = s_ColorFormat;
		sd.SampleDesc.Count = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = SWAP_BUFFERS;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Scaling = DXGI_SCALING_STRETCH;
		IDXGISwapChain1 *pSwapChain1 = NULL;
		if ( FAILED( s_pFactory->CreateSwapChainForHwnd( s_pQueue, s_hWnd, &sd, NULL, NULL, &pSwapChain1 ) ) )
			return false;
		HRESULT hr = pSwapChain1->QueryInterface( IID_PPV_ARGS( &s_pSwapChain ) );
		pSwapChain1->Release();
		if ( FAILED( hr ) )
			return false;
		s_pFactory->MakeWindowAssociation( s_hWnd, DXGI_MWA_NO_ALT_ENTER );	// the game handles fullscreen
	}
	s_nSwapWidth = nWidth; s_nSwapHeight = nHeight;
	return GetSwapBuffers();
}

static bool CreateHeap( D3D12_DESCRIPTOR_HEAP_TYPE type, UINT nCount, ID3D12DescriptorHeap **ppHeap )
{
	D3D12_DESCRIPTOR_HEAP_DESC hd = {};
	hd.Type = type;
	hd.NumDescriptors = nCount;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	return SUCCEEDED( s_pDevice->CreateDescriptorHeap( &hd, IID_PPV_ARGS( ppHeap ) ) );
}

bool Dx12_Init( void *hWnd, int nWidth, int nHeight )
{
	if ( s_pDevice && s_hWnd == (HWND)hWnd )
		return true;
	Dx12_Shutdown();
	if ( !s_hD3D12 )
		s_hD3D12 = LoadLibraryA( "d3d12.dll" );
	if ( !s_hD3D12 )
		return false;
	PFN_D3D12_CREATE_DEVICE pfnCreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress( s_hD3D12, "D3D12CreateDevice" );
	s_pfnSerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress( s_hD3D12, "D3D12SerializeRootSignature" );
	if ( !pfnCreateDevice || !s_pfnSerializeRootSignature )
		return false;
	s_hWnd = (HWND)hWnd;

	// GAMMA_DX12_DEBUG=1: Direct3D's validation layer (needs the Graphics Tools feature)
	char szDebug[8];
	if ( GetEnvironmentVariableA( "GAMMA_DX12_DEBUG", szDebug, sizeof( szDebug ) ) )
	{
		PFN_D3D12_GET_DEBUG_INTERFACE pfnGetDebug = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress( s_hD3D12, "D3D12GetDebugInterface" );
		ID3D12Debug *pDebug = NULL;
		if ( pfnGetDebug && SUCCEEDED( pfnGetDebug( IID_PPV_ARGS( &pDebug ) ) ) )
		{
			pDebug->EnableDebugLayer();
			pDebug->Release();
		}
	}

	if ( FAILED( CreateDXGIFactory1( IID_PPV_ARGS( &s_pFactory ) ) ) )
		return false;
	// the hardware adapter with the most video memory that does Direct3D 12
	IDXGIAdapter1 *pBest = NULL;
	SIZE_T nBestMemory = 0;
	IDXGIAdapter1 *pAdapter = NULL;
	for ( UINT i = 0; s_pFactory->EnumAdapters1( i, &pAdapter ) != DXGI_ERROR_NOT_FOUND; ++i )
	{
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1( &desc );
		if ( !( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) && ( !pBest || desc.DedicatedVideoMemory > nBestMemory ) &&
			SUCCEEDED( pfnCreateDevice( pAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof( ID3D12Device ), NULL ) ) )
		{
			if ( pBest ) pBest->Release();
			pBest = pAdapter;
			nBestMemory = desc.DedicatedVideoMemory;
			WideCharToMultiByte( CP_UTF8, 0, desc.Description, -1, s_szAdapterName, sizeof( s_szAdapterName ), NULL, NULL );
			continue;
		}
		pAdapter->Release();
	}
	if ( !pBest )
		return false;
	HRESULT hr = pfnCreateDevice( pBest, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &s_pDevice ) );
	pBest->Release();
	if ( FAILED( hr ) )
	{
		s_pDevice = NULL;
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC qd = {};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if ( FAILED( s_pDevice->CreateCommandQueue( &qd, IID_PPV_ARGS( &s_pQueue ) ) ) )
		return false;
	for ( int i = 0; i < FRAMES; ++i )
	{
		if ( FAILED( s_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &s_pAllocators[i] ) ) ) )
			return false;
		s_SlotFence[i] = 0;
		if ( !CreateHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, VIEW_RING, &s_pViewHeaps[i] ) )
			return false;
	}
	if ( FAILED( s_pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, s_pAllocators[0], NULL, IID_PPV_ARGS( &s_pCmd ) ) ) )
		return false;
	s_pCmd->Close();
	s_nFenceValue = 0;
	if ( FAILED( s_pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &s_pFence ) ) ) )
		return false;
	s_hFenceEvent = CreateEventA( NULL, FALSE, FALSE, NULL );
	s_nViewIncrement = s_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

	s_SRVPool.m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	s_RTVPool.m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	s_DSVPool.m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	if ( !CreateHeap( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 4, &s_pSamplerHeap ) )
		return false;
	s_nSamplerIncrement = s_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
	for ( int i = 0; i < 4; ++i )
	{
		D3D12_SAMPLER_DESC sd = {};
		sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = ( i & 1 ) ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sd.AddressV = ( i & 2 ) ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sd.MaxLOD = D3D12_FLOAT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE h = s_pSamplerHeap->GetCPUDescriptorHandleForHeapStart();
		h.ptr += (SIZE_T)i * s_nSamplerIncrement;
		s_pDevice->CreateSampler( &sd, h );
	}

	CreateSwapchain();	// may fail while minimised; Present retries
	RECT rc; GetClientRect( s_hWnd, &rc );
	if ( nWidth <= 0 || nHeight <= 0 ) { nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top; }
	return CreateBackBuffer( nWidth, nHeight );
}

bool Dx12_Resize( int nWidth, int nHeight )
{
	if ( !s_pDevice )
		return false;
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
	}
	if ( nWidth <= 0 || nHeight <= 0 || ( nWidth == s_BackBuffer.m_nWidth && nHeight == s_BackBuffer.m_nHeight ) )
		return true;
	Submit();
	WaitIdle();
	return CreateBackBuffer( nWidth, nHeight );
}

void Dx12_Shutdown()
{
	Dx12R_Shutdown();
	if ( s_pDevice )
	{
		if ( s_bRecording )
		{
			s_pCmd->Close();
			s_bRecording = false;
		}
		WaitIdle();
		for ( int i = 0; i < FRAMES; ++i )
		{
			CollectGarbage( i );
			for ( size_t j = 0; j < s_Stream[i].size(); ++j )
				s_Stream[i][j].m_pBuffer->Release();
			s_Stream[i].clear();
			SAFE_RELEASE( s_pAllocators[i] );
			SAFE_RELEASE( s_pViewHeaps[i] );
		}
		if ( s_BackBuffer.m_pResource ) DestroyTexture( &s_BackBuffer, false );
		for ( size_t b = 0; b < s_Blocks.size(); ++b )
			s_Blocks[b].m_pBuffer->Release();
		s_Blocks.clear();
		DestroyPool( s_SRVPool );
		DestroyPool( s_RTVPool );
		DestroyPool( s_DSVPool );
	}
	ReleaseSwapBuffers();
	SAFE_RELEASE( s_pSwapChain );
	SAFE_RELEASE( s_pSamplerHeap );
	SAFE_RELEASE( s_pCmd );
	SAFE_RELEASE( s_pFence );
	SAFE_RELEASE( s_pQueue );
	SAFE_RELEASE( s_pDevice );
	SAFE_RELEASE( s_pFactory );
	if ( s_hFenceEvent ) CloseHandle( s_hFenceEvent );
	s_hFenceEvent = NULL;
	memset( &s_BackBuffer, 0, sizeof( s_BackBuffer ) );
	s_hWnd = NULL;
	s_nSwapWidth = s_nSwapHeight = 0;
}

bool Dx12_IsActive()
{
	return s_pDevice != NULL;
}

void Dx12_SetClearColor( float r, float g, float b, float a )
{
	s_ClearColor[0] = r; s_ClearColor[1] = g; s_ClearColor[2] = b; s_ClearColor[3] = a;
}

void Dx12_Present()
{
	if ( !s_pDevice )
		return;
	Dx12_Resize( 0, 0 );
	BeginRecording();

	// (re)size the swap chain when the window changed
	RECT rc; GetClientRect( s_hWnd, &rc );
	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;
	if ( nWidth > 0 && nHeight > 0 && ( !s_pSwapChain || nWidth != s_nSwapWidth || nHeight != s_nSwapHeight ) )
	{
		Submit();
		WaitIdle();
		CreateSwapchain();
		BeginRecording();
	}

	bool bPresent = s_pSwapChain && s_pSwapBuffers[0] && s_BackBuffer.m_pResource;
	if ( bPresent )
	{
		ID3D12Resource *pSwap = s_pSwapBuffers[s_pSwapChain->GetCurrentBackBufferIndex() % SWAP_BUFFERS];
		D3D12_RESOURCE_STATES swapState = D3D12_RESOURCE_STATE_PRESENT;
		Transition( pSwap, swapState, D3D12_RESOURCE_STATE_COPY_DEST );
		Transition( s_BackBuffer.m_pResource, s_BackBuffer.m_State, D3D12_RESOURCE_STATE_COPY_SOURCE );
		D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
		dst.pResource = pSwap;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.pResource = s_BackBuffer.m_pResource;
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		D3D12_BOX box = { 0, 0, 0, (UINT)min( s_BackBuffer.m_nWidth, s_nSwapWidth ), (UINT)min( s_BackBuffer.m_nHeight, s_nSwapHeight ), 1 };
		s_pCmd->CopyTextureRegion( &dst, 0, 0, 0, &src, &box );
		Transition( pSwap, swapState, D3D12_RESOURCE_STATE_PRESENT );
	}
	Submit();
	if ( bPresent )
		s_pSwapChain->Present( 1, 0 );
	s_nFrame = ( s_nFrame + 1 ) % FRAMES;
}

void Dx12_GetBackBufferSize( int &nWidth, int &nHeight )
{
	nWidth = s_BackBuffer.m_nWidth; nHeight = s_BackBuffer.m_nHeight;
}

void Dx12_GetWindowSize( int &nWidth, int &nHeight )
{
	RECT rc = {};
	if ( s_hWnd ) GetClientRect( s_hWnd, &rc );
	nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
}

void Dx12_GetDesktopSize( int &nWidth, int &nHeight )
{
	nWidth = GetSystemMetrics( SM_CXSCREEN ); nHeight = GetSystemMetrics( SM_CYSCREEN );
}

const char *Dx12_AdapterName()
{
	return s_szAdapterName;
}

//-----------------------------------------------------------------------------
// Renderer
//-----------------------------------------------------------------------------
static ID3D12QueryHeap *s_pQueryHeap;
static ID3D12Resource *s_pQueryReadback;
static UINT64 *s_pQueryResults;

static ID3DBlob *Compile( const char *pszEntry, const char *pszTarget )
{
	ID3DBlob *pCode = NULL, *pErrors = NULL;
	HRESULT hr = D3DCompile( s_szShader, sizeof( s_szShader ) - 1, "gammasource_dx12", NULL, NULL, pszEntry, pszTarget,
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCode, &pErrors );
	if ( pErrors )
	{
		OutputDebugStringA( (const char *)pErrors->GetBufferPointer() );
		pErrors->Release();
	}
	return SUCCEEDED( hr ) ? pCode : NULL;
}

bool Dx12R_Init()
{
	if ( s_pRootSignature )
		return true;
	if ( !s_pDevice )
		return false;
	s_pVS = Compile( "VSMain", "vs_5_0" );
	s_pPS = Compile( "PSMain", "ps_5_0" );
	if ( !s_pVS || !s_pPS )
		return false;

	// constants b0, bones b1, textures t0-t6 (t3 uses s0, t4-t6 s3); s0 follows the texture's wrap mode,
	// s1 s2 clamp, s3 wraps
	D3D12_DESCRIPTOR_RANGE srvRange = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DRAW_VIEWS, 0, 0, 0 };
	D3D12_DESCRIPTOR_RANGE samplerRange = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0 };
	D3D12_ROOT_PARAMETER params[4] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].Descriptor.ShaderRegister = 1;
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &srvRange;
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &samplerRange;
	for ( int i = 0; i < 4; ++i )
		params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	D3D12_STATIC_SAMPLER_DESC clamp[3] = {};
	for ( int i = 0; i < 3; ++i )
	{
		clamp[i].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		clamp[i].AddressU = clamp[i].AddressV = i < 2 ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		clamp[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		clamp[i].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		clamp[i].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		clamp[i].MaxLOD = D3D12_FLOAT32_MAX;
		clamp[i].ShaderRegister = i + 1;
		clamp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}
	D3D12_ROOT_SIGNATURE_DESC rsd = {};
	rsd.NumParameters = 4;
	rsd.pParameters = params;
	rsd.NumStaticSamplers = 3;
	rsd.pStaticSamplers = clamp;
	rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob *pSignature = NULL, *pErrors = NULL;
	if ( FAILED( s_pfnSerializeRootSignature( &rsd, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pErrors ) ) )
	{
		if ( pErrors )
		{
			OutputDebugStringA( (const char *)pErrors->GetBufferPointer() );
			pErrors->Release();
		}
		return false;
	}
	HRESULT hr = s_pDevice->CreateRootSignature( 0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS( &s_pRootSignature ) );
	pSignature->Release();
	if ( FAILED( hr ) )
	{
		s_pRootSignature = NULL;
		return false;
	}

	static const unsigned char s_White[4] = { 255, 255, 255, 255 };
	s_hWhite = Dx12R_CreateTexture( 1, 1, 1 );
	Dx12R_TexImage( s_hWhite, 0, 1, 1, s_White );
	s_hWhiteCube = Dx12R_CreateTexture( 1, 1, 1, false, true );
	for ( int nFace = 0; nFace < 6; ++nFace )
		Dx12R_TexImageFace( s_hWhiteCube, nFace, 0, 1, 1, s_White );

	memset( &s_State, 0, sizeof( s_State ) );
	s_State.m_bDepthTest = true;
	s_State.m_bDepthWrite = true;
	s_State.m_nDepthFunc = 3;	// near or equal
	for ( int i = 0; i < 16; ++i )
		s_WVP[i] = ( i % 5 ) ? 0.0f : 1.0f;

	D3D12_QUERY_HEAP_DESC qhd = {};
	qhd.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	qhd.Count = MAX_QUERIES;
	if ( FAILED( s_pDevice->CreateQueryHeap( &qhd, IID_PPV_ARGS( &s_pQueryHeap ) ) ) )
		s_pQueryHeap = NULL;
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = MAX_QUERIES * sizeof( UINT64 );
	rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if ( s_pQueryHeap && SUCCEEDED( s_pDevice->CreateCommittedResource( &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
		IID_PPV_ARGS( &s_pQueryReadback ) ) ) )
	{
		s_pQueryReadback->Map( 0, NULL, (void **)&s_pQueryResults );
	}
	else
	{
		s_pQueryReadback = NULL;
		SAFE_RELEASE( s_pQueryHeap );
	}
	return true;
}

void Dx12R_Shutdown()
{
	if ( !s_pDevice )
		return;
	if ( s_bRecording )
		Submit();
	WaitIdle();
	for ( size_t i = 0; i < s_Textures.size(); ++i )
		if ( s_Textures[i].m_bUsed )
			DestroyTexture( &s_Textures[i], false );
	s_Textures.clear();
	Dx12R_ReleaseCachedVertices( NULL );
	for ( int i = 0; i < FRAMES; ++i )
		CollectGarbage( i );
	for ( std::map< unsigned int, ID3D12PipelineState * >::iterator it = s_Pipelines.begin(); it != s_Pipelines.end(); ++it )
		SAFE_RELEASE( it->second );
	s_Pipelines.clear();
	if ( s_pQueryReadback ) s_pQueryReadback->Unmap( 0, NULL );
	s_pQueryResults = NULL;
	SAFE_RELEASE( s_pQueryReadback );
	SAFE_RELEASE( s_pQueryHeap );
	SAFE_RELEASE( s_pRootSignature );
	SAFE_RELEASE( s_pVS );
	SAFE_RELEASE( s_pPS );
	s_pBoundPipeline = NULL;
	s_hWhite = s_hWhiteCube = s_hBound = s_hLightmap = s_hRenderTarget = 0;
}

int Dx12R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget, bool bCube )
{
	Texture_t tex = {};
	tex.m_nWidth = nWidth > 0 ? nWidth : 1;
	tex.m_nHeight = nHeight > 0 ? nHeight : 1;
	tex.m_nMips = ( nMips > 0 && !bRenderTarget ) ? nMips : 1;
	tex.m_bRenderTarget = bRenderTarget;
	tex.m_bCube = bCube && !bRenderTarget;
	tex.m_bUsed = true;
	for ( size_t i = 0; i < s_Textures.size(); ++i )
	{
		if ( !s_Textures[i].m_bUsed )
		{
			s_Textures[i] = tex;
			return (int)i + 1;
		}
	}
	s_Textures.push_back( tex );
	return (int)s_Textures.size();
}

void Dx12R_DeleteTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( hTexture == s_hRenderTarget )
		Dx12R_SetRenderTarget( 0 );
	DestroyTexture( pTex, true );
}

bool Dx12R_IsTexture( int hTexture )
{
	return GetTexture( hTexture ) != NULL;
}

void Dx12R_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( nLevel == 0 && ( nWidth != pTex->m_nWidth || nHeight != pTex->m_nHeight ) && !pTex->m_pResource )
	{
		pTex->m_nWidth = nWidth;
		pTex->m_nHeight = nHeight;
	}
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA, nWidth * 4 );
}

void Dx12R_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, x, y, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, x, y, nWidth, nHeight, pBGRA, nWidth * 4 );
}

// one face of a cube texture (cubes only get data once the device exists)
void Dx12R_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || !pTex->m_bCube || nFace < 0 || nFace >= 6 || nLevel >= pTex->m_nMips || !EnsureResource( pTex ) )
		return;
	UploadRegion( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA, nWidth * 4, nFace );
}

bool Dx12R_IsCubeTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bCube;
}

bool Dx12R_IsRenderTargetTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bRenderTarget;
}

unsigned char *Dx12R_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return NULL;
	if ( !pTex->m_pShadow )
		pTex->m_pShadow = (unsigned char *)calloc( pTex->m_nWidth * pTex->m_nHeight, 4 );
	nPitch = pTex->m_nWidth * 4;
	nWidth = pTex->m_nWidth;
	nHeight = pTex->m_nHeight;
	return pTex->m_pShadow;
}

void Dx12R_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nWidth <= 0 || nHeight <= 0 )
		return;
	if ( !EnsureResource( pTex ) )
	{
		for ( int row = 0; row < nHeight; ++row )
			KeepPending( pTex, 0, x, y + row, nWidth, 1, pBGRA + row * nPitch );
		return;
	}
	UploadRegion( pTex, 0, x, y, nWidth, nHeight, pBGRA, nPitch );
}

void Dx12R_TexWrap( int hTexture, int nCoord, bool bClamp )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( nCoord == 0 )
		pTex->m_bClampS = bClamp;
	else if ( nCoord == 1 )
		pTex->m_bClampT = bClamp;
}

void Dx12R_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ )
{
	s_Viewport.TopLeftX = (float)x; s_Viewport.TopLeftY = (float)y;
	s_Viewport.Width = (float)w; s_Viewport.Height = (float)h;
	s_Viewport.MinDepth = flMinZ; s_Viewport.MaxDepth = flMaxZ;
	s_bViewportSet = w > 0 && h > 0;
}

void Dx12R_GetTextureSize( int hTexture, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	nWidth = pTex ? pTex->m_nWidth : 0;
	nHeight = pTex ? pTex->m_nHeight : 0;
}

void Dx12R_SetRenderTarget( int hColor )
{
	Texture_t *pTex = GetTexture( hColor );
	if ( !pTex || !pTex->m_bRenderTarget || !EnsureResource( pTex ) )
		hColor = 0;
	s_hRenderTarget = hColor;
}

void Dx12R_Clear( bool bColor, bool bDepth )
{
	if ( !( bColor || bDepth ) || !s_pDevice )
		return;
	BeginRecording();
	Texture_t *pTarget = BindTarget();
	if ( !pTarget )
		return;
	if ( bColor )
		s_pCmd->ClearRenderTargetView( DescriptorHandle( s_RTVPool, pTarget->m_nRTV ), s_ClearColor, 0, NULL );
	if ( bDepth )
		s_pCmd->ClearDepthStencilView( DescriptorHandle( s_DSVPool, pTarget->m_nDSV ), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f, 0, 0, NULL );
}

void Dx12R_ClearDepth()
{
	Dx12R_Clear( false, true );
}

void Dx12R_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy )
{
	Texture_t *pDst = GetTexture( hTexture );
	if ( !pDst || !s_pDevice || !EnsureResource( pDst ) || hTexture == s_hRenderTarget )
		return;
	Texture_t *pSrc = CurrentTarget();
	if ( !pSrc->m_pResource )
		return;
	// clip the copy to both surfaces (no scaling here)
	if ( sx < 0 ) { dx -= sx; sw += sx; sx = 0; }
	if ( sy < 0 ) { dy -= sy; sh += sy; sy = 0; }
	if ( dx < 0 ) { sx -= dx; sw += dx; dx = 0; }
	if ( dy < 0 ) { sy -= dy; sh += dy; dy = 0; }
	if ( sx + sw > pSrc->m_nWidth ) sw = pSrc->m_nWidth - sx;
	if ( sy + sh > pSrc->m_nHeight ) sh = pSrc->m_nHeight - sy;
	if ( dx + sw > pDst->m_nWidth ) sw = pDst->m_nWidth - dx;
	if ( dy + sh > pDst->m_nHeight ) sh = pDst->m_nHeight - dy;
	if ( sw <= 0 || sh <= 0 )
		return;
	BeginRecording();
	Transition( pSrc->m_pResource, pSrc->m_State, D3D12_RESOURCE_STATE_COPY_SOURCE );
	Transition( pDst->m_pResource, pDst->m_State, D3D12_RESOURCE_STATE_COPY_DEST );
	D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
	dst.pResource = pDst->m_pResource;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.pResource = pSrc->m_pResource;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	D3D12_BOX box = { (UINT)sx, (UINT)sy, 0, (UINT)( sx + sw ), (UINT)( sy + sh ), 1 };
	s_pCmd->CopyTextureRegion( &dst, dx, dy, 0, &src, &box );
	pDst->m_bHasData = true;
}

//-----------------------------------------------------------------------------
// Occlusion queries: one heap, a slot per query object (handle = slot + 1). Each
// result is resolved into a readback buffer, readable once its frame is done.
//-----------------------------------------------------------------------------
static std::vector< bool > s_QueryUsed;
static std::vector< UINT64 > s_QuerySerial;	// fence value of the submission that resolves it; 0 never issued

int Dx12R_CreateQuery()
{
	if ( !s_pQueryHeap )
		return 0;
	for ( size_t i = 0; i < MAX_QUERIES; ++i )
	{
		if ( i >= s_QueryUsed.size() ) { s_QueryUsed.push_back( false ); s_QuerySerial.push_back( 0 ); }
		if ( !s_QueryUsed[i] )
		{
			s_QueryUsed[i] = true;
			s_QuerySerial[i] = 0;
			return (int)i + 1;
		}
	}
	return 0;
}

void Dx12R_DestroyQuery( int hQuery )
{
	if ( hQuery > 0 && hQuery <= (int)s_QueryUsed.size() )
		s_QueryUsed[hQuery - 1] = false;
}

void Dx12R_BeginQuery( int hQuery )
{
	if ( hQuery <= 0 || hQuery > (int)s_QueryUsed.size() || !s_pQueryHeap )
		return;
	BeginRecording();
	if ( s_nOpenQuery )
		Dx12R_EndQuery( s_nOpenQuery );
	s_pCmd->BeginQuery( s_pQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, hQuery - 1 );
	s_nOpenQuery = hQuery;
}

void Dx12R_EndQuery( int hQuery )
{
	if ( hQuery != s_nOpenQuery || !s_bRecording )
		return;
	s_pCmd->EndQuery( s_pQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, hQuery - 1 );
	s_pCmd->ResolveQueryData( s_pQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, hQuery - 1, 1, s_pQueryReadback, ( hQuery - 1 ) * sizeof( UINT64 ) );
	s_QuerySerial[hQuery - 1] = s_nFenceValue + 1;	// signalled when this command list is submitted
	s_nOpenQuery = 0;
}

int Dx12R_GetQueryResult( int hQuery, bool bFlush )
{
	if ( hQuery <= 0 || hQuery > (int)s_QueryUsed.size() || !s_QuerySerial[hQuery - 1] || !s_pQueryResults )
		return -2;	// OCCLUSION_QUERY_RESULT_ERROR
	if ( s_pFence->GetCompletedValue() < s_QuerySerial[hQuery - 1] )
		return -1;	// OCCLUSION_QUERY_RESULT_PENDING
	UINT64 nPixels = s_pQueryResults[hQuery - 1];
	return nPixels > 0x7fffffff ? 0x7fffffff : (int)nPixels;
}

//-----------------------------------------------------------------------------
// Static mesh vertex cache
//-----------------------------------------------------------------------------
struct CachedVertices_t
{
	const void *m_pKey;
	int m_nVersion;
	Allocation_t m_Memory;
};
static std::vector< CachedVertices_t > s_VertexCache;

static CachedVertices_t *FindCached( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
		if ( s_VertexCache[i].m_pKey == pCacheKey )
			return &s_VertexCache[i];
	return NULL;
}

bool Dx12R_HasCachedVertices( const void *pCacheKey, int nVersion )
{
	CachedVertices_t *pCached = pCacheKey ? FindCached( pCacheKey ) : NULL;
	return pCached && pCached->m_nVersion == nVersion && pCached->m_Memory.m_pBuffer;
}

static void RetireCached( CachedVertices_t &c )
{
	if ( !c.m_Memory.m_pBuffer )
		return;
	Garbage_t g = {};
	g.m_Memory = c.m_Memory;
	if ( s_pDevice ) s_Garbage[s_nFrame].push_back( g );
	memset( &c.m_Memory, 0, sizeof( c.m_Memory ) );
}

// NULL releases everything
void Dx12R_ReleaseCachedVertices( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); )
	{
		if ( pCacheKey && s_VertexCache[i].m_pKey != pCacheKey )
		{
			++i;
			continue;
		}
		RetireCached( s_VertexCache[i] );
		s_VertexCache.erase( s_VertexCache.begin() + i );
		if ( pCacheKey )
			return;
	}
}

static CachedVertices_t *CacheVertices( const void *pCacheKey, int nVersion, const Dx12Vertex_t *pVerts, int nVerts )
{
	CachedVertices_t *pCached = FindCached( pCacheKey );
	if ( pCached && pCached->m_nVersion == nVersion && pCached->m_Memory.m_pBuffer )
		return pCached;
	if ( !pVerts )
		return NULL;
	if ( !pCached )
	{
		CachedVertices_t entry = {};
		entry.m_pKey = pCacheKey;
		s_VertexCache.push_back( entry );
		pCached = &s_VertexCache.back();
	}
	RetireCached( *pCached );	// the GPU may still be reading the old one
	UINT64 nBytes = (UINT64)nVerts * sizeof( Dx12Vertex_t );
	if ( !Allocate( nBytes, pCached->m_Memory ) )
		return NULL;
	memcpy( pCached->m_Memory.m_pMapped, pVerts, (size_t)nBytes );
	pCached->m_Memory.m_nSize = nBytes;
	pCached->m_nVersion = nVersion;
	return pCached;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------
static UINT64 s_nBoneSerial;	// the bones in streaming memory belong to this submission
static D3D12_GPU_VIRTUAL_ADDRESS s_BoneAddress;

static Texture_t *SampleableTexture( int hTexture, const Texture_t *pTarget )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( pTex && !pTex->m_pResource )
		EnsureResource( pTex );
	if ( !pTex || !pTex->m_nSRV || !pTex->m_bHasData || pTex->m_pResource == pTarget->m_pResource )
		return NULL;
	return pTex;
}

void Dx12R_Draw( Dx12Primitive_t type, const Dx12Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey, int nVersion )
{
	if ( !s_pRootSignature || nVerts <= 0 )
		return;
	BeginRecording();
	if ( StreamUsed() > 256 * 1024 * 1024 || s_nViewUsed[s_nFrame] + DRAW_VIEWS > VIEW_RING )
		SubmitAndWait();
	Texture_t *pTarget = CurrentTarget();
	if ( !pTarget->m_pResource )
		return;

	// textures first (creating one may upload, and so submit); a texture without data
	// (or the current target) samples white
	Texture_t *pTex = SampleableTexture( s_hBound, pTarget );
	if ( !pTex ) pTex = SampleableTexture( s_hWhite, pTarget );
	Texture_t *pLightmap = SampleableTexture( s_hLightmap, pTarget );
	Texture_t *pSecond = SampleableTexture( s_bWaterPass ? s_hWaterReflection : s_hLightmap, pTarget );
	if ( !pSecond ) pSecond = SampleableTexture( s_hWhite, pTarget );
	Texture_t *pEnvmap = NULL;
	if ( s_bEnvmap )
		pEnvmap = SampleableTexture( Dx12R_IsCubeTexture( s_hStage1 ) ? s_hStage1 : s_hStage2, pTarget );
	if ( !pEnvmap || !pEnvmap->m_bCube ) pEnvmap = SampleableTexture( s_hWhiteCube, pTarget );
	if ( !pTex || !pSecond || !pEnvmap )
		return;
	// t3: the envmap mask, else white
	Texture_t *pMask = SampleableTexture( s_hEnvMask, pTarget );
	if ( !pMask || pMask->m_bCube ) pMask = SampleableTexture( s_hWhite, pTarget );
	if ( !pMask )
		return;
	// t4 - t6: the surface's normal map, detail texture and blend texture, else white
	int hExtra[3] = { s_PassSurface.m_nBump ? s_PassSurface.m_hBump : 0, s_PassSurface.m_nDetail ? s_PassSurface.m_hDetail : 0, s_PassSurface.m_hBase2 };
	Texture_t *pExtra[3];
	for ( int i = 0; i < 3; ++i )
	{
		pExtra[i] = SampleableTexture( hExtra[i], pTarget );
		if ( !pExtra[i] || pExtra[i]->m_bCube ) pExtra[i] = SampleableTexture( s_hWhite, pTarget );
		if ( !pExtra[i] )
			return;
	}

	// vertex data: cached for static meshes, streamed otherwise
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.StrideInBytes = sizeof( Dx12Vertex_t );
	if ( pCacheKey )
	{
		CachedVertices_t *pCached = CacheVertices( pCacheKey, nVersion, pVerts, nVerts );
		if ( pCached )
		{
			vbv.BufferLocation = pCached->m_Memory.m_pBuffer->GetGPUVirtualAddress() + pCached->m_Memory.m_nOffset;
			vbv.SizeInBytes = (UINT)pCached->m_Memory.m_nSize;
		}
	}
	else if ( pVerts )
	{
		unsigned char *p = StreamAlloc( (UINT64)nVerts * sizeof( Dx12Vertex_t ), vbv.BufferLocation );
		if ( p )
		{
			memcpy( p, pVerts, nVerts * sizeof( Dx12Vertex_t ) );
			vbv.SizeInBytes = nVerts * sizeof( Dx12Vertex_t );
		}
	}
	if ( !vbv.BufferLocation )
		return;
	D3D12_INDEX_BUFFER_VIEW ibv = {};
	if ( pIndices && nIndices > 0 )
	{
		unsigned char *p = StreamAlloc( (UINT64)nIndices * sizeof( unsigned short ), ibv.BufferLocation );
		if ( !p )
			return;
		memcpy( p, pIndices, nIndices * sizeof( unsigned short ) );
		ibv.SizeInBytes = nIndices * sizeof( unsigned short );
		ibv.Format = DXGI_FORMAT_R16_UINT;
	}

	Texture_t *pTextures[DRAW_VIEWS] = { pTex, pSecond, pEnvmap, pMask, pExtra[0], pExtra[1], pExtra[2] };
	for ( int i = 0; i < DRAW_VIEWS; ++i )
		Transition( pTextures[i]->m_pResource, pTextures[i]->m_State, s_SampleState );
	if ( !BindTarget() )
		return;

	// per-draw constants and the bone matrices, from this frame's streaming memory
	D3D12_GPU_VIRTUAL_ADDRESS constants;
	Constants_t *pConst = (Constants_t *)StreamAlloc( sizeof( Constants_t ), constants );
	if ( !pConst )
		return;
	memset( pConst, 0, sizeof( Constants_t ) );
	FillConstants( pConst, pLightmap != NULL, pTarget->m_nWidth, pTarget->m_nHeight );
	if ( s_bBonesDirty || s_nBoneSerial != s_nFenceValue + 1 || !s_BoneAddress )
	{
		unsigned char *pBones = StreamAlloc( sizeof( s_Bones ), s_BoneAddress );
		if ( !pBones )
			return;
		memcpy( pBones, s_Bones, sizeof( s_Bones ) );
		s_bBonesDirty = false;
		s_nBoneSerial = s_nFenceValue + 1;
	}

	// the textures' views go into this frame's shader-visible ring
	UINT nView = s_nViewUsed[s_nFrame];
	s_nViewUsed[s_nFrame] += DRAW_VIEWS;
	D3D12_CPU_DESCRIPTOR_HANDLE viewCPU = s_pViewHeaps[s_nFrame]->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE viewGPU = s_pViewHeaps[s_nFrame]->GetGPUDescriptorHandleForHeapStart();
	viewCPU.ptr += (SIZE_T)nView * s_nViewIncrement;
	viewGPU.ptr += (UINT64)nView * s_nViewIncrement;
	for ( int i = 0; i < DRAW_VIEWS; ++i )
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dst = viewCPU;
		dst.ptr += (SIZE_T)i * s_nViewIncrement;
		s_pDevice->CopyDescriptorsSimple( 1, dst, DescriptorHandle( s_SRVPool, pTextures[i]->m_nSRV ), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
	}
	D3D12_GPU_DESCRIPTOR_HANDLE sampler = s_pSamplerHeap->GetGPUDescriptorHandleForHeapStart();
	sampler.ptr += (UINT64)( ( pTex->m_bClampS ? 1 : 0 ) | ( pTex->m_bClampT ? 2 : 0 ) ) * s_nSamplerIncrement;

	ID3D12PipelineState *pPipeline = GetPipeline( type );
	if ( !pPipeline )
		return;
	if ( !s_bRootBound )
	{
		s_pCmd->SetGraphicsRootSignature( s_pRootSignature );
		s_bRootBound = true;
	}
	if ( pPipeline != s_pBoundPipeline )
	{
		s_pCmd->SetPipelineState( pPipeline );
		s_pBoundPipeline = pPipeline;
	}
	s_pCmd->SetGraphicsRootConstantBufferView( 0, constants );
	s_pCmd->SetGraphicsRootConstantBufferView( 1, s_BoneAddress );
	s_pCmd->SetGraphicsRootDescriptorTable( 2, viewGPU );
	s_pCmd->SetGraphicsRootDescriptorTable( 3, sampler );

	D3D12_VIEWPORT vp = s_Viewport;
	if ( !s_bViewportSet )
	{
		vp.TopLeftX = vp.TopLeftY = 0.0f;
		vp.Width = (float)pTarget->m_nWidth; vp.Height = (float)pTarget->m_nHeight;
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
	}
	s_pCmd->RSSetViewports( 1, &vp );
	D3D12_RECT scissor = { 0, 0, pTarget->m_nWidth, pTarget->m_nHeight };
	s_pCmd->RSSetScissorRects( 1, &scissor );

	++g_nDx12Draws;
	s_pCmd->IASetPrimitiveTopology( s_Topology[type] );
	s_pCmd->IASetVertexBuffers( 0, 1, &vbv );
	if ( ibv.BufferLocation )
	{
		s_pCmd->IASetIndexBuffer( &ibv );
		s_pCmd->DrawIndexedInstanced( nIndices, 1, 0, 0, 0 );
	}
	else
	{
		s_pCmd->DrawInstanced( nVerts, 1, 0, 0 );
	}
}
