//========= GammaSource ============//
//
// Native Direct3D 11 device used by shaderapidx11 (kept free of Source headers).
//
//=============================================================================//
#ifndef DX11DEVICE_H
#define DX11DEVICE_H
#pragma once

bool Dx11_Init( void *hWnd, int nWidth, int nHeight );
bool Dx11_Resize( int nWidth, int nHeight );
void Dx11_Shutdown();
bool Dx11_IsActive();
void Dx11_SetClearColor( float r, float g, float b, float a );
void Dx11_Clear( bool bColor );
void Dx11_Present();
void Dx11_GetBackBufferSize( int &nWidth, int &nHeight );
void Dx11_GetWindowSize( int &nWidth, int &nHeight );
const char *Dx11_AdapterName();
void Dx11_GetDesktopSize( int &nWidth, int &nHeight );
void *Dx11_Device();		// ID3D11Device *
void *Dx11_Context();	// ID3D11DeviceContext *
void Dx11_BindBackBuffer();
void Dx11_ClearDepth();
void Dx11_DebugLog( const char *pszMsg, int n );
void *Dx11_BackBufferRTV();		// ID3D11RenderTargetView *
void *Dx11_BackBufferDSV();		// ID3D11DepthStencilView *
void *Dx11_AcquireBackBuffer();	// ID3D11Texture2D *, AddRef'd: Release it
const float *Dx11_GetClearColor();
void Dx11_DumpTexture( void *pTexture, const char *pszFile );

#endif // DX11DEVICE_H
