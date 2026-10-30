//========= GammaSource ============//
//
// Native Direct3D 11 device for shaderapidx11: device, swap chain on the game window,
// back buffer render target, clears and present.
//
// Stage 2 of the native renderers: the device runs and clears/presents every frame.
// Geometry, textures and shaders come in the following stages.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <stdlib.h>
#include "dx11device.h"

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "user32.lib" )

static ID3D11Device *s_pDevice;
static ID3D11DeviceContext *s_pContext;
static IDXGISwapChain1 *s_pSwapChain;
static ID3D11RenderTargetView *s_pBackBufferRTV;
static ID3D11Texture2D *s_pDepthTexture;
static ID3D11DepthStencilView *s_pDepthDSV;
static HWND s_hWnd;
static int s_nWidth, s_nHeight;
static float s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static char s_szAdapterName[256] = "Direct3D 11";

static void ReleaseBackBuffer()
{
	if ( s_pBackBufferRTV ) { s_pBackBufferRTV->Release(); s_pBackBufferRTV = NULL; }
	if ( s_pDepthDSV ) { s_pDepthDSV->Release(); s_pDepthDSV = NULL; }
	if ( s_pDepthTexture ) { s_pDepthTexture->Release(); s_pDepthTexture = NULL; }
}

static bool CreateBackBuffer()
{
	ID3D11Texture2D *pBackBuffer = NULL;
	if ( FAILED( s_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&pBackBuffer ) ) )
		return false;
	HRESULT hr = s_pDevice->CreateRenderTargetView( pBackBuffer, NULL, &s_pBackBufferRTV );
	D3D11_TEXTURE2D_DESC bbDesc;
	pBackBuffer->GetDesc( &bbDesc );
	pBackBuffer->Release();
	if ( FAILED( hr ) )
		return false;

	D3D11_TEXTURE2D_DESC dd = {};
	dd.Width = bbDesc.Width;
	dd.Height = bbDesc.Height;
	dd.MipLevels = 1;
	dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dd.SampleDesc.Count = 1;
	dd.Usage = D3D11_USAGE_DEFAULT;
	dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if ( FAILED( s_pDevice->CreateTexture2D( &dd, NULL, &s_pDepthTexture ) ) ||
		 FAILED( s_pDevice->CreateDepthStencilView( s_pDepthTexture, NULL, &s_pDepthDSV ) ) )
		return false;
	s_pContext->OMSetRenderTargets( 1, &s_pBackBufferRTV, s_pDepthDSV );
	return true;
}

static void DebugLog( const char *pszMsg, int n )
{
	static char s_szFile[MAX_PATH];
	static const char *s_pszFile = GetEnvironmentVariableA( "GAMMA_DX11_LOG", s_szFile, MAX_PATH ) ? s_szFile : NULL;
	if ( !s_pszFile )
		return;
	FILE *f = fopen( s_pszFile, "a" );
	if ( f ) { fprintf( f, "%s %d\n", pszMsg, n ); fclose( f ); }
}

void Dx11_DebugLog( const char *pszMsg, int n ) { DebugLog( pszMsg, n ); }

void *Dx11_Device() { return s_pDevice; }
void *Dx11_BackBufferRTV() { return s_pBackBufferRTV; }
void *Dx11_BackBufferDSV() { return s_pDepthDSV; }
const float *Dx11_GetClearColor() { return s_ClearColor; }
void *Dx11_AcquireBackBuffer()
{
	ID3D11Texture2D *pBack = NULL;
	if ( s_pSwapChain )
		s_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&pBack );
	return pBack;
}
void *Dx11_Context() { return s_pContext; }
void Dx11_BindBackBuffer()
{
	if ( s_pContext && s_pBackBufferRTV )
		s_pContext->OMSetRenderTargets( 1, &s_pBackBufferRTV, s_pDepthDSV );
}
void Dx11_ClearDepth()
{
	if ( s_pContext && s_pDepthDSV )
		s_pContext->ClearDepthStencilView( s_pDepthDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

bool Dx11_Init( void *hWnd, int nWidth, int nHeight )
{
	if ( s_pDevice && s_hWnd == (HWND)hWnd )
		return Dx11_Resize( nWidth, nHeight );
	Dx11_Shutdown();

	s_hWnd = (HWND)hWnd;
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
	}
	s_nWidth = nWidth > 0 ? nWidth : 1; s_nHeight = nHeight > 0 ? nHeight : 1;

	static const D3D_FEATURE_LEVEL s_Levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL level;
	UINT nFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	HRESULT hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, nFlags, s_Levels, ARRAYSIZE( s_Levels ),
		D3D11_SDK_VERSION, &s_pDevice, &level, &s_pContext );
	if ( FAILED( hr ) )
		hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, nFlags, s_Levels + 1, 1,
			D3D11_SDK_VERSION, &s_pDevice, &level, &s_pContext );
	if ( FAILED( hr ) )
		return false;

	IDXGIDevice *pDXGIDevice = NULL;
	IDXGIAdapter *pAdapter = NULL;
	IDXGIFactory2 *pFactory = NULL;
	s_pDevice->QueryInterface( __uuidof( IDXGIDevice ), (void **)&pDXGIDevice );
	if ( pDXGIDevice ) pDXGIDevice->GetAdapter( &pAdapter );
	if ( pAdapter )
	{
		DXGI_ADAPTER_DESC desc;
		if ( SUCCEEDED( pAdapter->GetDesc( &desc ) ) )
			WideCharToMultiByte( CP_UTF8, 0, desc.Description, -1, s_szAdapterName, sizeof( s_szAdapterName ), NULL, NULL );
		pAdapter->GetParent( __uuidof( IDXGIFactory2 ), (void **)&pFactory );
	}

	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.Width = s_nWidth;
	sd.Height = s_nHeight;
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	if ( pFactory )
	{
		hr = pFactory->CreateSwapChainForHwnd( s_pDevice, s_hWnd, &sd, NULL, NULL, &s_pSwapChain );
		// the engine handles alt+enter itself
		pFactory->MakeWindowAssociation( s_hWnd, DXGI_MWA_NO_ALT_ENTER );
	}
	if ( pFactory ) pFactory->Release();
	if ( pAdapter ) pAdapter->Release();
	if ( pDXGIDevice ) pDXGIDevice->Release();
	if ( !s_pSwapChain || FAILED( hr ) )
	{
		Dx11_Shutdown();
		return false;
	}
	bool bOk = CreateBackBuffer();
	DebugLog( "init", bOk ? s_nWidth * 10000 + s_nHeight : -1 );
	return bOk;
}

bool Dx11_Resize( int nWidth, int nHeight )
{
	if ( !s_pSwapChain )
		return false;
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
	}
	if ( nWidth <= 0 || nHeight <= 0 || ( nWidth == s_nWidth && nHeight == s_nHeight ) )
		return true;
	s_pContext->OMSetRenderTargets( 0, NULL, NULL );
	ReleaseBackBuffer();
	if ( FAILED( s_pSwapChain->ResizeBuffers( 0, nWidth, nHeight, DXGI_FORMAT_UNKNOWN, 0 ) ) )
		return false;
	s_nWidth = nWidth; s_nHeight = nHeight;
	return CreateBackBuffer();
}

void Dx11_Shutdown()
{
	ReleaseBackBuffer();
	if ( s_pSwapChain ) { s_pSwapChain->Release(); s_pSwapChain = NULL; }
	if ( s_pContext ) { s_pContext->ClearState(); s_pContext->Release(); s_pContext = NULL; }
	if ( s_pDevice ) { s_pDevice->Release(); s_pDevice = NULL; }
	s_hWnd = NULL;
}

bool Dx11_IsActive()
{
	return s_pDevice != NULL;
}

void Dx11_SetClearColor( float r, float g, float b, float a )
{
	s_ClearColor[0] = r; s_ClearColor[1] = g; s_ClearColor[2] = b; s_ClearColor[3] = a;
}

void Dx11_Clear( bool bColor )
{
	if ( !s_pContext || !s_pBackBufferRTV )
		return;
	if ( bColor )
		s_pContext->ClearRenderTargetView( s_pBackBufferRTV, s_ClearColor );
}

// debug: write a BGRA texture to a .bmp
void Dx11_DumpTexture( void *pTexture, const char *pszFile )
{
	ID3D11Texture2D *pBack = (ID3D11Texture2D *)pTexture, *pStage = NULL;
	D3D11_TEXTURE2D_DESC td; pBack->GetDesc( &td );
	td.MipLevels = 1;
	td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
	if ( SUCCEEDED( s_pDevice->CreateTexture2D( &td, NULL, &pStage ) ) )
	{
		s_pContext->CopySubresourceRegion( pStage, 0, 0, 0, 0, pBack, 0, NULL );
		D3D11_MAPPED_SUBRESOURCE m;
		if ( SUCCEEDED( s_pContext->Map( pStage, 0, D3D11_MAP_READ, 0, &m ) ) )
		{
			FILE *f = fopen( pszFile, "wb" );
			if ( f )
			{
				int w = td.Width, h = td.Height, nSize = w * h * 4;
				BITMAPFILEHEADER bf = { 0x4D42, (DWORD)( 54 + nSize ), 0, 0, 54 };
				BITMAPINFOHEADER bi = { sizeof( bi ), w, -h, 1, 32, BI_RGB, (DWORD)nSize };
				fwrite( &bf, sizeof( bf ), 1, f ); fwrite( &bi, sizeof( bi ), 1, f );
				for ( int y = 0; y < h; ++y )
					fwrite( (char *)m.pData + y * m.RowPitch, w * 4, 1, f );
				fclose( f );
			}
			s_pContext->Unmap( pStage, 0 );
		}
		pStage->Release();
	}
}

// debug: GAMMA_DX11_DUMP=<file.bmp> writes the back buffer every 120 frames
static void DumpBackBuffer()
{
	static int s_nFrame;
	static char s_szFile[MAX_PATH];
	static const char *s_pszFile = GetEnvironmentVariableA( "GAMMA_DX11_DUMP", s_szFile, MAX_PATH ) ? s_szFile : NULL;
	extern int g_nDx11Draws, g_nDx11Meshes, g_nDx11Passes, g_nDx11EmptyMeshes, g_nDx11Binds, g_nDx11Textured, g_nDx11Uploads;
	if ( s_nFrame < 5 || s_nFrame % 300 == 0 ) { DebugLog( "present", s_nFrame ); DebugLog( " meshes", g_nDx11Meshes ); DebugLog( " draws", g_nDx11Draws ); DebugLog( " passes", g_nDx11Passes ); DebugLog( " empty", g_nDx11EmptyMeshes ); DebugLog( " binds", g_nDx11Binds ); DebugLog( " textured", g_nDx11Textured ); DebugLog( " uploads total", g_nDx11Uploads ); }
	g_nDx11Draws = g_nDx11Meshes = g_nDx11Passes = g_nDx11EmptyMeshes = g_nDx11Binds = g_nDx11Textured = 0;
	if ( !s_pszFile || ( ++s_nFrame % 120 ) != 0 )
		return;
	ID3D11Texture2D *pBack = NULL;
	if ( FAILED( s_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&pBack ) ) )
		return;
	Dx11_DumpTexture( pBack, s_pszFile );
	pBack->Release();
}

void Dx11_Present()
{
	if ( !s_pSwapChain )
		return;
	// follow window size changes (windowed / borderless)
	Dx11_Resize( 0, 0 );
	DumpBackBuffer();
	s_pSwapChain->Present( 1, 0 );
	Dx11_BindBackBuffer();
}

void Dx11_GetBackBufferSize( int &nWidth, int &nHeight )
{
	nWidth = s_nWidth; nHeight = s_nHeight;
}

void Dx11_GetWindowSize( int &nWidth, int &nHeight )
{
	RECT rc = {};
	if ( s_hWnd ) GetClientRect( s_hWnd, &rc );
	nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
}

const char *Dx11_AdapterName()
{
	return s_szAdapterName;
}

void Dx11_GetDesktopSize( int &nWidth, int &nHeight )
{
	nWidth = GetSystemMetrics( SM_CXSCREEN ); nHeight = GetSystemMetrics( SM_CYSCREEN );
}
