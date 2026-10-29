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
#include "dx11device.h"

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "user32.lib" )

static ID3D11Device *s_pDevice;
static ID3D11DeviceContext *s_pContext;
static IDXGISwapChain1 *s_pSwapChain;
static ID3D11RenderTargetView *s_pBackBufferRTV;
static HWND s_hWnd;
static int s_nWidth, s_nHeight;
static float s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static char s_szAdapterName[256] = "Direct3D 11";

static void ReleaseBackBuffer()
{
	if ( s_pBackBufferRTV ) { s_pBackBufferRTV->Release(); s_pBackBufferRTV = NULL; }
}

static bool CreateBackBuffer()
{
	ID3D11Texture2D *pBackBuffer = NULL;
	if ( FAILED( s_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void **)&pBackBuffer ) ) )
		return false;
	HRESULT hr = s_pDevice->CreateRenderTargetView( pBackBuffer, NULL, &s_pBackBufferRTV );
	pBackBuffer->Release();
	return SUCCEEDED( hr );
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
	return CreateBackBuffer();
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

void Dx11_Present()
{
	if ( !s_pSwapChain )
		return;
	// follow window size changes (windowed / borderless)
	Dx11_Resize( 0, 0 );
	s_pSwapChain->Present( 1, 0 );
	s_pContext->OMSetRenderTargets( 1, &s_pBackBufferRTV, NULL );
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
